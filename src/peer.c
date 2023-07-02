/*
 * peer.c
 * 
 * Author: Yi Lu <19212010040@fudan.edu.cn>,
 *
 * Modified from CMU 15-441,
 * Original Authors: Ed Bardsley <ebardsle+441@andrew.cmu.edu>,
 *                   Dave Andersen
 * 
 * Class: Networks (Spring 2015)
 *
 */

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "process_packets.h"
#include "my_list.h"
#include "connection_pool.h"

int sock;  // 当前对等方套接字
bt_config_t config;  // 当前对等方配置信息
my_list *master_chunks;  // 所有数据块信息
my_list *chunks_IHAVE;  // 当前对等方拥有的数据块信息
chunks_to_get_t chunks_to_get;  // 要求下载的数据块信息
download_pool_t download_pool;  // 当前对等方下载连接池
upload_pool_t upload_pool;  // 当前对等方上传连接池
int is_download_done = 1;

void peer_run(bt_config_t *config);

// 利用SIGALRM模拟定时器
void handler(int signal) {
    handle_timeout();
}

int main(int argc, char **argv) {

    bt_init(&config, argc, argv);

    DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

#ifdef TESTING
    config.identity = 1; // your group number here
    strcpy(config.chunk_file, "chunkfile");
    strcpy(config.has_chunk_file, "haschunks");
#endif

    bt_parse_command_line(&config);

#ifdef DEBUG
    if (debug & DEBUG_INIT) {
      bt_dump_config(&config);
    }
#endif
    signal(SIGALRM, handler);
    // 初始化所有的chunks列表
    master_chunks = (my_list *) malloc(sizeof(my_list));
    init_list(master_chunks);
    init_master_chunks();

    // 初始化chunks_IHAVE列表
    chunks_IHAVE = (my_list *) malloc(sizeof(my_list));
    init_list(chunks_IHAVE);
    init_chunks_IHAVE();

    // 初始化下载连接池
    init_download_pool(&download_pool);

    // 初始化上传连接池
    init_upload_pool(&upload_pool);

    peer_run(&config);
    return 0;
}


void process_inbound_udp(int sock) {
#define BUFLEN 1500
    struct sockaddr_in from;
    socklen_t fromlen;
    char buf[BUFLEN];

    fromlen = sizeof(from);
    spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);

//    printf("PROCESS_INBOUND_UDP SKELETON -- replace!\n"
//           "Incoming message from %s:%d\n%s\n\n",
//           inet_ntoa(from.sin_addr),
//           ntohs(from.sin_port),
//           buf);

    process_PACKET(sock, (packet *) buf, &from, fromlen);

}

void process_get(char *chunkfile, char *outputfile) {
//    printf("PROCESS GET SKELETON CODE CALLED.  Fill me in!  (%s, %s)\n",
//           chunkfile, outputfile);
    strcpy(config.chunk_file, chunkfile);
    strcpy(config.output_file, outputfile);
    init_chunks_to_get();
    process_download(sock);
}

void handle_user_input(char *line, void *cbdata) {
    char chunkf[128], outf[128];

    bzero(chunkf, sizeof(chunkf));
    bzero(outf, sizeof(outf));

    if (sscanf(line, "GET %120s %120s", chunkf, outf))  // 将用户输入读入chunkf与outf中
        if (strlen(outf) > 0) process_get(chunkf, outf);
}


void peer_run(bt_config_t *config) {
    struct sockaddr_in myaddr;
    fd_set readfds;
    struct user_iobuf *userbuf;

    if ((userbuf = create_userbuf()) == NULL) {
        perror("peer_run could not allocate userbuf");
        exit(-1);
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
        perror("peer_run could not create socket");
        exit(-1);
    }

    // 设置本机地址及端口
    bzero(&myaddr, sizeof(myaddr));
    myaddr.sin_family = AF_INET;  // ipv4
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);  // host to network, 0.0.0.0
    myaddr.sin_port = htons(config->myport);

    if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
        perror("peer_run could not bind socket");
        exit(-1);
    }

    spiffy_init(config->identity, (struct sockaddr *) &myaddr, sizeof(myaddr));

    while (1) {
        int nfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);  // 键盘输入
        FD_SET(sock, &readfds);  // 套接字接收

        nfds = select(sock + 1, &readfds, NULL, NULL, NULL);

        if (nfds > 0) {
            if (FD_ISSET(sock, &readfds)) {  // 接收到来自网络的输入
                process_inbound_udp(sock);
            }

            if (FD_ISSET(STDIN_FILENO, &readfds)) {  // 接收到来自用户的输入
                process_user_input(STDIN_FILENO, userbuf, handle_user_input,
                                   "Currently unused");
            }
        }
    }
}
