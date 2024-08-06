#include "libs/socket.h"
#include "libs/packet.h"
#include "libs/protocol.h"

#include <dirent.h>

#define INITIAL_SERVER_NETWORK_STATE RECEIVING
#define INITIAL_SERVER_SUBSTATE IDLE
#define SERVER_TRY 3
typedef enum
{
  IDLE = 0,
  LISTING,
  TRANSACTION
} server_state_e;

typedef struct
{
  char label[63];
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
  packet_t *window_buffer[5];
} server_t;

char interface_label[32];

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
    sprintf(catalog->movies[i].path, "%s/%s", path, list[i]->d_name);

    if (strstr(list[i]->d_name, ".mov") || strstr(list[i]->d_name, ".mp4"))
      list[i]->d_name[strlen(list[i]->d_name) - 4] = '\0';

    strcpy(catalog->movies[i].label, list[i]->d_name);
    free(list[i]);
  }

  return catalog;
}

void send_packet_helper(network_state_t *network, uint8_t type, uint8_t sequence, void *data, uint8_t size, uint8_t from)
{
  packet_union_t sending = (packet_union_t){0};

  pack(&sending.packet, type, sequence, data, size, from);
  send_packet(network, sending);

  if (network->last_packet)
  {
    free(network->last_packet);
  }

  network->last_packet = (packet_t *)malloc(sizeof(packet_t));
  memcpy(network->last_packet, &sending.packet, sizeof(packet_t));

  return;
}

void start_server(server_t **server)
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
  (*server)->network->state = INITIAL_SERVER_NETWORK_STATE;

  (*server)->network->socket = create_socket(interface_label);

  // Scan movies
  (*server)->catalog = scan_movies("./base");

  // Start server state
  (*server)->substate = INITIAL_SERVER_SUBSTATE;
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

void reset_server(server_t **server)
{
  printf("Resetting server\n");
  stop_server(server);
  start_server(server);
}

int main(int argc, char *argv[])
{
  int running = 1;
  server_t *server = NULL;
  packet_t *current = (packet_t *)malloc(sizeof(packet_t));

  if (argc != 2)
  {
    fprintf(stderr, "To use: %s <network interface>\n", argv[0]);
    return -1;
  }

  memcpy(interface_label, argv[1], strlen(argv[1]));
  start_server(&server);

  printf("Server started\n");

  while (running)
  {
    switch (server->network->state)
    {
    case RECEIVING:
    {
      // Receive packet
      if (!listen_packet(current, server->network, 0))
        continue;

      switch (current->type)
      {
      case TYPE_LIST:
      {
        printf("Listing movies\n");

        send_packet_helper(server->network, TYPE_ACK, 0, NULL, 0, 0);

        server->substate = LISTING;
        server->network->state = SENDING;
        break;
      }
      case TYPE_DOWNLOAD:
      {
        printf("Downloading movie: %s\n", current->data);
        server->network->state = SENDING;
        server->substate = TRANSACTION;

        send_packet_helper(server->network, TYPE_ACK, 0, NULL, 0, 0);
        continue;
      }
      default:
      {
        printf("Sending ERROR\n");
        send_packet_helper(server->network, TYPE_ERROR, 0, NULL, 0, 0);
        break;
      }
      }

      break;
    }
    case SENDING:
    {

      switch (server->substate)
      {
      case LISTING:
      {
        // For some reason with are receiving a TYPE_LIST packet here
        if (!listen_packet(current, server->network, 0))
          continue;

        if (current->type != TYPE_ACK)
          reset_server(&server);

        // Reset last packet
        if (server->network->last_packet)
        {
          free(server->network->last_packet);
          server->network->last_packet = NULL;
        }

        int i = 0;
        int try = 0;

        do
        {
          if (try == SERVER_TRY)
            break;

          // Send movie selection
          printf("[%d] Trying to send movie: %s\n", i, server->catalog->movies[i].label);
          send_packet_helper(server->network, TYPE_SHOW, i, server->catalog->movies[i].label, strlen(server->catalog->movies[i].label), 0);

          // Wait for ACK
          if (!listen_packet(current, server->network, 0))
          {
            try++;
            continue;
          }

          if (current->type != TYPE_ACK)
          {
            printf("Failed no ack! type: %d\n", current->type);
            continue;
          }

          i++;
          try = 0;

          if (i == server->catalog->amount)
          {
            send_packet_helper(server->network, TYPE_END_TX, 0, NULL, 0, 0);
          }

        } while (i < server->catalog->amount);

        reset_server(&server);
        break;
      }
      case TRANSACTION:
      {
        printf("Sending movie data\n");
        char path[257] = "";

        for (int i = 0; i < server->catalog->amount; i++)
        {
          if (strcmp(server->catalog->movies[i].label, (char *)current->data) == 0)
          {
            strcpy(path, server->catalog->movies[i].path);
            break;
          }
        }

        if (strcmp(path, "") == 0)
        {
          // TODO: send error 11111
          perror("Movie not found.");
        }

        printf("Sending movie: %s\n", path);

        FILE *file = fopen(path, "rb");

        if (!file)
        {
          perror("fopen");
          exit(EXIT_FAILURE);
        }

        printf("File opened\n");

        // get file size in  bytes
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Print file size in bytes
        printf("File size: %ld bytes\n", size);

        send_packet_helper(server->network, TYPE_FILE_DESCRIPTOR, 0, &size, sizeof(long), 0);

        // wait for ack
        if (!listen_packet(current, server->network, 0))
          continue;

        if (current->type != TYPE_ACK)
          continue;

        long total_packets = size / DATA_SIZE;

        // 1. We have 63 bytes of data to send in each packet, so we need to send the file in chunks
        // 2. We need to send the file in chunks of 63 bytes
        // 3. We will have 5 packets of 63 bytes of floating windows

        if (size % DATA_SIZE != 0)
          total_packets++;

        printf("Total packets: %ld\n", total_packets);
        // Send file in chunks
        uint8_t last_pkg_seq = 0;

        for (int i = 0; i < total_packets;)
        {
          // Build chunks of
          for (int j = 0; j < 5; j++)
          {
            // Read file
            uint8_t data[DATA_SIZE] = {0};
            size_t read = fread(data, 1, DATA_SIZE, file);

            if (read == 0)
            {
              printf("End of file???\n");
              break;
            }

            // Create packet
            if ((server->window_buffer[j]))
            {
              free(server->window_buffer[j]);
            }
            server->window_buffer[j] = (packet_t *)malloc(sizeof(packet_t));

            server->window_buffer[j]->type = TYPE_DATA;
            server->window_buffer[j]->sequence = last_pkg_seq;
            server->window_buffer[j]->size = read;

            // We dont need to copy the data, we just need the sequence number

            // Send packet
            send_packet_helper(server->network, TYPE_DATA, last_pkg_seq, data, read, 0);
          }

          // Wait for ack
          if (!listen_packet(current, server->network, 0))
            continue;
          if (!listen_packet(current, server->network, 0))  // Fucking loopback
            continue; 

          // Check for ack
          if(current->type == TYPE_ACK ){
            // Check sequence
            // Lets try it tomorrow, check if send & wait or voltan
          }
            
        }

        exit(1);

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