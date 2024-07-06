#include "libs/socket.h"
#include "libs/packet.h"
#include "libs/protocol.h"

#include <dirent.h>

typedef enum
{
  IDLE = 0,
  LISTING = 1,
} server_state_e;

typedef struct
{
  unsigned int id;
  char selection[63];
  char path[257];
} movie_t;

typedef struct
{
  int amount;
  movie_t *movies;
} catalog_t;

typedef struct
{
  network_state_t *network;
  catalog_t *catalog;
  server_state_e substate;
  int test;
} server_t;

int filter(const struct dirent *name)
{
  if (strstr(name->d_name, ".mov") || strstr(name->d_name, ".mp4"))
    return 1;
  else
    return 0;
}

catalog_t *scan_movies(char *path)
{
  catalog_t *catalog = (catalog_t *)malloc(sizeof(catalog_t));

  if (!catalog)
  {
    perror("Catalog malloc failed.");
    exit(EXIT_FAILURE);
  }

  struct dirent **list;

  int n = scandir(path, &list, filter, alphasort);
  if (n == -1)
  {
    perror("scandir");
    exit(EXIT_FAILURE);
  }

  catalog->amount = n;
  catalog->movies = (movie_t *)malloc(n * sizeof(movie_t));

  if (!catalog->movies)
  {
    perror("Movies malloc failed.");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < n; i++)
  {
    catalog->movies[i].id = i + 1;
    sprintf(catalog->movies[i].path, "%s/%s", path, list[i]->d_name);
    char selection[340];

    if (strstr(list[i]->d_name, ".mov") || strstr(list[i]->d_name, ".mp4"))
      list[i]->d_name[strlen(list[i]->d_name) - 4] = '\0';

    sprintf(selection, "%d) %s", catalog->movies[i].id, list[i]->d_name);

    strcpy(catalog->movies[i].selection, selection);
    free(list[i]);
  }

  return catalog;
}

void start_server(server_t **server, char *interface_label)
{
  *server = (server_t *)malloc(sizeof(server_t));

  if (!server)
  {
    perror("Server malloc failed.");
    exit(EXIT_FAILURE);
  }

  // Start server network
  (*server)->network = (network_state_t *)malloc(sizeof(network_state_t));

  if (!(*server)->network)
  {
    perror("Network malloc failed.");
    exit(EXIT_FAILURE);
  }

  (*server)->network->last_packet = NULL;
  (*server)->network->state = RECEIVING;

  (*server)->network->socket = create_socket(interface_label);

  uint8_t dest_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  memset(&((*server)->network->address), 0, sizeof((*server)->network->address));
  (*server)->network->address.sll_family = AF_PACKET;
  (*server)->network->address.sll_protocol = htons(ETH_P_ALL);
  (*server)->network->address.sll_ifindex = if_nametoindex(interface_label);
  (*server)->network->address.sll_halen = ETH_ALEN;
  memcpy((*server)->network->address.sll_addr, dest_mac, ETH_ALEN);

  // Scan movies
  (*server)->catalog = scan_movies("./base");

  // Start server state
  (*server)->substate = IDLE;

  (*server)->test = 5;
}

void stop_server(server_t **server)
{
  // Close socket
  close((*server)->network->socket);

  // Free network
  free((*server)->network->last_packet);
  free((*server)->network);

  // Free catalog
  free((*server)->catalog->movies);
  free((*server)->catalog);

  // Free server
  free((*server));
}

int main(int argc, char *argv[])
{
  int running = 1;
  server_t *server = NULL;
  packet_union_t sending = {0};
  packet_t *current = (packet_t *)malloc(sizeof(packet_t));

  if (argc != 2)
  {
    fprintf(stderr, "To use: %s <network interface>\n", argv[0]);
    return -1;
  }

  start_server(&server, argv[1]);

  printf("Server started\n");

  while (running)
  {
    switch (server->network->state)
    {
    case RECEIVING:
    {
      // If nothing is received, continue
      if (!listen_socket(current, server->network))
        continue;

      // If the packet is the same as the last one, continue
      if (server->network->last_packet && server->network->last_packet->sequence == current->sequence)
        continue;

      switch (current->type)
      {
      case TYPE_LIST:
      {
        printf("Listing movies\n");
        sending = (packet_union_t){0};
        pack(&sending.packet, TYPE_ACK, 0, NULL, 0);
        send_packet(server->network, sending);

        server->substate = LISTING;
        server->network->state = SENDING;
        break;
      }
      case TYPE_DOWNLOAD:
      {
        printf("Downloading movie\n");
        break;
      }
      default:
      {
        printf("Sending ERROR\n");
        sending = (packet_union_t){0};
        pack(&sending.packet, TYPE_ERROR, 0, NULL, 0);
        send_packet(server->network, sending);
        break;
      }
      }

      if (server->network->last_packet)
      {
        free(server->network->last_packet);
        server->network->last_packet = NULL;
      }

      server->network->last_packet = (packet_t *)malloc(sizeof(packet_t));
      memcpy(server->network->last_packet, current, sizeof(packet_t));

      break;
    }
    case SENDING:
    {

      switch (server->substate)
      {
      case LISTING:
      {
        free(server->network->last_packet);
        server->network->last_packet = NULL;

        for (int i = 0; i < server->catalog->amount; i++)
        {
          printf("Sending movie: %s\n", server->catalog->movies[i].selection);
          // sending = (packet_union_t){0};
          // pack(&sending.packet, TYPE_SHOW, 0, server->catalog->movies[i].selection, strlen(server->catalog->movies[i].selection));
          // send_packet(server, sending);
        }

        // sending = (packet_union_t){0};
        // pack(&sending.packet, TYPE_END_TX, 0, NULL, 0);
        // send_packet(server, sending);

        server->network->state = RECEIVING;
        break;
      }
      case IDLE:
      {
        continue;
      }
      }

      break;
    }
    }
  }

  free(current);
  stop_server(&server);

  return 0;
}