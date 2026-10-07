# Windows C Client-Server Memory Research Lab

This is a normal, modular Windows x64 C baseline for studying HTTP communication and Windows memory allocation. It has no code transformation or obfuscation. The client accepts only the fixed test response fields, allocates a `VirtualAlloc` region with read/write access, and writes the `DATA` bytes into its own process with `WriteProcessMemory`; it never executes received bytes.

## Run the server

From the `MemoryLab` directory:

```sh
python server/server.py
```

The server listens on `127.0.0.1:8000`. It returns HTTP 200 on `/task`, with a plain text body containing `STATUS=READY`, `TYPE=TEST`, `SIZE=128`, and `DATA=HELLO_TEST_DATA`.

## Build and run the client

See [client/README.md](client/README.md) for the MinGW-w64 and MSVC commands. The commands compile the files in `client/src`; from that directory start the server in another terminal and run:

```sh
client.exe
```

The host and port are constants near the top of `client/src/main.c`.

## Expected client output

```text
[+] Client started
[+] Network initialized
[+] Request sent
[+] Response received
[+] Processing data
    address: 000001...
    size: 128 bytes
[+] Memory allocated
    memory bytes: 48 45 4C 4C 4F 5F 54 45 53 54 5F 44 41 54 41
Payload test completed
[+] Test payload executed
[+] Cleanup complete
```

The allocation address varies by run. The client releases the allocation and closes Winsock during cleanup.

## Function overview

- `main`: runs startup, request, response handling, and cleanup.
- `initialize_network`, `connect_server`, `send_request`, `receive_response`, `close_connection`: Winsock lifecycle and HTTP transport.
- `parse_response`, `process_response`: validates HTTP/test fields, then delegates allocation and payload work.
- `allocate_test_memory`, `copy_test_data`, `inspect_memory`, `free_test_memory`: allocate, populate via `WriteProcessMemory`, display, and release a Windows memory region.
- `test_payload`: isolated harmless console message.

## Debugging workflow

Build with debug symbols and no optimization as shown in `client/README.md`. Run the server, then launch the client under x64dbg. Break on the functions listed there, step through response parsing and `VirtualAlloc`, and inspect the printed address while the program is paused before cleanup. Ghidra can import the resulting PE executable for static analysis.
