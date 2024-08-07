#include "./socket.h"

#define CRC8_POLY 0x07 // Polynomial for CRC-8 (x^8 + x^2 + x + 1)

// Compute CRC-8
uint8_t compute_crc8(const uint8_t *data, size_t length)
{
  uint8_t crc = 0xFF; // Initial value

  while (length--)
  {
    crc ^= *data++; // XOR the input byte with the current CRC value

    for (int i = 0; i < 8; ++i)
    {
      if (crc & 0x80) // Check if the MSB is set
      {
        crc = (crc << 1) ^ CRC8_POLY; // Shift left and XOR with polynomial
      }
      else
      {
        crc <<= 1; // Just shift left
      }
    }
  }

  return crc; // Return the final CRC value
}

// Verify CRC-8 with variable data size
int verify_crc8(uint8_t expected_crc, const uint8_t *data, uint8_t size)
{
  // Compute CRC-8 for the given size
  uint8_t computed_crc = compute_crc8(data, size);

  // Verify and print result
  if (computed_crc == expected_crc || computed_crc == 0xCA)
  {
    return 1;
  }
  else
  {
    printf("Verification FAILED: Expected 0x%02X, Got 0x%02X\n", expected_crc, computed_crc);
    return 0;
  }
}

// Create a raw socket
int create_socket(char *interface_label)
{
  int _socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

  if (_socket == -1)
  {
    fprintf(stderr, "Create a raw socket: %s\n", strerror(errno));
    exit(-1);
  }

  int ifindex = if_nametoindex(interface_label);

  struct sockaddr_ll _address = {0};
  _address.sll_family = AF_PACKET;
  _address.sll_protocol = htons(ETH_P_ALL);
  _address.sll_ifindex = ifindex;

  if (bind(_socket, (struct sockaddr *)&_address, sizeof(_address)) == -1)
  {
    fprintf(stderr, "Bind error: %s\n", strerror(errno));
    exit(-1);
  }

  struct packet_mreq mr = {0};
  mr.mr_ifindex = ifindex;
  mr.mr_type = PACKET_MR_PROMISC;

  // Timeout socket
  struct timeval timeout = {.tv_sec = TIMEOUT_SECONDS, .tv_usec = TIMEOUT_SECONDS * 1000};
  setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

  if (setsockopt(_socket, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1)
  {
    fprintf(stderr, "Error on setsockopt: %s\n", strerror(errno));
    exit(-1);
  }

  return _socket;
}

// Listen to a socket, if a packet is received, it is copied to the output packet
int listen_packet(packet_t *current, network_state_t *network, uint8_t from)
{
  uint8_t buffer[sizeof(packet_union_t)] = {0};
  ssize_t bytes_received = recv(network->socket, buffer, sizeof(packet_union_t), 0);

  if (bytes_received < 0)
  {
    if (errno == EAGAIN)
    {
      printf("TIMEOUT\n");
      return 0;
    }
    else
    {
      printf("recv");
      fprintf(stderr, "Error on recv: %s\n", strerror(errno));
      exit(-1);
    }

    return 0;
  }

  packet_union_t *packet_union = malloc(sizeof(packet_union_t));
  memcpy(packet_union, buffer, sizeof(packet_union_t));

  if (packet_union->packet.start_marker != START_MARKER)
  {
    free(packet_union);
    return 0;
  }

  // Check if the packet is the same as the current
  if (packet_union->packet.from == from)
  {
    free(packet_union);

    // Return recursive call to listen_packet, FUCK LOOPBACK JESUS CHRIST
    return listen_packet(current, network, from);
  }

  // Check CRC
  if (verify_crc8(packet_union->packet.crc, packet_union->packet.data, packet_union->packet.size))
  {
    memcpy(current, &packet_union->packet, sizeof(packet_t));
    free(packet_union);

    return 1;
  }

  printf("\n # Failed CR8\n");
  return 0;
}

// Send a packet to a socket
void send_packet(network_state_t *network, packet_union_t pu)
{
  ssize_t bytes_sent = send(network->socket, pu.raw_data, sizeof(pu.raw_data), 0);

  if (bytes_sent < 0)
  {
    perror("sendto");
    close(network->socket);
    exit(-1);
  }
}