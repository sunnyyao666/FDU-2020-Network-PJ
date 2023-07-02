#ifndef _PROCESS_CHUNKS_H_
#define _PROCESS_CHUNKS_H_

#include "bt_parse.h"
#include "sha.h"
#include "my_list.h"
#include "chunk.h"
#include "packet.h"

#define CHUNK_SIZE 512

#define MAX_CHUNKS_NUM ((MAX_SIZE - HEADER_SIZE - 4)/SHA1_HASH_SIZE)  // 一个WHOHAS能请求的最多数据块数量

// 数据块状态
typedef enum chunk_state {
    NOT_DOWNLOADED = 0,
    DOWNLOADING = 1,
    DONE = 2
} chunk_state;

typedef struct {
    int id;
    char sha1[SHA1_HASH_SIZE];  // 数据块的HASH值(2进制，20字节)
} chunk_t;

typedef struct {
    int chunks_num;
    chunk_t *chunks;
    char **chunks_data;  // 每个数据块的具体文件内容
    bt_peer_t **peers;  // 每个数据块的提供方
    chunk_state *statuses;  // 每个数据块当前的状态
} chunks_to_get_t;

void init_master_chunks();

void init_chunks_IHAVE();

// 得到来自用户的输入后，初始化需要下载的数据块信息
void init_chunks_to_get();

/**
 * 收到IHAVE包后调用
 * 将列表中的chunks的提供方更新为peer
 * 返回一个即将被下载的数据块HASH值
 */
char *update_provider(my_list *chunk_sha1s, bt_peer_t *peer);

// 从IHAVE包中读取数据块的HASH值，返回HASH值的列表
my_list *get_chunks_from_packet(packet *pkt);

// 为WHOHAS包做准备，将需要下载的数据块HASH值制作为data
void make_chunks_to_packet(char *data, int start, int chunks_num, chunk_t *chunks);

// 将指定数据块的文件内容打包为512个DATA包并返回
packet **make_chunk_data_to_packets(char *sha1);

/**
 * 接受完一整个chunk的文件内容后调用
 * 先检查HASH值是否一致
 * 若一致则保存文件内容并改变该块状态为DONE
 */
void save_data(char *sha1, char *data);

// 从chunks中找特定数据块，若无则返回NULL
chunk_t *find_chunk(my_list *chunks, char *sha1);

#endif  // _PROCESS_CHUNKS_H_
