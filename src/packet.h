#ifndef _PACKET_H_
#define _PACKET_H_

#include <inttypes.h>

#define MAX_SIZE 1500  // UDP数据包最大为1500字节
#define HEADER_SIZE 16  // 数据包头部为16字节
#define DATA_SIZE (MAX_SIZE-HEADER_SIZE)
#define MAGIC 15441
#define VERSION 1

/**
 * 数据包头部格式：
 * 1. 魔数 (2字节) (应为15441)
 * 2. 版本 (1字节) (应为1)
 * 3. 数据包类型 (1字节) (0-5)
 * 4. 头部长度 (2字节)
 * 5. 数据包总长度 (2字节)
 * 6. 序列号 (4字节)
 * 7. ACK号 (4字节) (用于可靠数据传输)
 */
typedef enum {
    WHOHAS = 0,
    IHAVE = 1,
    GET = 2,
    DATA = 3,
    ACK = 4,
    DENIED = 5
} packet_type;

typedef struct {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t header_len;
    uint16_t packet_len;
    uint32_t seq_num;
    uint32_t ack_num;
} packet_header;

typedef struct {
    packet_header header;
    uint8_t data[DATA_SIZE];
} packet;

void init_packet(packet *pkt, uint8_t type, uint16_t packet_len, uint32_t seq_num, uint32_t ack_num, char *data);

/**
 * WHOHAS 与 IHAVE:
 * 请求的数据块个数 (1字节), 空白填充 (3字节)
 * 请求的数据块的HASH值 (每个20字节)
 * seq_num = ack_num = 0
 */
packet *make_WHOHAS(uint16_t data_len, char *data);

packet *make_IHAVE(uint16_t data_len, char *data);

/**
 * GET:
 * 数据块的HASH值 (20字节)
 * seq_num = ack_num = 0
 */
packet *make_GET(char *data);

/**
 * DATA:
 * 具体文件内容
 * seq_num有意义，ack_num = 0
 */
packet *make_DATA(uint32_t seq_num, uint16_t data_len, char *data);

/**
 * ACK:
 * 无载荷
 * seq_num = 0，ack_num有意义
 */
packet *make_ACK(uint32_t ack_num);

packet_type get_packet_type(uint8_t type);

// 数据包头部字节顺序主机与网络的互换
void ntoh_packet(packet *pkt);

void hton_packet(packet *pkt);

// 检查数据包是否有效
int is_packet_valid(packet *pkt);

#endif  // _PACKET_H_
