#include "packet.h"
#include "sha.h"
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

void init_packet(packet *pkt, uint8_t type, uint16_t data_len, uint32_t seq_num, uint32_t ack_num, char *data) {
    packet_header *header = &pkt->header;
    header->magic = MAGIC;
    header->version = VERSION;
    header->type = type;
    header->header_len = HEADER_SIZE;
    header->packet_len = HEADER_SIZE + data_len;
    header->seq_num = seq_num;
    header->ack_num = ack_num;
    if (data != NULL) memcpy(pkt->data, data, (size_t)(data_len));
    hton_packet(pkt);
}

packet *make_WHOHAS(uint16_t data_len, char *data) {
    packet *pkt = (packet *) malloc(HEADER_SIZE + data_len);
    init_packet(pkt, WHOHAS, data_len, 0, 0, data);
    return pkt;
}

packet *make_IHAVE(uint16_t data_len, char *data) {
    packet *pkt = (packet *) malloc(HEADER_SIZE + data_len);
    init_packet(pkt, IHAVE, data_len, 0, 0, data);
    return pkt;
}

packet *make_GET(char *data) {
    packet *pkt = (packet *) malloc(HEADER_SIZE + SHA1_HASH_SIZE);
    init_packet(pkt, GET, (uint16_t)(SHA1_HASH_SIZE), 0, 0, data);
    return pkt;
}

packet *make_DATA(uint32_t seq_num, uint16_t data_len, char *data) {
    packet *pkt = (packet *) malloc(HEADER_SIZE + data_len);
    init_packet(pkt, DATA, (uint16_t)(data_len), seq_num, 0, data);
    return pkt;
}

packet *make_ACK(uint32_t ack_num) {
    packet *pkt = (packet *) malloc(sizeof(packet));
    init_packet(pkt, ACK, 0, 0, ack_num, NULL);
    return pkt;
}

packet_type get_packet_type(uint8_t type) {
    switch (type) {
        case 0:
            return WHOHAS;
        case 1:
            return IHAVE;
        case 2:
            return GET;
        case 3:
            return DATA;
        case 4:
            return ACK;
        case 5:
            return DENIED;
        default:
            break;
    }
}

void ntoh_packet(packet *pkt) {
    packet_header *header = &pkt->header;
    header->magic = ntohs(header->magic);
    header->header_len = ntohs(header->header_len);
    header->packet_len = ntohs(header->packet_len);
    header->seq_num = ntohl(header->seq_num);
    header->ack_num = ntohl(header->ack_num);
}

void hton_packet(packet *pkt) {
    packet_header *header = &pkt->header;
    header->magic = htons(header->magic);
    header->header_len = htons(header->header_len);
    header->packet_len = htons(header->packet_len);
    header->seq_num = htonl(header->seq_num);
    header->ack_num = htonl(header->ack_num);
}

int is_packet_valid(packet *pkt) {
    packet_header *header = &pkt->header;
    if (header->magic != MAGIC || header->version != VERSION || header->type < WHOHAS || header->type > DENIED)
        return 0;
    packet_type type = get_packet_type(header->type);
    if ((type == DATA && (header->seq_num == 0 || header->ack_num != 0)) || (type == ACK && header->seq_num != 0) ||
        (type != DATA && type != ACK && (header->seq_num != 0 || header->ack_num != 0)))
        return 0;
    return 1;
}
