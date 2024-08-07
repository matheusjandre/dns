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
  uint8_t sent;
  uint8_t to_clear;
  packet_t *window_buffer;
} window_t;

typedef struct
{
  network_state_t *network;
  catalog_t *catalog;
  server_state_e substate;
  window_t *window[5];
} server_t;

char interface_label[32];

int filter(const struct dirent *name)
{
  if (strstr(name->d_name, ".mov") || strstr(name->d_name, ".mp4"))
    return 1;
  else
    return 0;
}

int compare_window_sequence(const void *a, const void *b)
{
  const window_t *window_a = *(const window_t **)a;
  const window_t *window_b = *(const window_t **)b;

  // Compare the sequence numbers in window_buffer
  return (window_a->window_buffer->sequence - window_b->window_buffer->sequence);
}

// Function to sort the window array
void sort_server_windows(server_t *server)
{
  // Fixed size of the window array is 5
  size_t num_windows = 5;

  // Sort the window array
  qsort(server->window, num_windows, sizeof(server->window[0]), compare_window_sequence);
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

  // Start window
  for (int i = 0; i < 5; i++)
  {
    (*server)->window[i] = (window_t *)malloc(sizeof(window_t));
    (*server)->window[i]->sent = 0;
    (*server)->window[i]->to_clear = 1;
    (*server)->window[i]->window_buffer = NULL;
  }
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
            printf("Sending end tx\n");
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

        int8_t last_pkg_seq = -1;
        long total_packets_sent = 0;

        do
        {
          // for last_ack_index to 5
          for (int j = 0; j < 5; j++)
          {
            // Create pckg to buffer
            if (server->window[j]->to_clear == 1)
            {
              // Clears
              if (server->window[j]->window_buffer)
                free(server->window[j]->window_buffer);

              // Resets
              last_pkg_seq++;
              if (last_pkg_seq == 32)
                last_pkg_seq = 0;

              // Create
              packet_t *packet = (packet_t *)malloc(sizeof(packet_t));
              packet->type = TYPE_DATA;
              packet->sequence = last_pkg_seq;
              packet->from = 0;

              // Read file
              uint8_t data[DATA_SIZE] = {0};
              size_t read = fread(data, 1, DATA_SIZE, file);

              packet->size = read;
              memcpy(packet->data, data, read);

              // Update window
              server->window[j]->window_buffer = packet;
              server->window[j]->to_clear = 0;
              server->window[j]->sent = 0;
            }

            // if window is not full
            if (server->window[j]->sent == 0)
            {

              // Send packet
              printf("Sending packet: %d\n", server->window[j]->window_buffer->sequence);
              send_packet_helper(server->network, server->window[j]->window_buffer->type, server->window[j]->window_buffer->sequence, server->window[j]->window_buffer->data, server->window[j]->window_buffer->size, 0);

              // Update window
              server->window[j]->sent = 1;
            }
          }

          // After sending (5 - last_ack_index) packets, we need to wait for an ack
          // if we receive an ack, we need to update the window
          // if we receive a nack, we need to resend the window

          // wait for ack
          if (!listen_packet(current, server->network, 0))
            continue;
          if (!listen_packet(current, server->network, 0)) // fuck loopback
            continue;

          if (current->type == TYPE_ACK)
          {
            printf("Recebendo ack cur: %d\n", current->sequence);
            

            // Search on the buffer for the pck with current_sequence
            for (int x = 0; x < 5; x++)
            {
              if (server->window[x]->window_buffer->sequence <= current->sequence)
              {
                printf("Limpando buffer de seq: %d\n", server->window[x]->window_buffer->sequence);
                server->window[x]->to_clear = 1;
              }
            }

            total_packets_sent++;
          }

          if (current->type == TYPE_NACK)
          {
            for (int x = 0; x < 5; x++)
            {
              if (server->window[x]->window_buffer->sequence < current->sequence)
              {
                printf("Limpando buffer de seq: %d\n", server->window[x]->window_buffer->sequence);
                server->window[x]->to_clear = 1;
              }
              else
              {
                //   // We cant just resend everything, we need to send in order
                sort_server_windows(server);

                printf("Reenviando buffer de seq: %d\n", server->window[x]->window_buffer->sequence);
                server->window[x]->sent = 0;

                server->window[x]->window_buffer->crc = compute_crc8(server->window[x]->window_buffer->data, server->window[x]->window_buffer->size);
              }
            }
          }

          printf("falta: %d\n", total_packets - total_packets_sent);

        } while (total_packets_sent < total_packets);

        // Send end tx
        printf("Download finished\n");
        send_packet_helper(server->network, TYPE_END_TX, 0, NULL, 0, 0);
        reset_server(&server);

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