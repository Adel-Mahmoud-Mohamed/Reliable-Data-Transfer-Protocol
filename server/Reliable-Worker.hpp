#ifndef RELIABLE_WORKER
#define RELIABLE_WORKER

#include <string.h>
#include <iostream>
#include <queue>
#include <chrono>
#include <fstream>

#include "packetBuilder.hpp"
#include "packet.hpp"


class Reliable_Worker {
    public:
        Reliable_Worker(unsigned int seed, double PLP);
        void handle(const struct packet *, struct sockaddr *);
        
    private:
        void recvAck(uint32_t seqno);

        void sendPacket(const char data[], uint32_t seqno, int len, bool isFIN);
        void sendPacket(struct packet*, bool=false, int=sizeof(struct packet));
                
        void sendFileInPackets(const char url[]);
        
        void reset_timer();
        void handleDubAcks();
        void handleTimeOut();

        void logInfo();

        // congestion control.
        std::deque<struct packet *> window;
        uint32_t dubACKCount = 0;
        std::chrono::steady_clock::time_point timer;
        bool fast_recovery = false;
        const uint32_t MSS = 512;
        uint32_t ssthreshold = 64000;
        uint32_t windowSize = 512;
        const uint32_t timeout = 1; // timeout in seconds.

        // prob to loss
        double PLP;

        std::ofstream log;

        // to send and receive.
        struct sockaddr *sockaddr;
        uint32_t sockfd;
        uint32_t base, nextSeqNumber;

        PacketBuilder pcktBuilder;
};

#endif 
