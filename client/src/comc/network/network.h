#ifndef MEMORYLAB_NETWORK_H
#define MEMORYLAB_NETWORK_H

#include "network_win_compat.h"

int initialize_network(void);
SOCKET connect_server(const char *host, unsigned short port);
int send_request(SOCKET connection, const char *host, const char *path);
char *receive_response(SOCKET connection);
void close_connection(SOCKET connection);
void cleanup_network(void);

#endif
