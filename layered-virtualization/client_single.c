/*
 * client_single.c — all sources merged into one translation unit
 * Compile: x86_64-w64-mingw32-gcc client_single.c -o client.exe -lws2_32
 * Run:     wine client.exe
 */

/* ------------------------------------------------------------------ */
/*  System includes                                                     */
/* ------------------------------------------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <winsock2.h>
#include <ws2tcpip.h>

/*
 * memory_win_compat.h content is intentionally omitted here:
 * <winsock2.h> already pulls in <windows.h> via MinGW, which provides
 * SIZE_T, DWORD, BOOL, HANDLE, WINAPI, MEM_* / PAGE_* constants, and
 * all VirtualAlloc/VirtualFree/WriteProcessMemory/GetCurrentProcess
 * declarations with the correct dllimport attributes.
 */

/* ------------------------------------------------------------------ */
/*  memory.h — TestMemory type                                          */
/* ------------------------------------------------------------------ */
typedef struct TestMemory {
    void  *address;
    size_t size;
} TestMemory;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                                */
/* ------------------------------------------------------------------ */
/* network */
int     initialize_network(void);
SOCKET  connect_server(const char *host, unsigned short port);
int     send_request(SOCKET connection, const char *host, const char *path);
char   *receive_response(SOCKET connection);
void    close_connection(SOCKET connection);
void    cleanup_network(void);
/* memory */
int     allocate_test_memory(TestMemory *memory, size_t size);
int     copy_test_data(TestMemory *memory, const char *data, size_t length);
void    inspect_memory(const TestMemory *memory, size_t bytes_to_show);
void    free_test_memory(TestMemory *memory);
/* handler */
int     parse_response(const char *response, size_t *allocation_size,
                       const char **data, size_t *data_length);
int     process_response(const char *response, TestMemory *memory);
/* payload */
void    test_payload(void);

/* ------------------------------------------------------------------ */
/*  network.c                                                           */
/* ------------------------------------------------------------------ */
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
    hints.ai_family   = AF_UNSPEC;
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
    size_t used     = 0;
    size_t capacity = 4096;
    char  *buffer   = (char *)malloc(capacity);
    if (buffer == NULL) {
        fprintf(stderr, "[-] Response allocation failed\n");
        return NULL;
    }

    for (;;) {
        int received;
        if (used + 2048 + 1 > capacity) {
            size_t next_capacity = capacity * 2;
            char  *next;
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
            buffer   = next;
            capacity = next_capacity;
        }
        received = recv(connection, buffer + used,
                        (int)((capacity - used - 1) > 2048 ? 2048 : (capacity - used - 1)), 0);
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

/* ------------------------------------------------------------------ */
/*  memory.c                                                            */
/* ------------------------------------------------------------------ */
int allocate_test_memory(TestMemory *memory, size_t size)
{
    if (memory == NULL || size == 0) {
        return 0;
    }
    memory->address = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (memory->address == NULL) {
        fprintf(stderr, "[-] VirtualAlloc failed: %lu\n", (unsigned long)GetLastError());
        memory->size = 0;
        return 0;
    }
    memory->size = size;
    printf("    address: %p\n    size: %zu bytes\n", memory->address, memory->size);
    return 1;
}

int copy_test_data(TestMemory *memory, const char *data, size_t length)
{
    SIZE_T bytes_written = 0;
    if (memory == NULL || memory->address == NULL || data == NULL || length > memory->size) {
        fprintf(stderr, "[-] Invalid memory copy request\n");
        return 0;
    }
    if (!WriteProcessMemory(GetCurrentProcess(), memory->address, data, length, &bytes_written) ||
        bytes_written != length) {
        fprintf(stderr, "[-] WriteProcessMemory failed: %lu (wrote %llu of %llu bytes)\n",
                (unsigned long)GetLastError(),
                (unsigned long long)bytes_written,
                (unsigned long long)length);
        return 0;
    }
    return 1;
}

void inspect_memory(const TestMemory *memory, size_t bytes_to_show)
{
    size_t i;
    const unsigned char *bytes;
    if (memory == NULL || memory->address == NULL) {
        return;
    }
    if (bytes_to_show > memory->size) {
        bytes_to_show = memory->size;
    }
    bytes = (const unsigned char *)memory->address;
    printf("    memory bytes:");
    for (i = 0; i < bytes_to_show; ++i) {
        printf(" %02X", bytes[i]);
    }
    printf("\n");
}

void free_test_memory(TestMemory *memory)
{
    if (memory != NULL && memory->address != NULL) {
        if (!VirtualFree(memory->address, 0, MEM_RELEASE)) {
            fprintf(stderr, "[-] VirtualFree failed: %lu\n", (unsigned long)GetLastError());
        }
        memory->address = NULL;
        memory->size    = 0;
    }
}

/* ------------------------------------------------------------------ */
/*  payload.c                                                           */
/* ------------------------------------------------------------------ */
void test_payload(void)
{
    printf("Payload test completed\n");
}

/* ------------------------------------------------------------------ */
/*  handler.c                                                           */
/* ------------------------------------------------------------------ */
static const char *find_header_end(const char *response)
{
    const char *end = strstr(response, "\r\n\r\n");
    return end != NULL ? end + 4
         : strstr(response, "\n\n") != NULL ? strstr(response, "\n\n") + 2
         : NULL;
}

int parse_response(const char *response, size_t *allocation_size,
                   const char **data, size_t *data_length)
{
    const char   *body;
    const char   *status;
    const char   *type;
    const char   *size_line;
    const char   *data_line;
    char         *endptr;
    unsigned long parsed_size;

    if (response == NULL || allocation_size == NULL || data == NULL || data_length == NULL) {
        return 0;
    }
    body = find_header_end(response);
    if (body == NULL) {
        fprintf(stderr, "[-] Invalid HTTP response: missing header terminator\n");
        return 0;
    }
    if (strncmp(response, "HTTP/1.", 7) != 0 || strstr(response, " 200 ") == NULL) {
        fprintf(stderr, "[-] HTTP request did not succeed\n");
        return 0;
    }

    status    = strstr(body, "STATUS=READY");
    type      = strstr(body, "TYPE=TEST");
    size_line = strstr(body, "SIZE=");
    data_line = strstr(body, "DATA=");
    if (status == NULL || type == NULL || size_line == NULL || data_line == NULL) {
        fprintf(stderr, "[-] Response is missing required fields\n");
        return 0;
    }
    if ((status    != body && status[-1]    != '\n') ||
        (type      != body && type[-1]      != '\n') ||
        (size_line != body && size_line[-1] != '\n') ||
        (data_line != body && data_line[-1] != '\n')) {
        fprintf(stderr, "[-] Response fields are malformed\n");
        return 0;
    }

    parsed_size = strtoul(size_line + 5, &endptr, 10);
    if (endptr == size_line + 5 || (*endptr != '\r' && *endptr != '\n') ||
        parsed_size == 0 || parsed_size > 1024UL * 1024UL) {
        fprintf(stderr, "[-] Invalid SIZE field\n");
        return 0;
    }
    data_line    += 5;
    *data         = data_line;
    *data_length  = strcspn(data_line, "\r\n");
    if (*data_length > (size_t)parsed_size) {
        fprintf(stderr, "[-] DATA is larger than SIZE\n");
        return 0;
    }
    *allocation_size = (size_t)parsed_size;
    return 1;
}

int process_response(const char *response, TestMemory *memory)
{
    size_t      allocation_size;
    size_t      data_length;
    const char *data;

    printf("[+] Processing data\n");
    if (!parse_response(response, &allocation_size, &data, &data_length)) {
        return 0;
    }
    if (!allocate_test_memory(memory, allocation_size)) {
        return 0;
    }
    printf("[+] Memory allocated\n");
    if (!copy_test_data(memory, data, data_length)) {
        return 0;
    }
    inspect_memory(memory, data_length);
    test_payload();
    printf("[+] Test payload executed\n");
    return 1;
}

/* ------------------------------------------------------------------ */
/*  main.c                                                              */
/* ------------------------------------------------------------------ */
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8000

int main(void)
{
    SOCKET     connection        = INVALID_SOCKET;
    char      *response          = NULL;
    TestMemory memory            = { NULL, 0 };
    int        network_initialized = 0;
    int        exit_code          = 1;

    printf("[+] Client started\n");

    if (!initialize_network()) {
        fprintf(stderr, "[-] Network initialization failed\n");
        goto cleanup;
    }
    network_initialized = 1;
    printf("[+] Network initialized\n");

    connection = connect_server(SERVER_HOST, SERVER_PORT);
    if (connection == INVALID_SOCKET) {
        goto cleanup;
    }

    if (!send_request(connection, SERVER_HOST, "/task")) {
        goto cleanup;
    }
    printf("[+] Request sent\n");

    response = receive_response(connection);
    if (response == NULL) {
        goto cleanup;
    }
    printf("[+] Response received\n");

    if (!process_response(response, &memory)) {
        goto cleanup;
    }

    exit_code = 0;

cleanup:
    close_connection(connection);
    free_test_memory(&memory);
    free(response);
    if (network_initialized) {
        cleanup_network();
    }
    printf("[+] Cleanup complete\n");
    return exit_code;
}
