#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <ctime>
#include <errno.h>

#include "utils.h"

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    timeval time_v;
    packet ack_pkt;

    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sockfd, reinterpret_cast<sockaddr *>(&client_addr), sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    // send input.txt or whatever argument we specify to server
    // max packet size = 1200 bytes (header + payload)
    // client sends to port 5002
    // server sends to port 5001

    // step 1, read file in chunks and send data to 

    time_v.tv_sec = 0; 
    time_v.tv_usec = 210000; // 0.25 seconds
    if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &time_v, sizeof(time_v)) < 0) {
        perror("Error setting socket timeout");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    int seq_num = 0;
    bool last_ACK = false;
    while (!feof(fp)) {
        if (last_ACK == true) {
            break;
        }
        packet batch[WINDOW_SIZE];
        for (int i = 0; i < WINDOW_SIZE; i++) {
            packet pkt;
            pkt.seqnum = seq_num; 
            pkt.acknum = 0;
            pkt.ack = 0;
            pkt.last = 0;
            pkt.length = fread(pkt.payload, sizeof(char), PAYLOAD_SIZE, fp); 
            if (pkt.length < PAYLOAD_SIZE) {   // Check if last packet
                pkt.last = 1;
                batch[i] = pkt;
                break;
            }
            batch[i] = pkt;
            seq_num++;
        }
        
        int j = 0;
        while(j < WINDOW_SIZE) {
            printSend(&batch[j], 0);
            int bytes_sent = sendto(send_sockfd, &batch[j], sizeof(batch[j]), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to)); // Send packet to the server
            if (bytes_sent < 0) {
                perror("Packet send failed"); fclose(fp); close(listen_sockfd); close(send_sockfd); return 1;
            } else if (batch[j].last == 1) {
                break;
            }
            j++;
        }

        bool ack_received[WINDOW_SIZE] = {false};
        int start_ack_num = batch[0].seqnum;
        printf("start_ack_num: %d\n", start_ack_num);
        while (true) {
            int recv_len = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size);
            printf("recv_len: %d\n", recv_len);
            if (recv_len > 0) {
                if (ack_pkt.acknum >= start_ack_num && ack_pkt.acknum < start_ack_num + WINDOW_SIZE) {
                    int modified_window_size = WINDOW_SIZE;
                    if (ack_pkt.last == true) {
                        last_ACK = true;
                        modified_window_size = ack_pkt.acknum % WINDOW_SIZE;
                    }
                    int index = ack_pkt.acknum % WINDOW_SIZE;
                    if (!ack_received[index]) {
                        ack_received[index] = true;
                        printf("ACK received for packet with seqnum: %d\n", ack_pkt.acknum);
                    }
                    bool all_acks_received = true;
                    
                    for (int i = 0; i < modified_window_size; ++i) {
                        if (!ack_received[i]) {
                            all_acks_received = false;
                            break;
                        }
                    }
                    if (all_acks_received) {
                        printf("ALL ACKs RECEIVED FOR BATCH\n\n");
                        time_v.tv_usec = 250000;
                        break; // Exit loop if all ACKs are received
                    } 
                }                
            } else if (errno == EWOULDBLOCK) {
                printf("Timeout occurred. Retransmitting packets.\n");
                time_v.tv_usec = time_v.tv_usec * 2;
                for (int i = 0; i < WINDOW_SIZE; i++) {
                    sendto(send_sockfd, &batch[i], sizeof(batch[i]), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));
                    printf("Retransmitted packet with seqnum: %d\n", batch[i].seqnum);
                }
                printf("\n");
            }
        }
    }
    printf("Entire File received successfully.");
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
