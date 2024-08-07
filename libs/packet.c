#include "./packet.h"
#include "./socket.h"


// Pack data into a packet
void pack(packet_t *packet, uint8_t type, uint8_t sequence, void *data, uint8_t size, uint8_t from)
{
  packet->start_marker = START_MARKER;

  packet->size = size;
  packet->type = type;
  packet->sequence = sequence;
  packet->from = from;
  packet->crc = compute_crc8(data, size);

  memcpy(packet->data, data, size);
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