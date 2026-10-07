# Windows x64 client

## Build

Run these commands from `client/src`.

MinGW-w64:

```sh
gcc -std=c11 -Wall -Wextra -O0 -g main.c network.c handler.c memory.c payload.c -o client.exe -lws2_32
```

MSVC Developer Command Prompt:

```bat
cl /nologo /W4 /Od /Zi main.c network.c handler.c memory.c payload.c ws2_32.lib
```

The source is ordinary C and keeps networking, parsing, memory management, and the harmless payload in separate functions. `VirtualAlloc` reserves and commits a read/write region, `WriteProcessMemory` copies the received test bytes into the client's own process, and `VirtualFree` releases it. The received bytes are never executed.

## Run

Start the Python server as described in the root README, then run `client.exe`. The client connects to `127.0.0.1:8000` and requests `/task`.

## Debugging

Build with debug information (`-g -O0` or `/Zi /Od`), launch under x64dbg, and set breakpoints on `main`, `initialize_network`, `send_request`, `receive_response`, `process_response`, `allocate_test_memory`, and `test_payload`. Inspect the printed allocation address in the debugger before the cleanup path releases the region. The same executable can be imported into Ghidra for static inspection.
