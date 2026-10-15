#include <stdio.h>
#include "network.h"
#include "handler.h"
#include "memory.h"

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8000

int main(void)
{
    SOCKET connection = INVALID_SOCKET;
    char *response = NULL;
    TestMemory memory = { NULL, 0 };
    int network_initialized = 0;
    int exit_code = 1;

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
