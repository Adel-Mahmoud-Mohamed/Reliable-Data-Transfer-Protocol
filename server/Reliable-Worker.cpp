#include <stdio.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>

#include "packet.hpp"
#include "Reliable-Worker.hpp"

#define MAXBUFFERLENGTH 600

packet *make_packet(int seq, const char *data, int len);

void sigchld_handler(int s);

/**
 * @brief Get the socket address struct either in sockaddr_in(IPv4) or sockaddr_in6(IPv6) based on socket family attribute.
 * @param sa general socket address
 */
void *get_in_addr(struct sockaddr *sa);

/**
 * @brief Get the socket address struct either in sockaddr_in(IPv4) or sockaddr_in6(IPv6) based on socket family attribute.
 * @param sa general socket address
 */
uint16_t get_in_port(struct sockaddr *sa);

int getFileSize(const std::string &filename)
{
    FILE *p_file = NULL;
    p_file = fopen(filename.c_str(), "rb");
    if (p_file == NULL)
        return -1;

    fseek(p_file, 0, SEEK_END);
    int size = ftell(p_file);
    fclose(p_file);
    return size;
}

/**
 * @brief Setup localhost socket and bind it to the default/given port.
 *
 * @return int socket file descriptor.
 */
int setup_socket(const char *host, const char *port)
{

    struct addrinfo *serverinfo, hints;
    int status, sockfd, yes = 1;

    // setting hints to zero
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_UNSPEC;     // we can use either IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;  // Specify that it's a UDP socket.
    hints.ai_protocol = IPPROTO_UDP; // Specify that it uses a UDP protocol

    if ((status = getaddrinfo(host, port, &hints, &serverinfo) != 0))
    {
        perror("Address info error\n");
        exit(1);
    }

    struct addrinfo *info = NULL;
    for (info = serverinfo; info != NULL; info = serverinfo->ai_next)
    {
        if ((sockfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) < 0)
        {
            perror("server: socket");
            continue;
        }
        break;
    }

    // in case no info is valid.
    if (info == NULL)
    {
        perror("Can't bind the socket to the port");
        exit(1);
    }

    // We don't need it anymore.
    freeaddrinfo(serverinfo);

    // Here means that we've successfully created our socket and let's return its file descriptor
    return sockfd;
}

// Function to reset the timer such that timer = currentTime + timeout
// It specifies the point at which the timer shall expire
void Reliable_Worker::reset_timer()
{
    this->timer = std::chrono::steady_clock::now() + std::chrono::seconds(this->timeout);
}

void Reliable_Worker::logInfo()
{
    log << this->windowSize << '\n';
}

// Function to handle the duplicate acks recived and if they're three dup then we'll
// Go to the faseRecovery mode
void Reliable_Worker::handleDubAcks()
{
    // Increment the count of duplicate acknowledgments
    this->dubACKCount++;

    // Check if the count of duplicate acknowledgments is greater than or equal to 3
    if (this->dubACKCount >= 3)
    {
        // Check if the sender is already in fast recovery mode
        if (this->fast_recovery)
        {
            // If in fast recovery mode, increase the window size by MSS (Maximum Segment Size)
            this->windowSize += this->MSS;
        }
        else
        {
            // If not in fast recovery mode
            // Set slow-start threshold to half of the current window size
            this->ssthreshold = this->windowSize / 2;

            // Set the window size to the slow-start threshold plus 3 times MSS
            this->windowSize = this->ssthreshold + 3 * this->MSS;

            // Enable fast recovery mode
            this->fast_recovery = true;

            // If the window is not empty, retransmit the first packet in the window
            if (!this->window.empty())
            {
                this->sendPacket(*this->window.begin());
            }
        }
    }
}

// Function to handle the timeout event
void Reliable_Worker::handleTimeOut()
{
    // Reset the timer to the current time plus the timeout duration
    reset_timer();

    // If the window is not empty, retransmit the first packet in the window
    if (!this->window.empty())
    {
        // Re-transmit the first unAcked packet in the window
        this->sendPacket(*this->window.begin());
    }

    // Reset variables related to congestion control and recovery
    this->fast_recovery = false;
    this->dubACKCount = 0;
    this->ssthreshold = this->windowSize / 2;
    this->windowSize = this->MSS;
}

void Reliable_Worker::sendPacket(const char data[], uint32_t seqno, int len, bool isFIN)
{
    PacketBuilder *packetBuilder = this->pcktBuilder.initPacket(seqno)->addData(data, len);
    if (isFIN)
    {
        packetBuilder->markAsFIN();
    }
    this->sendPacket(packetBuilder->build(), true);
}

void Reliable_Worker::sendPacket(struct packet *packet, bool addToWindow, int len)
{
    // If addToWindow is true, add the packet to the window and start the timer if the window is empty
    if (addToWindow)
    {
        if (this->window.size() == 0)
        {
            // Start the timer
            reset_timer();
        }
        this->window.push_back(packet);
    }

    // Simulate packet loss based on the Packet Loss Probability (PLP)
    // If the probability of sending the packet is greater than the randomly generated number we'll send it
    if ((rand() % 100) < (100 - this->PLP))
    {
        // If no packet loss, send the packet using sendto
        if (sendto(sockfd, packet, len, 0, sockaddr, sizeof(*sockaddr)) == -1)
        {
            // Error handling in case of failure to send the packet
            std::cerr << "Server: Error with sending the packet";
            exit(1);
        }
    }
}

void Reliable_Worker::recvAck(uint32_t seqno)
{
    struct ack_packet ack_packet;
    int numOfBytesReceived = 0;
    struct sockaddr_storage outside_sockets;
    socklen_t size = sizeof(outside_sockets);

    bool time_out = true;

    if (!this->window.empty())
    {
        // that should be followed.
        if (seqno != this->base)
            exit(1);
    }

    std::cout << "Trying to receive an ack for " << seqno << " with window size : " << this->windowSize << " and threshold : " << this->ssthreshold << '\n';
    std::cout << "Current not-acked packets are : " << this->window.size() << '\n';
    while (std::chrono::steady_clock::now() < this->timer)
    {
        numOfBytesReceived = recvfrom(sockfd, &ack_packet, sizeof(struct ack_packet), 0, (struct sockaddr *)&outside_sockets, &size);
        if (numOfBytesReceived > 0)
        {
            time_out = false;
            break;
        }
    }

    if (time_out)
    {
        std::cout << "Timeout!!, let's resend missing packet again" << '\n';
        handleTimeOut();
        std::cout << "new window size : " << this->windowSize << " new threshold : " << this->ssthreshold << "\n\n";
        logInfo();
        return;
    }

    // Here means that no timeout has occured and we've recieved an ack
    std::cout << numOfBytesReceived << " with ack: " << ack_packet.ackno << " and was lookign for: " << seqno << '\n';

    // to handle wrapping around.
    if ((uint32_t)(ack_packet.ackno - seqno) >= 0x80000000)
    {
        // Means that we've got a duplicate ack let's handle it
        this->handleDubAcks();
    }
    else
    {
        // It's not a duplicate ack
        this->dubACKCount = 0;
        // Then we need to check which mood we're in
        if (this->fast_recovery)
        {
            // This means that we're in the fast recovery mode
            this->windowSize = this->ssthreshold;
            this->fast_recovery = false;
        }
        else
        {

            if (this->windowSize >= this->ssthreshold)
            {
                // This means that we're in the congestion avoidence mode
                this->windowSize += this->MSS * (this->MSS / (double)this->windowSize);
            }
            else
            {
                // If not in the CA nor the fast recovery then we must be in the slow start mode
                this->windowSize += this->MSS;
            }
        }

        // Let's adjust our window
        while (!this->window.empty() && (uint32_t)(ack_packet.ackno - this->window.front()->seqno) < 0x80000000)
        {
            this->base = this->window.front()->seqno + this->window.front()->len;
            free(this->window.front());
            this->window.pop_front();
        }
        this->reset_timer();
    }

    logInfo();
    std::cout << '\n';
}

void Reliable_Worker::sendFileInPackets(const char url[])
{

    // read the file as a stream of binary data.
    std::ifstream file(url, std::ifstream::binary);
    int len = getFileSize(std::string(url));

    // check the existence of the file.
    if (file.fail())
    {
        std::cout << "no such a file\n";
        exit(1);
    }

    const unsigned int BUFFER_SIZE = this->MSS;
    char buff[BUFFER_SIZE] = {0};

    while (file)
    {
        while ((this->base + this->windowSize > this->nextSeqNumber && this->base + this->windowSize - this->nextSeqNumber >= BUFFER_SIZE) ||
               (len <= BUFFER_SIZE && len <= this->base + this->windowSize - this->nextSeqNumber))
        {
            // determine the payload size
            int payloadSize = std::min(BUFFER_SIZE, this->base + this->windowSize - this->nextSeqNumber);

            file.read(buff, payloadSize);
            size_t count = file.gcount();

            // no bytes left
            if (!count)
                break;
            len -= count;

            // send the packet.
            std::cout << "Sending the packet " << this->nextSeqNumber << " with " << count << " bytes" << '\n';
            this->sendPacket(buff, this->nextSeqNumber, count, len == 0);
            this->nextSeqNumber += count;
        }
        this->recvAck(this->base);
    }
    file.close();
}

// This is the constructor for our class it initializes the log file named data2.txt for logging out info
Reliable_Worker::Reliable_Worker(unsigned int seed, double PLP) : log("data2.txt", std::ios::out)
{
    this->base = this->nextSeqNumber = 1;
    this->fast_recovery = false;
    this->PLP = PLP * 100;
    srand(seed);
}

void Reliable_Worker::handle(const struct packet *packet, struct sockaddr *sockaddr)
{

    // getting host and port.
    char ips[INET6_ADDRSTRLEN];
    const char *host = inet_ntop(sockaddr->sa_family, get_in_addr(sockaddr), ips, sizeof ips);
    char port[16];
    sprintf(port, "%u", get_in_port(sockaddr));
    std::cout << "LOG: "
              << "GOT " << packet->data << packet->seqno << packet->len << '\n';

    this->sockfd = setup_socket(host, port);
    this->sockaddr = sockaddr;

    // setting timeout for the socket.
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = 10;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof socket_timeout);

    this->sendFileInPackets(packet->data);

    while (!this->window.empty())
        this->recvAck(this->base);
    this->log.close();
    std::cout << "Sending the file is completed!!!!\n";
    return;
}