#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 user@IP_DU_CONTENEUR"
  exit 1
fi

REMOTE="$1"
REMOTE_USER="${REMOTE%@*}"
REMOTE_HOST="${REMOTE#*@}"

if [ "$REMOTE_USER" = "root" ]; then
  SUDO=""
else
  SUDO="sudo"
fi

remote_exec() {
  if [ "$REMOTE_USER" = "root" ]; then
    ssh "$REMOTE" "$1"
  else
    ssh -tt "$REMOTE" "$1"
  fi
}

scp bridge/proxmox_bridge.py "$REMOTE:/tmp/proxmox_bridge.py"
remote_exec "$SUDO install -m 755 /tmp/proxmox_bridge.py /opt/pve-esp32-bridge/proxmox_bridge.py && rm -f /tmp/proxmox_bridge.py"
remote_exec "$SUDO systemctl restart pve-esp32-bridge && $SUDO systemctl --no-pager status pve-esp32-bridge"

echo
echo "Bridge code updated."
echo "Test from your Mac:"
echo "  curl http://${REMOTE_HOST}:8080/metrics"
