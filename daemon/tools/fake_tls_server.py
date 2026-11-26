#!/usr/bin/env python3
"""Local TLS test fixture for https_client.c - same role as fake_server.py
but over TLS, using a locally-generated self-signed certificate. Test
tooling only, not part of the shipped product."""
import ssl
import sys
import http.server


class Handler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length)
        with open(sys.argv[4], 'ab') as f:
            f.write(body + b'\n')
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'ok')

    def log_message(self, format, *args):
        pass


if __name__ == '__main__':
    port = int(sys.argv[1])
    certfile = sys.argv[2]
    keyfile = sys.argv[3]

    server = http.server.HTTPServer(('127.0.0.1', port), Handler)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(certfile=certfile, keyfile=keyfile)
    server.socket = ctx.wrap_socket(server.socket, server_side=True)
    server.serve_forever()
