#include "packet.hpp"
#include "packetBuilder.hpp"

struct packet *PacketBuilder::build()
{
    struct packet *temp = this->current_packet;
    this->current_packet = NULL;
    return temp;
}

// Function to craete a new Ack packet with a specific ack number
struct ack_packet *PacketBuilder::getAckPacket(int ackno)
{
    struct ack_packet *ack = (struct ack_packet *)malloc(sizeof(struct ack_packet));
    ;
    ack->ackno = ackno;
    ack->len = 0;
    ack->cksum = 0;
    return ack;
}

PacketBuilder *PacketBuilder::initPacket(int seq)
{
    if (this->current_packet != NULL)
    {
        // First we free up the current_packet pointer
        free(this->current_packet);
    }
    // And then make it pointing to a new packet with seq as a sequence number
    this->current_packet = (struct packet *)malloc(sizeof(struct packet));
    this->current_packet->seqno = seq;
    this->current_packet->cksum = 0;
    this->current_packet->FIN = this->current_packet->SYN = false;
    this->current_packet->len = 0;

    return this;
}

// Function to set the data field of the current_packet with the passed data
PacketBuilder *PacketBuilder::addDataToPacket(const char *data, int len)
{
    strncpy(current_packet->data, data, len);
    this->current_packet->len = len;
    return this;
}

// To mark the current packet as the end of the data stream
PacketBuilder *PacketBuilder::markAsFIN()
{
    this->current_packet->FIN = true;
    return this;
}

// To initiate the connection
PacketBuilder *PacketBuilder::markAsSYN()
{
    this->current_packet->SYN = true;
    return this;
}

PacketBuilder *PacketBuilder::calculateChecksum()
{
    return this;
}