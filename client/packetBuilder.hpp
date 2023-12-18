#ifndef PACKET_BUILDER
#define PACKET_BUILDER

#include <string.h>
#include <iostream>
#include <queue>
#include <chrono>

#include "packet.hpp"

class PacketBuilder {
    struct packet *current_packet = NULL;
    
    public:
        struct packet *build();
        struct ack_packet *getAckPacket(int ackno);

        PacketBuilder *addData(const char *, int len);
        PacketBuilder *initPacket(int seq);
        PacketBuilder *markAsFIN();
        PacketBuilder *markAsSYN();
        PacketBuilder *calculateChecksum();
};

#endif 
