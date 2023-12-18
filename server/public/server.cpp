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

#include "packet.hpp"
#include "Reliable-Worker.hpp"

#define PORT "5000"
#define MAXBUFFERLENGTH 600

packet* make_packet(int seq, char* data, int len);

void sigchld_handler(int s) {
    // waitpid might overwrite errno, so we need to save it.
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
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



int main(int argc, char *argv[]) {

    std::string port = (argc > 1 ? std::string(argv[1]) : PORT);

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

    // setup local socket.
    int sockfd = setup_socket(port.c_str());

    std::cout << "Starting the server with port : " << port << '\n';
    socklen_t size = sizeof(outside_sockets);
    while (1) {
        std::cout << "Waiting for a request on " << port << " with socket: " << sockfd << '\n';
        ssize_t numOfBytesReceived = recvfrom(sockfd, buffer, MAXBUFFERLENGTH - 1, 0, (struct sockaddr *) &outside_sockets, &size);
        if (numOfBytesReceived < 0) {
            std::cerr << "Receiving failed\n";
        }
        std::cout << "Getting " << numOfBytesReceived << " bytes\n";

        if (!fork()) {
            // child process.
            Reliable_Worker worker;
            worker.handle((struct packet *)buffer, (struct sockaddr *) &outside_sockets);
            exit(0);
        }

    } 

    printf("Closing the server.");
    close(sockfd);
    return 0;
}