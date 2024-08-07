#ifndef _PACKET_H_
#define _PACKET_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "./protocol.h"

#define DATA_SIZE 63

typedef struct
{
  uint8_t start_marker : 8; // 8 bits
  uint8_t size : 6;         // 6 bits
  uint8_t sequence : 5;     // 5 bits // 32 is max
  uint8_t type : 5;         // 5 bits
  uint8_t data[DATA_SIZE];  // 63 bytes
  uint8_t crc : 8;          // 8 bits
  uint8_t from : 8;         // 8 bits 0 - server, 1 - client
} packet_t;

typedef union
{
  packet_t packet;
  uint8_t raw_data[sizeof(packet_t)];
} packet_union_t;

typedef struct packet_node
{
  packet_union_t *packet;
  struct packet_node *next;
} packet_node_t;

// Pack data into a packet
void pack(packet_t *packet, uint8_t type, uint8_t sequence, void *data, uint8_t size, uint8_t from);

// Dump bits of a byte array
void dump_bits(const uint8_t *data, size_t size);

#endif