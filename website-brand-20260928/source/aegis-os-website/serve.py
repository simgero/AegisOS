#!/usr/bin/env python3
"""Serve the Aegis OS preview locally. Not a production web server."""
from __future__ import annotations
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8000, help='Local port (default: 8000).')
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error('The port must be between 1 and 65535.')
    root = Path(__file__).resolve().parent
    handler = partial(SimpleHTTPRequestHandler, directory=str(root))
    try:
        server = ThreadingHTTPServer(('127.0.0.1', args.port), handler)
    except OSError as error:
        parser.exit(1, f'Could not start the preview server: {error}\n')
    print(f'Aegis OS: http://127.0.0.1:{args.port}\nStop with Ctrl+C.')
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('\nPreview stopped.')
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
