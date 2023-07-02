#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "connection_pool.h"
#include "chunk.h"
#include "spiffy.h"
#include "process_chunks.h"

extern bt_config_t config;

void init_download_connection(download_connection_t *connection, bt_peer_t *peer, char *sha1) {
    connection->expected_seq = 1;
    connection->position = 0;
    connection->peer = peer;
    memcpy(connection->sha1, sha1, SHA1_HASH_SIZE);
    connection->data = malloc(BT_CHUNK_SIZE);
    connection->rcv_time = clock();
    connection->overtime_count = 0;
}

void init_upload_connection(upload_connection_t *connection, bt_peer_t *peer, packet **pkts) {
    connection->last_acked = 0;
    connection->next = 1;
    connection->available = 64;
    connection->duplicate_num = 0;
    connection->peer = peer;
    connection->pkts = pkts;
    connection->rcv_time = clock();
    connection->overtime_count = 0;
}

void init_download_pool(download_pool_t *pool) {
    pool->size = 0;
    pool->connections = malloc(config.max_conn * sizeof(download_connection_t *));
    for (int i = 0; i < config.max_conn; i++) pool->connections[i] = NULL;
}

void init_upload_pool(upload_pool_t *pool) {
    pool->size = 0;
    pool->connections = malloc(config.max_conn * sizeof(upload_connection_t *));
    for (int i = 0; i < config.max_conn; i++) pool->connections[i] = NULL;
}

void add_to_download_pool(download_pool_t *pool, download_connection_t *connection) {
    for (int i = 0; i < config.max_conn; i++)
        if (pool->connections[i] == NULL) {
            pool->connections[i] = connection;
            break;
        }
    pool->size++;
}

void add_to_upload_pool(upload_pool_t *pool, upload_connection_t *connection) {
    for (int i = 0; i < config.max_conn; i++)
        if (pool->connections[i] == NULL) {
            pool->connections[i] = connection;
            break;
        }
    pool->size++;
}

void remove_from_download_pool(download_pool_t *pool, bt_peer_t *peer) {
    for (int i = 0; i < config.max_conn; i++) {
        download_connection_t *connection = pool->connections[i];
        if (connection != NULL && connection->peer->id == peer->id) {
            free(connection->data);
            free(connection);
            pool->connections[i] = NULL;
            pool->size--;
            break;
        }
    }
}

void remove_from_upload_pool(upload_pool_t *pool, bt_peer_t *peer) {
    for (int i = 0; i < config.max_conn; i++) {
        upload_connection_t *connection = pool->connections[i];
        if (connection != NULL && connection->peer->id == peer->id) {
            for (int j = 0; j < CHUNK_SIZE; j++) free(connection->pkts[j]);
            free(connection->pkts);
            free(connection);
            pool->connections[i] = NULL;
            pool->size--;
            break;
        }
    }
}

download_connection_t *find_download_connection(download_pool_t *pool, bt_peer_t *peer) {
    for (int i = 0; i < config.max_conn; i++) {
        download_connection_t *connection = pool->connections[i];
        if (connection != NULL && connection->peer->id == peer->id) return connection;
    }
    return NULL;
}

upload_connection_t *find_upload_connection(upload_pool_t *pool, bt_peer_t *peer) {
    for (int i = 0; i < config.max_conn; i++) {
        upload_connection_t *connection = pool->connections[i];
        if (connection != NULL && connection->peer->id == peer->id) return connection;
    }
    return NULL;
}

void send_DATAs(upload_connection_t *connection, int sock, struct sockaddr *to) {
    while (connection->next <= connection->available) {
        spiffy_sendto(sock, connection->pkts[connection->next - 1],
                      connection->pkts[connection->next - 1]->header.packet_len, 0, to, sizeof(*to));
        connection->next++;
    }
}
