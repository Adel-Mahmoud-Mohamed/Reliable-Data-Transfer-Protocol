#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

// Data-only packets 
struct packet {
    
    /* Header */
    uint16_t cksum;
    uint16_t len;
    uint32_t seqno;
    bool FIN; // A boolean flag indicating whether this packet represents the end of a data stream.
    bool SYN; // A boolean flag to initiate the connection between the two end points
 
    /* payload */
    char data[512];
};

// Ack-only packets are 8 bytes in length
struct ack_packet {
    uint16_t cksum;
    uint16_t len;
    uint32_t ackno;
};

#endif