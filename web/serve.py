#!/usr/bin/env python3
"""Offline localhost-only player. No installation or network downloads."""
from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
from pathlib import Path
import os,webbrowser
os.chdir(Path(__file__).resolve().parent)
class Handler(SimpleHTTPRequestHandler):
 def end_headers(self):
  self.send_header('Cache-Control','no-cache');super().end_headers()
server=ThreadingHTTPServer(('127.0.0.1',8768),Handler)
url=f'http://127.0.0.1:{server.server_port}/';print(url,flush=True)
webbrowser.open(url)
try:server.serve_forever()
except KeyboardInterrupt:pass
finally:server.server_close()
