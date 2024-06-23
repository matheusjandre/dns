#ifndef _RAWSOCKET_H_
#define _RAWSOCKET_H_

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdlib.h>
#include <stdio.h>

// Create a raw socket
int create_raw_socket(char *interface_label);

#endif