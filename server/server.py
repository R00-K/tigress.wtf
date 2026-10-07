#!/usr/bin/env python3
"""Small local HTTP server for the MemoryLab client."""

from http.server import BaseHTTPRequestHandler, HTTPServer


HOST = "127.0.0.1"
PORT = 8000
BODY = b"STATUS=READY\nTYPE=TEST\nSIZE=128\nDATA=HELLO_TEST_DATA\n"


class MemoryLabHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != "/task":
            self.send_error(404, "Not Found")
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(BODY)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(BODY)
        print(f"[+] Served {self.path} to {self.client_address[0]}:{self.client_address[1]}")

    def log_message(self, format_string, *args):
        print("[HTTP] " + format_string % args)


def main():
    server = HTTPServer((HOST, PORT), MemoryLabHandler)
    print(f"[+] MemoryLab server listening on http://{HOST}:{PORT}")
    print("[+] Press Ctrl+C to stop")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[+] Server stopping")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
