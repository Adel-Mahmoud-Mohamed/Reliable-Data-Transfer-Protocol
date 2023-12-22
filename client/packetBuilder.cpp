#include "packet.hpp"
#include "packetBuilder.hpp"


/*
    * PacketBuilder
    *  - build() : returns the packet and resets the builder.
    *  - initPacket(int seq) : initializes a packet with the given sequence number.
    *  - addData(const char *data, int len) : adds data to the packet.
    *  - markAsFIN() : marks the packet as a FIN packet.
    *  - markAsSYN() : marks the packet as a SYN packet.
    *  - calculateChecksum() : calculates the checksum of the packet.
*/



struct packet *PacketBuilder::build() {
    struct packet *temp = this->current_packet;
    this->current_packet = NULL;
    return temp;
}

struct ack_packet *PacketBuilder::getAckPacket(int ackno) {
    struct ack_packet *ack = (struct ack_packet *)malloc(sizeof(struct ack_packet));;
    ack->ackno = ackno;
    ack->len = 0;
    ack->cksum = 0;
    return ack;
}


PacketBuilder *PacketBuilder::initPacket(int seq) {
    if (this->current_packet != NULL) {
        free(this->current_packet);
    }
    this->current_packet = (struct packet *)malloc(sizeof(struct packet));
    this->current_packet->seqno = seq;
    this->current_packet->cksum = 0;
    this->current_packet->FIN = this->current_packet->SYN = false;
    this->current_packet->len = 0;

    return this;
}

PacketBuilder *PacketBuilder::addData(const char *data, int len) {
    strncpy(current_packet->data, data, len);
    this->current_packet->len = len;
    return this;
}

PacketBuilder *PacketBuilder::markAsFIN() {
    this->current_packet->FIN = true;
    return this;
}
PacketBuilder *PacketBuilder::markAsSYN() {
    this->current_packet->SYN = true;
    return this;
}
PacketBuilder *PacketBuilder::calculateChecksum() {
    return this;
}