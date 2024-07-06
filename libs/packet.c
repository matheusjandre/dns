#include "./packet.h"

// Example function to compute CRC-8 (this is just a placeholder; use a proper implementation)
uint8_t compute_crc8(const uint8_t *data, size_t length)
{
  uint8_t crc = 0;
  for (size_t i = 0; i < length; ++i)
  {
    crc ^= data[i];
  }
  return crc;
}

// Pack data into a packet
void pack(packet_t *packet, uint8_t type, uint8_t sequence, void *data, uint8_t size)
{
  packet->start_marker = START_MARKER;

  packet->size = size;
  packet->type = type;
  packet->sequence = sequence;

  memcpy(packet->data, data, size);

  packet->crc = compute_crc8((uint8_t *)packet, sizeof(packet_t) - 1);
}

// Dump bits of a byte array
void dump_bits(const uint8_t *data, size_t size)
{
  int column = 0;
  for (size_t i = 0; i < size; i++)
  {
    for (int j = 7; j >= 0; j--)
      printf("%d", (data[i] >> j) & 1);

    printf(" ");
    column++;
    if (column == 4)
    {
      printf("\n");
      column = 0;
    }
  }
  printf("\n");
}