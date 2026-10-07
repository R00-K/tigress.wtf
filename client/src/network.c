#include "network.h"
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESPONSE_LIMIT (1024U * 1024U)

int initialize_network(void)
{
    WSADATA data;
    int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        fprintf(stderr, "[-] WSAStartup failed: %d\n", result);
        return 0;
    }
    return 1;
}

SOCKET connect_server(const char *host, unsigned short port)
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *current;
    char service[6];
    SOCKET connection = INVALID_SOCKET;
    int result;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    _snprintf(service, sizeof(service), "%u", (unsigned int)port);

    result = getaddrinfo(host, service, &hints, &addresses);
    if (result != 0) {
        fprintf(stderr, "[-] getaddrinfo failed: %d\n", result);
        return INVALID_SOCKET;
    }

    for (current = addresses; current != NULL; current = current->ai_next) {
        connection = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (connection == INVALID_SOCKET) {
            continue;
        }
        if (connect(connection, current->ai_addr, (int)current->ai_addrlen) == 0) {
            break;
        }
        fprintf(stderr, "[-] connect failed: %d\n", WSAGetLastError());
        closesocket(connection);
        connection = INVALID_SOCKET;
    }

    freeaddrinfo(addresses);
    if (connection == INVALID_SOCKET) {
        fprintf(stderr, "[-] Could not connect to %s:%u\n", host, (unsigned int)port);
    }
    return connection;
}

int send_request(SOCKET connection, const char *host, const char *path)
{
    char request[512];
    int length = _snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    int sent_total = 0;

    if (length < 0 || (size_t)length >= sizeof(request)) {
        fprintf(stderr, "[-] Request is too large\n");
        return 0;
    }
    while (sent_total < length) {
        int sent = send(connection, request + sent_total, length - sent_total, 0);
        if (sent == SOCKET_ERROR || sent == 0) {
            fprintf(stderr, "[-] send failed: %d\n", WSAGetLastError());
            return 0;
        }
        sent_total += sent;
    }
    return 1;
}

char *receive_response(SOCKET connection)
{
    size_t used = 0;
    size_t capacity = 4096;
    char *buffer = (char *)malloc(capacity);
    if (buffer == NULL) {
        fprintf(stderr, "[-] Response allocation failed\n");
        return NULL;
    }

    for (;;) {
        int received;
        if (used + 2048 + 1 > capacity) {
            size_t next_capacity = capacity * 2;
            char *next;
            if (next_capacity > RESPONSE_LIMIT + 1) {
                next_capacity = RESPONSE_LIMIT + 1;
            }
            if (next_capacity <= capacity) {
                fprintf(stderr, "[-] Response exceeds limit\n");
                free(buffer);
                return NULL;
            }
            next = (char *)realloc(buffer, next_capacity);
            if (next == NULL) {
                fprintf(stderr, "[-] Response growth failed\n");
                free(buffer);
                return NULL;
            }
            buffer = next;
            capacity = next_capacity;
        }
        received = recv(connection, buffer + used, (int)((capacity - used - 1) > 2048 ? 2048 : (capacity - used - 1)), 0);
        if (received == 0) {
            break;
        }
        if (received == SOCKET_ERROR) {
            fprintf(stderr, "[-] recv failed: %d\n", WSAGetLastError());
            free(buffer);
            return NULL;
        }
        used += (size_t)received;
        if (used >= RESPONSE_LIMIT) {
            fprintf(stderr, "[-] Response exceeds limit\n");
            free(buffer);
            return NULL;
        }
    }
    buffer[used] = '\0';
    return buffer;
}

void close_connection(SOCKET connection)
{
    if (connection != INVALID_SOCKET) {
        shutdown(connection, SD_BOTH);
        closesocket(connection);
    }
}

void cleanup_network(void)
{
    WSACleanup();
}
