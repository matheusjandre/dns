#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include "./packet.h"

#define BUFFER_SIZE 4096

#define SOCKET_LISTEN_SUCCESS 1
#define SOCKET_LISTEN_RETRY 1
#define SOCKET_LISTEN_TIMEOUT -1
#define SOCKET_LISTEN_CRC_FAIL -2

#define TIMEOUT_SECONDS 4

typedef enum
{
  SENDING = 1,
  RECEIVING
} network_state_e;

typedef struct sockaddr_ll address_t;

typedef struct
{
  int socket;
  network_state_e state;
  packet_t *last_packet;
} network_state_t;

int verify_crc8(uint8_t expected_crc, const uint8_t *data, uint8_t size);
uint8_t compute_crc8(const uint8_t *data, size_t length);


// Create a raw socket
int create_socket(char *interface_label);

// Listen to a socket
int listen_packet(packet_t *current, network_state_t *network, uint8_t from);

// Send a packet
void send_packet(network_state_t *network, packet_union_t pu);

#endif