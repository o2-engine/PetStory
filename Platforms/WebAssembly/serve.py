#!/usr/bin/env python3
"""Dev server for the wasm build: no-store cache headers, otherwise Chrome's heuristic
caching keeps serving a stale PetStory.data for hours after relinks."""
import http.server
import sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 8090
directory = sys.argv[2] if len(sys.argv) > 2 else "."


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=directory, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


print(f"Serving HTTP on 0.0.0.0 port {port} (no-store) from {directory}", flush=True)
http.server.ThreadingHTTPServer(("", port), Handler).serve_forever()
