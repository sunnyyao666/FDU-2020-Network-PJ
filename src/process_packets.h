#ifndef _PROCESS_PACKETS_H_
#define _PROCESS_PACKETS_H_

#include "process_chunks.h"

/**
 * 处理用户输入
 * 将chunks_to_get制作为WHOHAS包，并向每个对等方发出
 */
void process_download(int sock);

void process_PACKET(int sock, packet *pkt, struct sockaddr_in *from,socklen_t fromlen);

/**
 * 处理WHOHAS包
 * 检查自己是否有被需求的数据块，有则返回对应的IHAVE包
 */
packet *process_WHOHAS(packet *pkt);

/**
 * 处理IHAVE包
 * 尝试建立与对等方的下载链接，若成功则返回对应的GET包
 */
packet *process_IHAVE(packet *pkt, bt_peer_t *peer);

/**
 * 处理GET包
 * 尝试建立与对等方的上传链接，若成功则开始传送DATA包
 */
void process_GET(int sock, packet *pkt, bt_peer_t *peer);

/**
 * 处理DATA包
 * 检查DATA包的seq_num并返回相应的ack_num
 * 检查是否接受完整个chunk
 */
void process_DATA(int sock, packet *pkt, bt_peer_t *peer);

/**
 * 处理ACK包
 * 检查ACK包的ack_num并相应处理
 */
void process_ACK(int sock, packet *pkt, bt_peer_t *peer);

/**
 * 处理定时器
 * 检查上传池和下载池中每个连接
 * 若超时则重发
 * 超过阈值则丢弃连接
 */
void handle_timeout();

bt_peer_t *find_peer(struct sockaddr_in *addr);

#endif  // _PROCESS_PACKETS_H_