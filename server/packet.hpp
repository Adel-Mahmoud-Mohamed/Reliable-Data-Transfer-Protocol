#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

struct packet {
    
    /* Header */
    uint16_t cksum;
    uint16_t len;
    uint32_t seqno;
    bool FIN;
    bool SYN;

    /* Data */
    char data[512];
};


struct ack_packet {
    uint16_t cksum;
    uint16_t len;
    uint32_t ackno;
};

#endif