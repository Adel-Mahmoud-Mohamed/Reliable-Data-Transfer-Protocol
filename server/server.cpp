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

#include "packet.hpp"
#include "Reliable-Worker.hpp"
#include "packetBuilder.hpp"

#define PORT "5000"
#define MAXBUFFERLENGTH 600

packet* make_packet(int seq, char* data, int len);

void sigchld_handler(int s) {
    // waitpid might overwrite errno, so we need to save it.
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

bool isChildAlive(int pid) {
    int status;
    pid_t return_pid = waitpid(pid, &status, WNOHANG);
    if (return_pid == -1) {
        std::cout << "Server: while checking child process status\n";
    } else if (return_pid == 0) {
        return true;
    } else if (return_pid == pid) {
        return false;
    }
    return false;
}


/**
 * @brief Get the socket address struct either in sockaddr_in(IPv4) or sockaddr_in6(IPv6) based on socket family attribute.
 * @param sa general socket address 
 */
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &((struct sockaddr_in *) sa)->sin_addr;
    }

    return &((struct sockaddr_in6 *) sa)->sin6_addr;
}


/**
 * @brief Get the socket address struct either in sockaddr_in(IPv4) or sockaddr_in6(IPv6) based on socket family attribute.
 * @param sa general socket address 
 */
uint16_t get_in_port(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return htons(((struct sockaddr_in *) sa)->sin_port);
    }

    return htons(((struct sockaddr_in6 *) sa)->sin6_port);
}


/**
 * @brief Setup localhost socket and bind it to the default/given port.
 * 
 * @return int socket file descriptor.
 */
int setup_socket(const char *port) {
    
    struct addrinfo *serverinfo, hints;
    int status, sockfd, yes = 1;

    // setting hints to zero
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_UNSPEC;   // we can use either IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // UDP used.
    hints.ai_flags = AI_PASSIVE; // to make getaddrinfo fill info of the localhost.
    hints.ai_protocol = IPPROTO_UDP;


    if ((status = getaddrinfo(NULL, port, &hints, &serverinfo) != 0)) {
        perror("Address info error\n");
        exit(1);
    }

    struct addrinfo *info = NULL;
    for (info = serverinfo; info != NULL; info = serverinfo->ai_next) {
        if ((sockfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) < 0) {
            perror("server: socket");
            continue;
        }

        // bind the socket to the port.
        if ((status = bind(sockfd, info->ai_addr, info->ai_addrlen)) < 0) {
            perror("server: bind");
            close(sockfd);
            continue;
        }
        break;
    }

    // in case no info is valid.
    if (info == NULL) {
        perror("Can't bind the socket to the port");
        exit(1);
    }

    // We don't need it anymore.
    freeaddrinfo(serverinfo);
    
    return sockfd;
}

void sendAck(int sockfd, struct ack_packet *ack, struct sockaddr *sockaddr) {
    if ((rand() % 100) < 100) {
        if (sendto(sockfd, ack, sizeof(struct ack_packet), 0, sockaddr, sizeof(*sockaddr)) == -1) {
            std::cerr << "Server: Error with sending Ack packet";
            exit(1);
        }
    }
}



// --------------------------Here's the main function----------------------------------------------------------------
int main(int argc, char *argv[]) {
    
    if (argc <= 3) {
        std::cout << "Not enough parameters\n";
        exit(1);
    }
    std::string port = std::string(argv[1]);
    unsigned int SEED = atoi(argv[2]);
    double PLP = atof(argv[3]);


    if (PLP >= 1) {
        std::cout << "Prob to loss should be less than 1\n";
        exit(1);
    }

    // To hold socket addresses for clients (connections).
    struct sockaddr_storage outside_sockets;
    
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    char buffer[MAXBUFFERLENGTH];
    char ips[INET6_ADDRSTRLEN];
    char ports[8];
    PacketBuilder packetBuilder;

    // setup local socket.
    int sockfd = setup_socket(port.c_str());
    std::cout << "Starting the server with port : " << port << '\n';
    socklen_t size = sizeof(outside_sockets);
    
    std::map<std::string, int> clients;
    int pid = -1;

    while (1) {
        
        std::cout << "Waiting for a request on " << port << " with socket: " << sockfd << '\n';
        ssize_t numOfBytesReceived = recvfrom(sockfd, buffer, MAXBUFFERLENGTH - 1, 0, (struct sockaddr *) &outside_sockets, &size);
        
        if (numOfBytesReceived < 0) {
            std::cerr << "Receiving failed\n";
            exit(1);
        }

        std::cout << "Getting " << numOfBytesReceived << " bytes\n";
        
        // To know the client.
        inet_ntop(outside_sockets.ss_family, get_in_addr((struct sockaddr *)&outside_sockets), ips, sizeof ips);
        sprintf(ports, "%u", get_in_port((struct sockaddr *)&outside_sockets));
        std::string client_id =  std::string(ips).append(":").append("").append(std::string(ports));
        std::cout << "Client : " << client_id << '\n';        
        sendAck(sockfd, packetBuilder.getAckPacket(((struct packet *) buffer)->seqno), (struct sockaddr *)&outside_sockets);

        if (clients.count(client_id) != 0 && isChildAlive(clients[client_id])) {
            continue;
        }
        else if (!(pid = fork())) {
            // child process.
            std::chrono::steady_clock::time_point time = std::chrono::steady_clock::now();
            Reliable_Worker worker(SEED, PLP);
            worker.handle((struct packet *)buffer, (struct sockaddr *) &outside_sockets);
            std::cout << "it takes " << (std::chrono::steady_clock::now() - time).count() / 1e9 << " to transfer the file\n";
            exit(0);
        } else {
            clients[client_id] = pid;
        }

    } 
    printf("Closing the server.");
    close(sockfd);
    return 0;
}