#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <map>

#include "packet.hpp"
#include "packetBuilder.hpp"

#define MAXBUFFERLENGTH 5000

using namespace std;

packet *make_packet(int seq, char *data, int len);

void sendAck(int sockfd, struct ack_packet *ack, struct sockaddr *sockaddr)
{

    if (sendto(sockfd, ack, sizeof(struct ack_packet), 0, sockaddr, sizeof(*sockaddr)) == -1)
    {
        std::cerr << "Server: Error with sending Ack packet";
        exit(1);
    }
}

void sendPacket(int sockfd, struct packet *pack, struct sockaddr *sockaddr)
{
    if ((rand() % 100) < 100)
    {
        if (sendto(sockfd, pack, sizeof(struct packet), 0, sockaddr, sizeof(*sockaddr)) == -1)
        {
            std::cerr << "Server: Error with sending Ack packet";
            exit(1);
        }
    }
}

vector<string> readClientInFile()
{
    string fileName = "client.in";
    vector<string> commands;
    string line;
    string content = "";
    ifstream myfile;
    myfile.open(fileName);
    while (getline(myfile, line))
    {
        commands.push_back(line);
    }
    return commands;
}

int main(int argc, char *argv[])
{
    int sockfd, status;
    struct addrinfo *servinfo, hints, *root;

    char buffer[MAXBUFFERLENGTH];
    struct ack_packet *ack;
    ssize_t numOfBytesReceived = -1;

    std::map<uint32_t, std::pair<std::string, u_int16_t>> cache; // buffer for out-of-order packets.
    PacketBuilder packetBuilder;

    vector<string> args = readClientInFile();
    if (args.size() != 3)
    {
        printf("Insufficient number of args (must be 3)!");
        exit(1);
    }
    const char *serverIP = args[0].c_str();
    const char *portNumber = args[1].c_str();
    const char *fileName = args[2].c_str();

    unsigned int SEED = atoi(args[1].c_str());
    double PLP = atof(args[2].c_str());

    // setting hints to zero
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_UNSPEC;    // we can use either IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // UDP used.
    hints.ai_protocol = IPPROTO_UDP;

    if ((status = getaddrinfo(serverIP, portNumber, &hints, &root) != 0))
    {
        perror("Address info error\n");
        exit(1);
    }

    // loop through all the results and make a socket
    for (servinfo = root; servinfo != NULL; servinfo = root->ai_next)
    {
        if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) < 0)
        {
            perror("server: socket");
            continue;
        }
        break;
    }

    // in case no info is valid.
    if (servinfo == NULL)
    {
        perror("Can't bind the socket to the port");
        exit(1);
    }

    std::cout << serverIP << " " << portNumber << " " << fileName << '\n';
    struct packet *pckt = packetBuilder.initPacket(0)->addData(fileName, strlen(fileName))->build();

    // setting timeout for the socket.
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 1;
    socket_timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof socket_timeout);

    while (numOfBytesReceived <= 0)
    {
        std::cout << "trying to send the requst packet!!\n";
        sendPacket(sockfd, pckt, servinfo->ai_addr);

        // receive the ack.
        std::cout << "trying to receive an ack\n\n";
        numOfBytesReceived = recvfrom(sockfd, buffer, MAXBUFFERLENGTH, 0, servinfo->ai_addr, &servinfo->ai_addrlen);
        if (numOfBytesReceived > 0)
        {
            ack = (struct ack_packet *)buffer;
        }
    }

    std::cout << "LOG: Client: Got ack and getting ready to receive the file.\n";
    uint32_t expectedSeq = 1;            // expected sequence number.
    ack = packetBuilder.getAckPacket(0); // ack for the first packet.

    std::ofstream out; // output file stream.

    int noResponse = 15; // To indicate that there's no response from the server

    bool fileOpened = false;

    std::pair<uint32_t, bool> isFin = std::make_pair(-1, false); // isFin.first is the seqno of the fin packet, isFin.second is true if we got the fin packet.

    while (noResponse >= 0 && (!isFin.second || isFin.first > expectedSeq))
    {
        std::cout << "Waiting for the next packet!!\n";
        numOfBytesReceived = recvfrom(sockfd, buffer, MAXBUFFERLENGTH, 0, servinfo->ai_addr, &servinfo->ai_addrlen);
        if (numOfBytesReceived <= 0)
        {
            noResponse--;
            continue;
        }
        else
        {
            if (!fileOpened)
            {
                fileOpened = true;
                out.open(fileName, ifstream::binary); // Open the file back to open in it
            }
            noResponse = 15; // Set it back again to it's old value
        }

        pckt = (struct packet *)buffer;

        // check if it's a random ack.
        if (pckt->len == 0)
        {
            continue;
        }
        // out-of-order, buffer the incoming packet, until we got the expected one.
        else if (pckt->seqno > expectedSeq)
        {

            std::cout << "Client: packets are out of order!!! expecting " << expectedSeq << " but got " << pckt->seqno << '\n';
            cache[pckt->seqno] = std::make_pair(std::string(pckt->data, pckt->len), pckt->len);
            if (pckt->FIN)
            {
                isFin = std::make_pair(pckt->seqno, true);
            }
        }
        else if (pckt->seqno == expectedSeq)
        { // got the expected packet.

            free(ack);
            std::cout << "Got " << pckt->seqno << '\n';
            out.write(pckt->data, pckt->len);

            expectedSeq += pckt->len;
            uint16_t prev_len = pckt->len;

            if (pckt->FIN)
            {
                isFin = std::make_pair(pckt->seqno, true);
            }

            while (cache.count(expectedSeq) != 0)
            {                                                                       // check if we have the next packet in the buffer.
                std::pair<std::string, uint16_t> cachedPacket = cache[expectedSeq]; // get the packet from the buffer.
                cache.erase(expectedSeq);                                           // remove the packet from the buffer.
                out.write(cachedPacket.first.c_str(), cachedPacket.second);
                std::cout << "Buffer is used : from : " << expectedSeq << " to : " << expectedSeq + cachedPacket.second << '\n';
                expectedSeq += cachedPacket.second;
                prev_len = cachedPacket.second;
            }
            ack = packetBuilder.getAckPacket(expectedSeq - prev_len);
        }
        std::cout << "Going to send ack: " << ack->ackno << "\n\n";

        // sending the ack.
        sendAck(sockfd, ack, servinfo->ai_addr);
    }
    if (!fileOpened)
    {
        // Then the file is not found
        cout << "File is not found at the server side "
             << "\n";
    }
    std::cout << "Done."
              << "\n";
    free(ack);
    freeaddrinfo(root);
    close(sockfd);
    return 0;
}
