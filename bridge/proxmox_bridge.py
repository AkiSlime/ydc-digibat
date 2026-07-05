#!/usr/bin/env python3
import json
import os
import ssl
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


PVE_HOST = os.environ.get("PVE_HOST", "https://pve.example.local:8006").rstrip("/")
PVE_NODE = os.environ.get("PVE_NODE", "pve")
PVE_TOKEN_ID = os.environ.get("PVE_TOKEN_ID", "")
PVE_TOKEN_SECRET = os.environ.get("PVE_TOKEN_SECRET", "")
PVE_VERIFY_SSL = os.environ.get("PVE_VERIFY_SSL", "true").lower() in ("1", "true", "yes")
LISTEN_HOST = os.environ.get("LISTEN_HOST", "0.0.0.0")
LISTEN_PORT = int(os.environ.get("LISTEN_PORT", "8080"))
GUEST_LIMIT = int(os.environ.get("GUEST_LIMIT", "10"))


def percent(used, total):
    if not total:
        return 0.0
    return round((float(used) / float(total)) * 100.0, 1)


def gib(value):
    return round(float(value or 0) / 1024 / 1024 / 1024, 1)


def fetch_pve(path):
    if not PVE_TOKEN_ID or not PVE_TOKEN_SECRET:
        raise RuntimeError("PVE_TOKEN_ID and PVE_TOKEN_SECRET are required")

    url = f"{PVE_HOST}/api2/json{path}"
    req = Request(
        url,
        headers={"Authorization": f"PVEAPIToken={PVE_TOKEN_ID}={PVE_TOKEN_SECRET}"},
    )

    context = None
    if not PVE_VERIFY_SSL:
        context = ssl._create_unverified_context()

    with urlopen(req, timeout=4, context=context) as response:
        payload = json.loads(response.read().decode("utf-8"))

    return payload["data"]


def normalize_guest(kind, item):
    ram_used = int(item.get("mem", 0))
    ram_total = int(item.get("maxmem", 0))
    disk_used = int(item.get("disk", 0))
    disk_total = int(item.get("maxdisk", 0))

    return {
        "type": kind,
        "vmid": int(item.get("vmid", 0)),
        "name": item.get("name") or f"{kind}-{item.get('vmid', '?')}",
        "status": item.get("status", "unknown"),
        "cpu": round(float(item.get("cpu", 0.0)) * 100.0, 1),
        "ram": percent(ram_used, ram_total),
        "ram_used_bytes": ram_used,
        "ram_total_bytes": ram_total,
        "ram_used_gb": gib(ram_used),
        "ram_total_gb": gib(ram_total),
        "storage": percent(disk_used, disk_total),
        "storage_used_bytes": disk_used,
        "storage_total_bytes": disk_total,
        "storage_used_gb": gib(disk_used),
        "storage_total_gb": gib(disk_total),
        "uptime": int(item.get("uptime", 0)),
    }


def fetch_guests():
    guests = []
    for kind, endpoint in (
        ("vm", f"/nodes/{PVE_NODE}/qemu"),
        ("lxc", f"/nodes/{PVE_NODE}/lxc"),
    ):
        for item in fetch_pve(endpoint):
            guests.append(normalize_guest(kind, item))

    guests.sort(key=lambda guest: guest["vmid"])
    return guests[:GUEST_LIMIT]


def fetch_node_status():
    data = fetch_pve(f"/nodes/{PVE_NODE}/status")
    memory = data.get("memory", {})
    rootfs = data.get("rootfs", {})
    ram_used = int(memory.get("used", 0))
    ram_total = int(memory.get("total", 0))
    storage_used = int(rootfs.get("used", 0))
    storage_total = int(rootfs.get("total", 0))

    return {
        "cpu": round(float(data.get("cpu", 0.0)) * 100.0, 1),
        "ram": percent(ram_used, ram_total),
        "ram_used_bytes": ram_used,
        "ram_total_bytes": ram_total,
        "ram_used_gb": gib(ram_used),
        "ram_total_gb": gib(ram_total),
        "storage": percent(storage_used, storage_total),
        "storage_used_bytes": storage_used,
        "storage_total_bytes": storage_total,
        "storage_used_gb": gib(storage_used),
        "storage_total_gb": gib(storage_total),
        "uptime": int(data.get("uptime", 0)),
        "guests": fetch_guests(),
    }


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/health":
            self.send_json(200, {"ok": True})
            return

        if self.path != "/metrics":
            self.send_json(404, {"error": "not found"})
            return

        try:
            self.send_json(200, fetch_node_status())
        except (HTTPError, URLError, RuntimeError, KeyError, TypeError, ValueError) as exc:
            self.send_json(502, {"error": str(exc)})

    def send_json(self, status, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))


def main():
    server = ThreadingHTTPServer((LISTEN_HOST, LISTEN_PORT), Handler)
    print(f"Serving Proxmox metrics on http://{LISTEN_HOST}:{LISTEN_PORT}/metrics")
    server.serve_forever()


if __name__ == "__main__":
    main()
