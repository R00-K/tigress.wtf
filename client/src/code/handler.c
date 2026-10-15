#include "handler.h"
#include "payload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_header_end(const char *response)
{
    const char *end = strstr(response, "\r\n\r\n");
    return end != NULL ? end + 4 : strstr(response, "\n\n") != NULL ? strstr(response, "\n\n") + 2 : NULL;
}

int parse_response(const char *response, size_t *allocation_size,
                   const char **data, size_t *data_length)
{
    const char *body;
    const char *status;
    const char *type;
    const char *size_line;
    const char *data_line;
    char *endptr;
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

    status = strstr(body, "STATUS=READY");
    type = strstr(body, "TYPE=TEST");
    size_line = strstr(body, "SIZE=");
    data_line = strstr(body, "DATA=");
    if (status == NULL || type == NULL || size_line == NULL || data_line == NULL) {
        fprintf(stderr, "[-] Response is missing required fields\n");
        return 0;
    }
    if ((status != body && status[-1] != '\n') || (type != body && type[-1] != '\n') ||
        (size_line != body && size_line[-1] != '\n') || (data_line != body && data_line[-1] != '\n')) {
        fprintf(stderr, "[-] Response fields are malformed\n");
        return 0;
    }

    parsed_size = strtoul(size_line + 5, &endptr, 10);
    if (endptr == size_line + 5 || (*endptr != '\r' && *endptr != '\n') || parsed_size == 0 || parsed_size > 1024UL * 1024UL) {
        fprintf(stderr, "[-] Invalid SIZE field\n");
        return 0;
    }
    data_line += 5;
    *data = data_line;
    *data_length = strcspn(data_line, "\r\n");
    if (*data_length > (size_t)parsed_size) {
        fprintf(stderr, "[-] DATA is larger than SIZE\n");
        return 0;
    }
    *allocation_size = (size_t)parsed_size;
    return 1;
}

int process_response(const char *response, TestMemory *memory)
{
    size_t allocation_size;
    size_t data_length;
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
