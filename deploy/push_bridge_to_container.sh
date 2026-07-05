#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 user@IP_DU_CONTENEUR"
  echo
  echo "Required environment variables:"
  echo "  PVE_HOST=https://IP_DU_PROXMOX:8006"
  echo "  PVE_NODE=pve"
  echo "  PVE_TOKEN_ID=esp32-dashboard@pve!metrics"
  echo "  PVE_TOKEN_SECRET=secret"
  echo
  echo "Optional:"
  echo "  BRIDGE_PORT=8080"
  exit 1
fi

REMOTE="$1"
: "${PVE_HOST:?Missing PVE_HOST}"
: "${PVE_NODE:?Missing PVE_NODE}"
: "${PVE_TOKEN_ID:?Missing PVE_TOKEN_ID}"
: "${PVE_TOKEN_SECRET:?Missing PVE_TOKEN_SECRET}"

BRIDGE_PORT="${BRIDGE_PORT:-8080}"
REMOTE_USER="${REMOTE%@*}"
REMOTE_HOST="${REMOTE#*@}"
TMP_ENV="$(mktemp)"
trap 'rm -f "$TMP_ENV"' EXIT

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

remote_exec "$SUDO apt update && $SUDO apt install -y python3 curl && $SUDO mkdir -p /opt/pve-esp32-bridge"
scp bridge/proxmox_bridge.py "$REMOTE:/tmp/proxmox_bridge.py"
scp deploy/pve-esp32-bridge.service "$REMOTE:/tmp/pve-esp32-bridge.service"
remote_exec "$SUDO install -m 755 /tmp/proxmox_bridge.py /opt/pve-esp32-bridge/proxmox_bridge.py && $SUDO install -m 644 /tmp/pve-esp32-bridge.service /etc/systemd/system/pve-esp32-bridge.service"

cat > "$TMP_ENV" <<EOF
PVE_HOST=${PVE_HOST}
PVE_NODE=${PVE_NODE}
PVE_TOKEN_ID=${PVE_TOKEN_ID}
PVE_TOKEN_SECRET=${PVE_TOKEN_SECRET}
PVE_VERIFY_SSL=false
LISTEN_HOST=0.0.0.0
LISTEN_PORT=${BRIDGE_PORT}
EOF

scp "$TMP_ENV" "$REMOTE:/tmp/pve-esp32-bridge.env"
remote_exec "$SUDO install -m 600 /tmp/pve-esp32-bridge.env /etc/pve-esp32-bridge.env && rm -f /tmp/pve-esp32-bridge.env"
remote_exec "$SUDO systemctl daemon-reload && $SUDO systemctl enable --now pve-esp32-bridge && $SUDO systemctl --no-pager status pve-esp32-bridge"

echo
echo "Bridge installed."
echo "Test from your Mac:"
echo "  curl http://${REMOTE_HOST}:${BRIDGE_PORT}/metrics"
