#include <iostream>
#include <cstdlib>
#include <string.h>
#include "packet.hpp"


struct packet* make_packet(int seq, const char* data, int len) {
    
    struct packet *packet = (struct packet *)malloc(sizeof(struct packet));
    packet->len = len;
    packet->cksum = 0; //calculateCkSum(data, len);
    packet->seqno = seq;
    if (data != NULL) {
        strncpy(packet->data, data, len);
    }
    return packet;
}

int calculateCkSum(char *data, int len) {
    return 0;
}

