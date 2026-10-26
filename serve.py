#!/usr/bin/env python3
"""Tiny local web server for the WebAssembly build (wasm needs http://, not file://).
Usage: python3 serve.py [port]      Author: Sami Khan"""
import http.server, socketserver, sys, os, socket
port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build_web")
if not os.path.exists(os.path.join(root, "index.html")):
    sys.exit("build_web/index.html not found - run build_web.sh first (or use the prebuilt web/ folder).")
os.chdir(root)
class H(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm"}
    def log_message(self, *a): pass
try:
    ip = socket.gethostbyname(socket.gethostname())
except Exception:
    ip = "your-computer-ip"
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(("", port), H) as s:
    print(f"Orbital Dock 3D running:\n  laptop : http://localhost:{port}\n  phone  : http://{ip}:{port}  (same Wi-Fi)\nCtrl+C to stop.")
    try: s.serve_forever()
    except KeyboardInterrupt: pass
