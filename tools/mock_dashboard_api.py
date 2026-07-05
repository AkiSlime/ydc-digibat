#!/usr/bin/env python3
import json
import math
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


HOST = "0.0.0.0"
PORT = 18080


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        t = time.time()

        if self.path == "/health":
            self.send_json({"ok": True})
            return

        if self.path == "/metrics":
            ram_used_gb = 2.8 + 0.4 * math.sin(t / 9)
            ram_total_gb = 16.0
            storage_used_gb = 62.5
            storage_total_gb = 512.0
            guests = [
                {
                    "type": "vm",
                    "vmid": 100,
                    "name": "agent",
                    "status": "running",
                    "cpu": round(18 + 12 * math.sin(t / 7), 1),
                    "ram": 64.0,
                    "ram_used_gb": 2.6,
                    "ram_total_gb": 4.0,
                    "storage": 41.0,
                    "storage_used_gb": 41.0,
                    "storage_total_gb": 100.0,
                    "uptime": 84000,
                },
                {
                    "type": "lxc",
                    "vmid": 101,
                    "name": "hub",
                    "status": "running",
                    "cpu": round(4 + 2 * math.sin(t / 5), 1),
                    "ram": 38.0,
                    "ram_used_gb": 0.4,
                    "ram_total_gb": 1.0,
                    "storage": 23.0,
                    "storage_used_gb": 1.8,
                    "storage_total_gb": 8.0,
                    "uptime": 54000,
                },
                {
                    "type": "lxc",
                    "vmid": 102,
                    "name": "tools",
                    "status": "running",
                    "cpu": round(7 + 3 * math.sin(t / 11), 1),
                    "ram": 52.0,
                    "ram_used_gb": 1.0,
                    "ram_total_gb": 2.0,
                    "storage": 31.0,
                    "storage_used_gb": 3.1,
                    "storage_total_gb": 10.0,
                    "uptime": 123456,
                },
            ]
            self.send_json(
                {
                    "cpu": round(35 + 25 * math.sin(t / 5), 1),
                    "ram": round(ram_used_gb / ram_total_gb * 100, 1),
                    "ram_used_gb": round(ram_used_gb, 1),
                    "ram_total_gb": ram_total_gb,
                    "storage": round(storage_used_gb / storage_total_gb * 100, 1),
                    "storage_used_gb": storage_used_gb,
                    "storage_total_gb": storage_total_gb,
                    "uptime": 123456,
                    "guests": guests,
                }
            )
            return

        if self.path == "/weather":
            self.send_json(
                {
                    "temperature": round(21.5 + 1.2 * math.sin(t / 12), 1),
                    "humidity": round(48 + 5 * math.sin(t / 15), 1),
                }
            )
            return

        self.send_response(404)
        self.end_headers()

    def send_json(self, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"Mock API running on http://{HOST}:{PORT}")
    print("Endpoints: /health, /metrics, /weather")
    server.serve_forever()


if __name__ == "__main__":
    main()
