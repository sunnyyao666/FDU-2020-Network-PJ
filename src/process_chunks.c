#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "process_chunks.h"
#include "bt_parse.h"
#include "packet.h"
#include "my_list.h"

extern bt_config_t config;
extern my_list *master_chunks;
extern chunks_to_get_t chunks_to_get;
extern my_list *chunks_IHAVE;
char master_chunk_file[256];

void init_master_chunks() {
    FILE *fd = fopen(config.chunk_file, "r");
    char sha1[256], line[256];
    fgets(line, 256, fd);
    sscanf(line, "%s %s", sha1, master_chunk_file);

    fgets(line, 256, fd);  // 第二行为Chunks:，无效
    while (fgets(line, 256, fd)) {
        int id;
        if (sscanf(line, "%d %s", &id, sha1) != 2) continue;
        chunk_t *chunk = (chunk_t *) malloc(sizeof(chunk_t));
        chunk->id = id;
        hex2binary(sha1, SHA1_HASH_SIZE * 2, (uint8_t *) chunk->sha1);
        push(master_chunks, chunk);
    }
    fclose(fd);
}

void init_chunks_IHAVE() {
    FILE *fd = fopen(config.has_chunk_file, "r");
    char line[256];
    while (fgets(line, 256, fd)) {
        int id;
        char sha1[256];
        if (sscanf(line, "%d %s", &id, sha1) != 2) continue;  // chunkfile每行为id HASH值
        chunk_t *chunk = malloc(sizeof(chunk_t));
        chunk->id = id;
        hex2binary(sha1, SHA1_HASH_SIZE * 2, (uint8_t *) chunk->sha1);  // 16进制下HASH值长度为2倍
        push(chunks_IHAVE, chunk);
    }
    fclose(fd);
}

void init_chunks_to_get() {
    FILE *fd = fopen(config.chunk_file, "r");

    int chunks_num = 0;
    char line[256];
    while (fgets(line, 256, fd)) chunks_num++;
    chunks_to_get.chunks_num = chunks_num;
    chunks_to_get.chunks = malloc(chunks_num * sizeof(chunk_t));
    chunks_to_get.chunks_data = malloc(chunks_num * sizeof(char *));
    chunks_to_get.statuses = malloc(chunks_num * sizeof(int));
    chunks_to_get.peers = malloc(chunks_num * sizeof(bt_peer_t *));

    for (int i = 0; i < chunks_num; i++) {
        chunks_to_get.statuses[i] = NOT_DOWNLOADED;
        chunks_to_get.peers[i] = NULL;
    }

    fseek(fd, 0, SEEK_SET);
    int i = 0;
    while (fgets(line, 256, fd)) {
        int id;
        char sha1[SHA1_HASH_SIZE * 2];
        if (sscanf(line, "%d %s", &id, sha1) != 2) continue;
        chunks_to_get.chunks[i].id = id;
        hex2binary(sha1, SHA1_HASH_SIZE * 2, (uint8_t *) chunks_to_get.chunks[i].sha1);
        i++;
    }
    fclose(fd);
}

char *update_provider(my_list *chunk_sha1s, bt_peer_t *peer) {
    char *result = NULL;  // 开始下载数据块的HASH值
    int f = 0;  // 是否找到下载目标
    for (list_node *node = chunk_sha1s->head; node != NULL; node = node->next) {
        char *sha1 = (char *) (node->data);
        for (int i = 0; i < chunks_to_get.chunks_num; i++) {
            char *curr_sha1 = chunks_to_get.chunks[i].sha1;
            if (memcmp(curr_sha1, sha1, SHA1_HASH_SIZE) == 0) {
                if (chunks_to_get.statuses[i] == NOT_DOWNLOADED) {
                    chunks_to_get.peers[i] = peer;
                    if (!f) {
                        printf("DOWNLOADING chunk id = %d from peer %d\n", i, peer->id);
                        chunks_to_get.statuses[i] = DOWNLOADING;
                        result = chunks_to_get.chunks[i].sha1;
                        f = 1;
                    }
                }
                break;
            }
        }
    }
    return result;
}

my_list *get_chunks_from_packet(packet *pkt) {
    if (pkt == NULL) return NULL;
    my_list *chunk_sha1s = (my_list *) malloc(sizeof(my_list));
    init_list(chunk_sha1s);
    uint8_t *data = pkt->data;
    int chunks_num = data[0];  // IHAVE载荷的第一个字节为请求数据块的个数
    uint8_t *position = data + 4;
    for (int i = 0; i < chunks_num; i++, position += SHA1_HASH_SIZE) {
        char *sha1 = malloc(SHA1_HASH_SIZE);
        memcpy(sha1, position, SHA1_HASH_SIZE);
        push(chunk_sha1s, sha1);
    }
    return chunk_sha1s;
}

void make_chunks_to_packet(char *data, int start, int chunks_num, chunk_t *chunks) {
    char *position = data + 4;
    int n = 0;  // 需要传输的数据块个数
    for (int i = 0; i < chunks_num; i++) {
        if (chunks_to_get.statuses[start + i] != NOT_DOWNLOADED) continue;  // 当前块已传输完成
        memcpy(position, chunks[i].sha1, SHA1_HASH_SIZE);
        n++;
        position += SHA1_HASH_SIZE;
    }
    memset(data, 0, 4);
    data[0] = n;
}

packet **make_chunk_data_to_packets(char *sha1) {
    int id;  // 要找的数据块在master_chunks中的id
    for (list_node *node = master_chunks->head; node != NULL; node = node->next) {
        chunk_t *chunk = (chunk_t *) node->data;
        if (memcmp(sha1, chunk->sha1, SHA1_HASH_SIZE) == 0) {
            id = chunk->id;
            break;
        }
    }
    printf("SEND CHUNK id = %d\n", id);

    FILE *fd = fopen(master_chunk_file, "r");
    fseek(fd, BT_CHUNK_SIZE * id, SEEK_SET);  // 指针移到指定块起始位置
    char data[1024];  // 每次打包一字节
    packet **pkts = malloc(CHUNK_SIZE * sizeof(packet *));
    for (uint32_t i = 0; i < CHUNK_SIZE; i++) {
        fread(data, 1024, 1, fd);
        pkts[i] = make_DATA(i + 1, 1024, data);
    }
    fclose(fd);
    return pkts;
}

void save_data(char *sha1, char *data) {
    int i;
    for (i = 0; i < chunks_to_get.chunks_num; i++) {
        char *tmp = chunks_to_get.chunks[i].sha1;
        if (memcmp(tmp, sha1, SHA1_HASH_SIZE) == 0) {
            chunks_to_get.chunks_data[i] = malloc(BT_CHUNK_SIZE);
            memcpy(chunks_to_get.chunks_data[i], data, BT_CHUNK_SIZE);

            break;
        }
    }
    uint8_t curr_sha1[SHA1_HASH_SIZE];
    shahash((uint8_t *) chunks_to_get.chunks_data[i], BT_CHUNK_SIZE, curr_sha1);
    if (memcmp(curr_sha1, sha1, SHA1_HASH_SIZE) != 0)
        chunks_to_get.statuses[i] = NOT_DOWNLOADED;
    else {
        chunks_to_get.statuses[i] = DONE;
        printf("GOT chunk id = %d\n", i);
    }
}

chunk_t *find_chunk(my_list *chunks, char *sha1) {
    if (chunks == NULL || sha1 == NULL) return NULL;
    list_node *node = chunks->head;
    while (node != NULL) {
        chunk_t *result = (chunk_t *) (node->data);
        if (memcmp(sha1, result->sha1, SHA1_HASH_SIZE) == 0) return result;
        node = node->next;
    }
    return NULL;
}
