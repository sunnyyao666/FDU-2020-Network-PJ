#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "process_packets.h"
#include "spiffy.h"
#include "packet.h"
#include "bt_parse.h"
#include "connection_pool.h"

extern int sock;
extern bt_config_t config;
extern my_list *chunks_IHAVE;
extern chunks_to_get_t chunks_to_get;
extern download_pool_t download_pool;
extern upload_pool_t upload_pool;
extern int is_download_done;

void process_download(int sock) {
    my_list *pkts_WHOHAS = (my_list *) malloc(sizeof(my_list));
    init_list(pkts_WHOHAS);

    if (chunks_to_get.chunks_num > MAX_CHUNKS_NUM) {
        int q = chunks_to_get.chunks_num / MAX_CHUNKS_NUM;
        int r = chunks_to_get.chunks_num - q * MAX_CHUNKS_NUM;
        for (int i = 0; i < q; i++) {
            char data[DATA_SIZE];
            make_chunks_to_packet(data, i * MAX_CHUNKS_NUM, MAX_CHUNKS_NUM, chunks_to_get.chunks + i * MAX_CHUNKS_NUM);
            uint16_t data_len = 4 + MAX_CHUNKS_NUM * SHA1_HASH_SIZE;
            push(pkts_WHOHAS, make_WHOHAS(data_len, data));
        }
        char data[DATA_SIZE];
        make_chunks_to_packet(data, q * MAX_CHUNKS_NUM, r, chunks_to_get.chunks + q * MAX_CHUNKS_NUM);
        uint16_t data_len = 4 + r * SHA1_HASH_SIZE;
        push(pkts_WHOHAS, make_WHOHAS(data_len, data));
    } else {
        char data[DATA_SIZE];
        make_chunks_to_packet(data, 0, chunks_to_get.chunks_num, chunks_to_get.chunks);
        uint16_t data_len = 4 + chunks_to_get.chunks_num * SHA1_HASH_SIZE;
        push(pkts_WHOHAS, make_WHOHAS(data_len, data));
    }

    list_node *node = pkts_WHOHAS->head;
    while (node != NULL) {
        packet *pkt = (packet *) node->data;
        bt_peer_t *peer = config.peers;
        while (peer != NULL) {
            if (peer->id != config.identity) {
                struct sockaddr *to = (struct sockaddr *) &(peer->addr);
                spiffy_sendto(sock, pkt, pkt->header.packet_len, 0, to, sizeof(*to));
            }
            peer = peer->next;
        }
        free(pkt);
        node = node->next;
    }
    alarm(1);

}

void process_PACKET(int sock, packet *pkt, struct sockaddr_in *from, socklen_t fromlen) {
    ntoh_packet(pkt);
    if (!is_packet_valid(pkt)) {
        puts("Bad packet!");
        return;
    }
    bt_peer_t *peer = find_peer(from);
    packet_type type = get_packet_type(pkt->header.type);
    switch (type) {
        case WHOHAS: {
            packet *result = process_WHOHAS(pkt);
            if (result != NULL) {
                struct sockaddr *to = (struct sockaddr *) (&(peer->addr));
                spiffy_sendto(sock, result, result->header.packet_len, 0, to, sizeof(*to));
                free(result);
            }
            break;
        }
        case IHAVE: {
            packet *result = process_IHAVE(pkt, peer);
            if (result != NULL) {
                struct sockaddr *to = (struct sockaddr *) (&(peer->addr));
                spiffy_sendto(sock, result, result->header.packet_len, 0, to, sizeof(*to));
                free(result);
            }
            break;
        }
        case GET: {
            process_GET(sock, pkt, peer);
            break;
        }
        case DATA: {
            process_DATA(sock, pkt, peer);
            break;
        }
        case ACK: {
            process_ACK(sock, pkt, peer);
            break;
        }
        case DENIED:
            break;
        default:
            break;
    }
}

packet *process_WHOHAS(packet *pkt) {
    int chunks_num = pkt->data[0];  // WHOHAS载荷的第一个字节为请求数据块的个数
    int n = 0;  // 找到多少个符合条件的数据块
    int position = 4;
    char data[DATA_SIZE];
    char *sha1 = (char *) pkt->data + position;

    for (int i = 0; i < chunks_num; i++, sha1 += SHA1_HASH_SIZE)
        if (find_chunk(chunks_IHAVE, sha1) != NULL) {
            n++;
            memcpy(data + position, sha1, SHA1_HASH_SIZE);
            position += SHA1_HASH_SIZE;
        }
    if (n == 0) return NULL;
    memset(data, 0, 4);  // 对齐4字节
    data[0] = n;  // IHAVE载荷第一字节为包含的数据块个数
    return make_IHAVE(position, data);
}

packet *process_IHAVE(packet *pkt, bt_peer_t *peer) {
    if (find_download_connection(&download_pool, peer) != NULL) return NULL;  // 与该对等方已存在下载链接
    if (download_pool.size >= config.max_conn) return NULL;  // 下载链接数已达最大

    my_list *chunk_sha1s = get_chunks_from_packet(pkt);
    char *sha1 = update_provider(chunk_sha1s, peer);

    download_connection_t *connection = (download_connection_t *) malloc(sizeof(download_connection_t));
    init_download_connection(connection, peer, sha1);
    add_to_download_pool(&download_pool, connection);
    alarm(1);

    return make_GET(sha1);
}

void process_GET(int sock, packet *pkt, bt_peer_t *peer) {
    upload_connection_t *connection = find_upload_connection(&upload_pool, peer);
    if (connection != NULL) {
        remove_from_upload_pool(&upload_pool, peer);
        connection = NULL;
    }
    if (upload_pool.size >= config.max_conn) return;  // 上传链接数已达最大
    char sha1[SHA1_HASH_SIZE];
    memcpy(sha1, pkt->data, SHA1_HASH_SIZE);
    packet **pkts = make_chunk_data_to_packets(sha1);

    connection = (upload_connection_t *) malloc(sizeof(upload_connection_t));
    init_upload_connection(connection, peer, pkts);
    add_to_upload_pool(&upload_pool, connection);
    send_DATAs(connection, sock, (struct sockaddr *) (&(peer->addr)));
    alarm(1);
}

void process_DATA(int sock, packet *pkt, bt_peer_t *peer) {
    download_connection_t *connection = find_download_connection(&download_pool, peer);

    if (connection == NULL) return;
    uint32_t seq_num = pkt->header.seq_num;

    packet *pkt_ACK;
    if (seq_num == connection->expected_seq) {
        int data_len = pkt->header.packet_len - HEADER_SIZE;
        memcpy(connection->data + connection->position, pkt->data, data_len);
        connection->expected_seq += 1;
        connection->position += data_len;
        pkt_ACK = make_ACK(seq_num);
    } else pkt_ACK = make_ACK(connection->expected_seq - 1);

    struct sockaddr *to = (struct sockaddr *) (&(peer->addr));
    spiffy_sendto(sock, pkt_ACK, pkt_ACK->header.packet_len, 0, to, sizeof(*to));
    free(pkt_ACK);

    connection->rcv_time = clock();
    connection->overtime_count = 0;
    alarm(1);

    // 检查是否读完整个chunk
    if (connection->position == BT_CHUNK_SIZE) {
        save_data(connection->sha1, connection->data);
        remove_from_download_pool(&download_pool, peer);
        // 检查是否所有chunks都已下载完成
        int f = 1;
        for (int i = 0; i < chunks_to_get.chunks_num; i++)
            if (chunks_to_get.statuses[i] != DONE) {
                f = 0;
                break;
            }
        if (f) {
            // 整个文件下载完毕，输出至目标文件
            is_download_done = 1;
            FILE *fd = fopen(config.output_file, "wb+");
            for (int i = 0; i < chunks_to_get.chunks_num; i++) fwrite(chunks_to_get.chunks_data[i], 1024, 512, fd);
            fclose(fd);
            for (int i = 0; i < chunks_to_get.chunks_num; i++) free(chunks_to_get.chunks_data[i]);
            free(chunks_to_get.chunks_data);
            free(chunks_to_get.statuses);
            free(chunks_to_get.peers);
            free(chunks_to_get.chunks);
            printf("GOT %s\n", config.chunk_file);
        } else process_download(sock);
    }
}

void process_ACK(int sock, packet *pkt, bt_peer_t *peer) {

    int ack_num = pkt->header.ack_num;
    upload_connection_t *connection = find_upload_connection(&upload_pool, peer);
    if (connection == NULL) return;

    if (ack_num == CHUNK_SIZE) {
        alarm(0);
        remove_from_upload_pool(&upload_pool, peer);
        alarm(1);
        return;
    }
    // 累积确认
    if (ack_num > connection->last_acked) {
        alarm(0);
        connection->last_acked = ack_num;
        connection->duplicate_num = 0;
        connection->rcv_time = clock();
        connection->overtime_count = 0;
        if (ack_num + 64 <= CHUNK_SIZE) connection->available = ack_num + 64;
        else connection->available = CHUNK_SIZE;
        send_DATAs(connection, sock, (struct sockaddr *) (&(peer->addr)));
        alarm(1);
        return;
    }
    // 重复ACK
    if (ack_num == connection->last_acked) {
        connection->duplicate_num++;
        connection->rcv_time = clock();
        connection->overtime_count = 0;
        if (connection->duplicate_num >= 3) {
            alarm(0);
            connection->next = connection->last_acked + 1;
            connection->duplicate_num = 0;
            if (connection->last_acked + 64 <= CHUNK_SIZE)
                connection->available = connection->last_acked + 64;
            else connection->available = CHUNK_SIZE;
            send_DATAs(connection, sock, (struct sockaddr *) (&(peer->addr)));
            alarm(1);
        }
    }
}

void handle_timeout() {
    int n = 0;  // 现存仍在进行的连接
    // 上传池检查
    for (int i = 0; i < config.max_conn; i++) {
        upload_connection_t *connection = upload_pool.connections[i];
        if (connection == NULL) continue;
        if (clock() - connection->rcv_time < 100) {
            n++;
            continue;
        }
        connection->overtime_count++;
        // 超过阈值，丢弃对等方
        if (connection->overtime_count > 5) {
            printf("Giving up on peer %d\n", connection->peer->id);
            remove_from_upload_pool(&upload_pool, connection->peer);
            continue;
        }
        n++;
        connection->rcv_time = clock();
        connection->next = connection->last_acked + 1;
        connection->duplicate_num = 0;
        // 重发
        if (connection->last_acked + 64 <= CHUNK_SIZE)
            connection->available = connection->last_acked + 64;
        else connection->available = CHUNK_SIZE;

        send_DATAs(connection, sock, (struct sockaddr *) (&(connection->peer->addr)));
    }

    // 下载池检查
    for (int i = 0; i < config.max_conn; i++) {
        download_connection_t *connection = download_pool.connections[i];
        if (connection == NULL) continue;
        if (clock() - connection->rcv_time < 100) {
            n++;
            continue;
        }
        connection->overtime_count++;
        // 超过阈值，丢弃对等方并重新请求下载
        if (connection->overtime_count > 5) {
            printf("Giving up on peer %d\n", connection->peer->id);
            // 将当前chunk状态改为未下载
            for (int j = 0; j < chunks_to_get.chunks_num; j++) {
                char *tmp = chunks_to_get.chunks[j].sha1;
                if (memcmp(tmp, connection->sha1, SHA1_HASH_SIZE) == 0) {
                    chunks_to_get.statuses[j] = NOT_DOWNLOADED;
                    break;
                }
            }
            remove_from_download_pool(&download_pool, connection->peer);
            // 重新请求下载
            process_download(sock);
            continue;
        }
        n++;
        connection->rcv_time = clock();
        // 重发ACK
        packet *pkt;
        if (connection->expected_seq == 1) pkt = make_GET(connection->sha1);
        else pkt = make_ACK(connection->expected_seq - 1);
        struct sockaddr *to = (struct sockaddr *) &(connection->peer->addr);
        spiffy_sendto(sock, pkt, pkt->header.packet_len, 0, to, sizeof(*to));
    }

    // 若还有连接存在则继续计时
    if (n > 0) alarm(1);
    else if (!is_download_done) process_download(sock);


}

bt_peer_t *find_peer(struct sockaddr_in *addr) {
    bt_peer_t *peer;
    for (peer = config.peers; peer != NULL; peer = peer->next)
        if (peer->addr.sin_port == addr->sin_port) return peer;
    return NULL;
}
