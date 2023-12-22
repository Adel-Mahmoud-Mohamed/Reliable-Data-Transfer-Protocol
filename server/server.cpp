#include <stdio.h>
#include <vector>
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
#include <map>

// Include our headers
#include "packet.hpp"
#include "Reliable-Worker.hpp"
#include "packetBuilder.hpp"

#define MAXBUFFERLENGTH 600

using namespace std;

// For reaping off the terminated child process such that it doesn't become a zombie process
void sigchld_handler(int s)
{
    // waitpid might overwrite errno, so we need to save it.
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

bool isChildAlive(int pid)
{
    int status;
    pid_t return_pid = waitpid(pid, &status, WNOHANG);
    if (return_pid == -1)
    {
        cout << "Server: Error occured while checking child process status\n";
    }
    else if (return_pid == 0) // Then our process's still alive
    {
        return true;
    }
    else if (return_pid == pid) // Then our child process's terminated and the return_pid is now equal to the parent if
    {
        return false;
    }
    return false;
}

// Gets the ip address based on the family type of the socket
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &((struct sockaddr_in *)sa)->sin_addr;
    }

    return &((struct sockaddr_in6 *)sa)->sin6_addr;
}

// Get the port number as a network sequence accoriding to the ip family
uint16_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return htons(((struct sockaddr_in *)sa)->sin_port);
    }

    return htons(((struct sockaddr_in6 *)sa)->sin6_port);
}

// Setup the local socket and bind it to the given port
int setup_socket(const char *port)
{

    struct addrinfo *serverinfo, hints;
    int status, sockfd;

    // setting hints to zero
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_UNSPEC;     // we can use either IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;  // UDP used.
    hints.ai_flags = AI_PASSIVE;     // to make getaddrinfo fill info of the localhost.
    hints.ai_protocol = IPPROTO_UDP; // to use the UDP protocol in the transmission

    if ((status = getaddrinfo(NULL, port, &hints, &serverinfo) != 0))
    {
        perror("Error occured while getting the address info\n");
        exit(1);
    }

    struct addrinfo *info = NULL;
    for (info = serverinfo; info != NULL; info = serverinfo->ai_next)
    {
        if ((sockfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) < 0)
        {
            perror("Error occured while creating the socket");
            continue;
        }

        // bind the socket to the port.
        if ((status = bind(sockfd, info->ai_addr, info->ai_addrlen)) < 0)
        {
            perror("Error occured while binding the port");
            close(sockfd);
            continue;
        }
        break;
    }

    if (info == NULL)
    {
        perror("Can't bind the socket to the given port");
        exit(1);
    }

    freeaddrinfo(serverinfo);

    return sockfd;
}

void sendAck(int sockfd, struct ack_packet *ack, struct sockaddr *sockaddr)
{
    if ((rand() % 100) < 100)
    {
        if (sendto(sockfd, ack, sizeof(struct ack_packet), 0, sockaddr, sizeof(*sockaddr)) == -1)
        {
            cerr << "Server: Error while sending the Ack packet";
            exit(1);
        }
        cout << "Ack has been sent successfully"
             << "\n";
    }
}

vector<string> readServerInFile()
{
    string fileName = "server.in";
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

// --------------------------------------------Here's the main function----------------------------------------------------------------
int main(int argc, char *argv[])
{
    vector<string> args = readServerInFile();

    if (args.size() < 3)
    {
        cout << "Not enough parameters found in server.in file !\n";
        exit(1);
    }

    string port = args[0];
    unsigned int SEED = atoi(args[1].c_str());
    double PLP = atof(args[2].c_str());

    if (PLP >= 1)
    {
        cout << "Prob to loss should be less than 1\n";
        exit(1);
    }

    // To hold socket addresses for clients (connections).
    struct sockaddr_storage outside_sockets;

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    char buffer[MAXBUFFERLENGTH];
    char ips[INET6_ADDRSTRLEN];
    char ports[8];
    PacketBuilder packetBuilder;

    // setup local socket.
    int sockfd = setup_socket(port.c_str());

    cout << "Starting the server with port : " << port << '\n';

    socklen_t size = sizeof(outside_sockets);

    map<string, int> clients;

    int pid = -1;

    // Main server loop
    while (1)
    {
        // Print a message indicating that the server is waiting for a request
        cout << "Waiting to be requested at port: " << port << " , and with socket: " << sockfd << "..." << '\n';

        // Receive data from the client
        ssize_t bytesRecved = recvfrom(sockfd, buffer, MAXBUFFERLENGTH - 1, 0, (struct sockaddr *)&outside_sockets, &size);

        // Check for errors in receiving data
        if (bytesRecved < 0)
        {
            cerr << "Receiving failed\n";
            exit(1);
        }

        // Print the number of bytes received
        cout << "Got " << bytesRecved << " Bytes\n";

        // Get client info
        inet_ntop(outside_sockets.ss_family, get_in_addr((struct sockaddr *)&outside_sockets), ips, sizeof ips);
        sprintf(ports, "%u", get_in_port((struct sockaddr *)&outside_sockets));
        string client_id = string(ips).append(":").append("").append(string(ports));
        cout << "The client id is: " << client_id << '\n';

        // Send acknowledgment to the client
        sendAck(sockfd, packetBuilder.getAckPacket(((struct packet *)buffer)->seqno), (struct sockaddr *)&outside_sockets);

        // Check if a child process for the client is already running
        if (clients.count(client_id) != 0 && isChildAlive(clients[client_id]))
        {
            // If yes, continue to the next iteration of the loop
            continue;
        }
        else if (!(pid = fork()))
        {
            // Child process
            chrono::steady_clock::time_point startTime = chrono::steady_clock::now();

            // Create a Reliable_Worker and handle the request
            Reliable_Worker worker(SEED, PLP);
            worker.handleRequest((struct packet *)buffer, (struct sockaddr *)&outside_sockets);

            chrono::steady_clock::time_point endTime = chrono::steady_clock::now();

            // Print the time taken to transfer the file
            cout << "It took " << (endTime - startTime).count() / 1e9 << " seconds to transfer the file\n";

            // Exit the child process
            exit(0);
        }
        else
        {
            // Parent process
            // Store the child process ID in the map
            clients[client_id] = pid;
        }
    }

    printf("Closing the server.");

    // Close the socket
    close(sockfd);

    // Return 0 to indicate successful execution
    return 0;
}