#!/usr/bin/env python3
"""Minimal local test fixture: accepts POSTs and appends each body to a
file, so test_http_client.sh can assert on what was actually received.
Not part of the shipped product - test tooling only."""
import sys
import http.server


class Handler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length)
        with open(sys.argv[2], 'ab') as f:
            f.write(body + b'\n')
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'ok')

    def log_message(self, format, *args):
        pass  # keep test output clean


if __name__ == '__main__':
    port = int(sys.argv[1])
    server = http.server.HTTPServer(('127.0.0.1', port), Handler)
    server.serve_forever()
