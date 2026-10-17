#!/usr/bin/env python3
import socket, sys

PORT   = 8000
BODY   = b"STATUS=READY\nTYPE=TEST\nSIZE=4\nDATA=AAAA\n"
HEADER = (
    b"HTTP/1.1 200 OK\r\n"
    b"Content-Type: text/plain\r\n"
    b"Connection: close\r\n"
    b"Content-Length: " + str(len(BODY)).encode() + b"\r\n"
    b"\r\n"
)
RESPONSE = HEADER + BODY

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    srv.bind(("127.0.0.1", PORT))
except OSError as e:
    print(f"bind failed: {e}", flush=True)
    sys.exit(1)
srv.listen(1)
srv.settimeout(30)
print(f"test server ready on 127.0.0.1:{PORT}", flush=True)
sys.stdout.flush()
try:
    conn, _ = srv.accept()
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = conn.recv(4096)
        if not chunk:
            break
        buf += chunk
    conn.sendall(RESPONSE)
    try:
        conn.shutdown(socket.SHUT_WR)
    except OSError:
        pass
    conn.close()
    print("test server: request served.", flush=True)
except socket.timeout:
    print("test server: timed out waiting for client.", flush=True)
    sys.exit(1)
finally:
    srv.close()
