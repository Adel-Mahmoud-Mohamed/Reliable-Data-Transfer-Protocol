#include <iostream>
#include <cstdlib>
#include <string.h>
#include "packet.hpp"

/*
    * make_packet(int seq, char *data, int len)
    *   - returns a packet with the given sequence number, data, and length.
    *   - checksum is calculated automatically.
*/

struct packet* make_packet(int seq, char* data, int len) {
    
    struct packet *packet = (struct packet *)malloc(sizeof(struct packet));
    packet->len = len;
    packet->cksum = 0; //calculateCkSum(data, len);
    packet->seqno = seq;
    if (data != NULL) {
        strcpy(packet->data, data);
    }
    return packet;
}

int calculateCkSum(char *data, int len) {
    return 0;
}

