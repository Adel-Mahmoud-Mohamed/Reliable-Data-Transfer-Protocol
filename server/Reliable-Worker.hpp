#ifndef RELIABLE_WORKER
#define RELIABLE_WORKER

#include <string.h>
#include <iostream>
#include <queue>
#include <chrono>
#include <fstream>

#include "packetBuilder.hpp"
#include "packet.hpp"

class Reliable_Worker
{
public:
    Reliable_Worker(unsigned int seed, double PLP);
    void handle(const struct packet *, struct sockaddr *);

private:
    void recvAck(uint32_t seqno);

    void sendPacket(const char data[], uint32_t seqno, int len, bool isFIN);
    void sendPacket(struct packet *, bool = false, int = sizeof(struct packet));

    void sendFileInPackets(const char url[]);

    void reset_timer();
    void handleDubAcks();
    void handleTimeOut();

    void logInfo();

    // congestion control.
    std::deque<struct packet *> window;          // The sending window as a queue of packets
    uint32_t dubACKCount = 0;                    // Counter for duplicate acks
    std::chrono::steady_clock::time_point timer; // For tracking the timeouts
    bool fast_recovery = false;                  // Boolean flag indicating whether the sender is in fast recovery mode.
    const uint32_t MSS = 512;                    // Boolean flag indicating whether the sender is in fast recovery mode (initially = 512 bytes)
    uint32_t ssthreshold = 64000;                // The slow start threshold (initially 64 k)
    uint32_t windowSize = 512;                   // The current size of cwnd
    const uint32_t timeout = 1;                  // timeout in seconds.

    // prob to loss
    double PLP;

    std::ofstream log; // The log file outputFileStream

    // to send and receive.
    struct sockaddr *sockaddr; // Pointer to a sockaddr structure representing the server's address.
    uint32_t sockfd;           // The file descriptor of the socket number
    uint32_t base;             // Sequence number of the oldest unacknowledged packet.
    uint32_t nextSeqNumber;    // Sequence number for the next packet to be sent.

    PacketBuilder pcktBuilder; // An instance of the packetBuilder class
};

#endif
