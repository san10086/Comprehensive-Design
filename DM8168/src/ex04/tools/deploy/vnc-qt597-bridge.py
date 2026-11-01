#!/usr/bin/env python3
"""Local RFB 3.3 bridge for the existing Qt 5.9.7 VNC server.

Initialize the server's pixel format explicitly, even if the viewer keeps
the advertised format. Only loopback clients are accepted. No input or
clipboard contents are logged. This does not patch the Qt plugin.
"""

import argparse
import select
import socket
import socketserver
import struct
import time


def log(message):
    print(time.strftime('%H:%M:%S'), message, flush=True)


def receive(sock, size):
    chunks = []
    while size:
        data = sock.recv(size)
        if not data:
            raise ConnectionError('Connection closed during RFB handshake')
        chunks.append(data)
        size -= len(data)
    return b''.join(chunks)


class ClientMessages:
    """Observe message headers; relay bytes without changing them."""

    def __init__(self):
        self.pending = bytearray()
        self.updates = 0
        self.observing = True

    def feed(self, data):
        if not self.observing:
            return
        self.pending.extend(data)
        while self.pending:
            kind = self.pending[0]
            if kind == 0:
                size = 20
            elif kind == 2:
                if len(self.pending) < 4:
                    return
                size = 4 + 4 * struct.unpack('!H', self.pending[2:4])[0]
            elif kind == 3:
                size = 10
            elif kind == 4:
                size = 8
            elif kind == 5:
                size = 6
            elif kind == 6:
                # Do not retain or inspect clipboard content.
                self.pending.clear()
                self.observing = False
                return
            else:
                log('Unparsed client message type=%s; continuing transparent relay' % kind)
                self.pending.clear()
                self.observing = False
                return
            if len(self.pending) < size:
                return
            message = bytes(self.pending[:size])
            del self.pending[:size]
            if kind == 0:
                log('Viewer SetPixelFormat bpp=%s depth=%s trueColor=%s' %
                    (message[4], message[5], message[7]))
            elif kind == 2:
                count = (size - 4) // 4
                encodings = struct.unpack('!%di' % count, message[4:])
                log('Viewer SetEncodings=%s' % (encodings,))
            elif kind == 3:
                self.updates += 1
                if self.updates <= 3:
                    log('Viewer framebuffer request incremental=%s' % message[1])


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        viewer = self.request
        upstream = None
        server_bytes = 0
        log('Viewer connected from loopback port=%s' % self.client_address[1])
        try:
            upstream = socket.create_connection(self.server.target, timeout=8)
            upstream.settimeout(15)
            viewer.settimeout(15)
            version = receive(upstream, 12)
            if version != b'RFB 003.003\n':
                raise ValueError('Expected the existing RFB 3.3 server')
            viewer.sendall(version)
            reply = receive(viewer, 12)
            if reply != version:
                raise ValueError('Viewer did not select RFB 3.3')
            upstream.sendall(reply)
            security = receive(upstream, 4)
            viewer.sendall(security)
            if security != struct.pack('!I', 1):
                raise ValueError('This bridge only supports the existing None security mode')
            upstream.sendall(receive(viewer, 1))
            header = receive(upstream, 24)
            width, height = struct.unpack('!HH', header[:4])
            name_length, = struct.unpack('!I', header[20:24])
            if name_length > 65536 or not (0 < width <= 4096 and 0 < height <= 4096):
                raise ValueError('Unexpected server framebuffer metadata')
            name = receive(upstream, name_length)
            pixel_format = header[4:20]
            if pixel_format[0] not in (8, 16, 32) or pixel_format[3] != 1:
                raise ValueError('Expected a true-color framebuffer')
            # Qt 5.9.7 does not initialize its client pixel format in ServerInit.
            # Send the advertised format before any viewer update request.
            upstream.sendall(b'\x00\x00\x00\x00' + pixel_format)
            log('Initialized Qt pixel format bpp=%s depth=%s frame=%sx%s' %
                (pixel_format[0], pixel_format[1], width, height))
            viewer.sendall(header + name)
            viewer.settimeout(None)
            upstream.settimeout(None)
            observer = ClientMessages()
            while True:
                readable, _, _ = select.select([viewer, upstream], [], [], 30)
                for source in readable:
                    data = source.recv(65536)
                    if not data:
                        log('%s closed; server bytes relayed=%s' %
                            ('Viewer' if source is viewer else 'Qt server', server_bytes))
                        return
                    if source is viewer:
                        observer.feed(data)
                        upstream.sendall(data)
                    else:
                        if not server_bytes:
                            log('First server update received: %s bytes' % len(data))
                        server_bytes += len(data)
                        viewer.sendall(data)
        except (OSError, ValueError) as error:
            log('%s: %s' % (type(error).__name__, error))
        finally:
            if upstream:
                upstream.close()


class Bridge(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', default='192.168.1.21')
    parser.add_argument('--target-port', type=int, default=5900)
    parser.add_argument('--listen-port', type=int, default=5901)
    args = parser.parse_args()
    with Bridge(('127.0.0.1', args.listen_port), Handler) as bridge:
        bridge.target = (args.target, args.target_port)
        log('Listening only on 127.0.0.1:%s -> %s:%s' %
            (args.listen_port, args.target, args.target_port))
        try:
            bridge.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
