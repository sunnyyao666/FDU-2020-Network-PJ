#ifndef _CONNECTION_POOL_H_
#define _CONNECTION_POOL_H_

#include "bt_parse.h"
#include "packet.h"
#include "sha.h"
#include "spiffy.h"

typedef struct {
    bt_peer_t *peer;  // 发送方
    char sha1[SHA1_HASH_SIZE];  // 当前链接对于数据块的HASH值
    int expected_seq;  // 下一个期望收到的包序号
    int position;  // 下一个数据储存于data中的位置
    char *data;  // 目前读到的数据
    long rcv_time;  // 上次在本连接中传输/接受数据包的时间
    int overtime_count;  // 丢弃连接阈值
} download_connection_t;

typedef struct {
    download_connection_t **connections;
    int size;
} download_pool_t;

typedef struct {
    int last_acked;  // 最后被确认的包序号
    int next;  // 下一个将被发送的包序号
    int available;  // 最后一个目前可发送的包序号
    int duplicate_num;  // 重复ACK次数
    long rcv_time;  // 上次在本连接中传输/接受数据包的时间
    int overtime_count;  // 丢弃连接阈值

    bt_peer_t *peer;  // 接收方
    packet **pkts;  // 一个字节一个包，512个
} upload_connection_t;

typedef struct {
    upload_connection_t **connections;
    int size;
} upload_pool_t;

void init_download_connection(download_connection_t *connection, bt_peer_t *peer, char *sha1);

void init_upload_connection(upload_connection_t *connection, bt_peer_t *peer, packet **pkts);

void init_download_pool(download_pool_t *pool);

void init_upload_pool(upload_pool_t *pool);

// 将一个链接加入连接池中
void add_to_download_pool(download_pool_t *pool, download_connection_t *connection);

void add_to_upload_pool(upload_pool_t *pool, upload_connection_t *connection);

// 将一个链接从连接池中移除
void remove_from_download_pool(download_pool_t *pool, bt_peer_t *peer);

void remove_from_upload_pool(upload_pool_t *pool, bt_peer_t *peer);

// 在连接池中与对应peer的链接
download_connection_t *find_download_connection(download_pool_t *pool, bt_peer_t *peer);

upload_connection_t *find_upload_connection(upload_pool_t *pool, bt_peer_t *peer);

// 将当前connection发送窗口中的包全部发送
void send_DATAs(upload_connection_t *connection, int sock, struct sockaddr *to);

#endif  // _CONNECTION_POOL_H_
