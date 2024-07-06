#include "libs/socket.h"
#include "libs/packet.h"
#include "libs/protocol.h"
#include <fcntl.h>
#include <errno.h>

enum menu_option
{
  LIST = 1,
  EXIT
};

typedef enum
{
  IDLE = 0,
  LISTING = 1,
} client_state_e;

typedef struct
{
  network_state_t *network;
  client_state_e substate;
} client_t;

int get_option()
{
  int option = 0;
  int invalid = 0;

  do
  {
    printf("Selecione a opção:\n");
    printf("    1) Listar\n");
    printf("    2) Sair\n");
    printf("\n> ");

    scanf("%d", &option);

    // if options is a character, clear buffer
    if (option == 0)
    {
      while (getchar() != '\n')
        ;
    }

    invalid = option < 1 || option > 2;

    if (invalid)
      printf("Opção inválida\n");

  } while (invalid);

  system("clear");

  return option;
}

void start_client(client_t **client, char *interface_label)
{
  (*client) = (client_t *)malloc(sizeof(client_t));

  if (!client)
  {
    perror("Client malloc failed.");
    exit(EXIT_FAILURE);
  }

  // Start client network
  (*client)->network = (network_state_t *)malloc(sizeof(network_state_t));

  if (!(*client)->network)
  {
    perror("Network malloc failed.");
    exit(EXIT_FAILURE);
  }

  (*client)->network->last_packet = (packet_t *)malloc(sizeof(packet_t));

  if (!(*client)->network->last_packet)
  {
    perror("Last packet malloc failed.");
    exit(EXIT_FAILURE);
  }

  (*client)->network->state = SENDING;
  (*client)->network->socket = create_socket(interface_label);

  uint8_t dest_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  memset(&(*client)->network->address, 0, sizeof((*client)->network->address));
  (*client)->network->address.sll_family = AF_PACKET;
  (*client)->network->address.sll_protocol = htons(ETH_P_ALL);
  (*client)->network->address.sll_ifindex = if_nametoindex(interface_label);
  (*client)->network->address.sll_halen = ETH_ALEN;
  memcpy((*client)->network->address.sll_addr, dest_mac, ETH_ALEN);
}

void stop_client(client_t **client)
{
  // Close socket
  close((*client)->network->socket);

  // Free network
  free((*client)->network->last_packet);
  free((*client)->network);

  // Free client
  free((*client));
}

void list_files(client_t *client)
{

  // printf("Listando arquivos\n");

  // Initialize the packet
  // packet_union_t send = {0};
  // packet_union_t *receiving = NULL;

  // pack(&send.packet, TYPE_LIST, 0, NULL, 0);
  // printf("Sending packet type: %d\n", send.packet.type);
  // send_packet(client, send);

  // while (1)
  // {
  //   if (current >= client->packet_count)
  //   {
  //     if (client->packet_buffer)
  //       free(client->packet_buffer);

  //     listen_socket(client);
  //   }

  //   for (int i = 0; i < client->packet_count; i++)
  //   {
  //     printf("Packet %d: %d\n", i, client->packet_buffer[i]->packet.type);
  //   }

  //   if (!receiving)
  //     continue;

  //   switch (receiving->packet.type)
  //   {
  //   case TYPE_ACK:
  //   {
  //     client->last_sequence = receiving->packet.sequence;
  //     client->state = RECEIVING;
  //     printf("Pacote recebido com sucesso\n");
  //     return;
  //   }
  //   }
  // }
}

int main(int argc, char *argv[])
{
  int running = 1;
  client_t *client = NULL;
  packet_union_t sending = {0};
  packet_t *current = (packet_t *)malloc(sizeof(packet_t));

  if (argc != 2)
  {
    fprintf(stderr, "To use: %s <network interface>\n", argv[0]);
    return -1;
  }

  start_client(&client, argv[1]);

  printf("Client started\n");

  while (running)
  {
    switch (client->network->state)
    {
    case RECEIVING:
    {
      // If nothing is received, continue
      if (!listen_socket(current, client->network))
        continue;

      // If the packet is the same as the last one, continue
      if (client->network->last_packet && client->network->last_packet->sequence == current->sequence)
        continue;

      if (current->type == TYPE_SHOW)
        printf("%s\n", current->data);

      if (client->network->last_packet)
      {
        free(client->network->last_packet);
        client->network->last_packet = NULL;
      }

      client->network->last_packet = (packet_t *)malloc(sizeof(packet_t));
      memcpy(client->network->last_packet, current, sizeof(packet_t));

      if (current->type == TYPE_END_TX)
      {
        client->network->state = SENDING;
        continue;
      }

      break;
    }
    case SENDING:
    {
      int option = get_option();

      switch (option)
      {
      case LIST:
      {
        sending = (packet_union_t){0};
        pack(&sending.packet, TYPE_LIST, 0, 0, 0);
        send_packet(client->network, sending);

        if (!listen_socket(current, client->network))
        {
          printf("TIMEOUT\n");
          continue;
        }

        printf("Listando arquivos [%d]\n", current->type);

        switch (current->type)
        {
        case TYPE_ACK:
        {
          client->network->state = RECEIVING;
          continue;
        }
        default:
        {
          printf("Erro ao listar arquivos\n");
          break;
        }
        }

        break;
      }
      case EXIT:
      {
        running = 0;
        break;
      }
      }

      break;
    }
    }
  }

  free(current);
  stop_client(&client);

  return 0;
}