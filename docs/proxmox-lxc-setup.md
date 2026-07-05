# Deployer le bridge Proxmox dans un LXC

Objectif: creer un petit conteneur qui expose une URL simple pour l'ESP32:

```text
http://IP_DU_LXC:8080/metrics
```

Cette URL renvoie:

```json
{"cpu":12.3,"ram":45.6,"storage":78.9,"uptime":123456,"guests":[{"type":"lxc","vmid":101,"name":"ai-buddy-hub","status":"running","cpu":3.2,"ram":42.0}]}
```

Le flux complet est:

```text
ESP32 -> http://IP_DU_LXC:8080/metrics
LXC bridge -> https://IP_DU_PROXMOX:8006/api2/json/nodes/NOM_DU_NODE/status
```

L'ESP32 ne parle pas directement a Proxmox. Il lit seulement le JSON du bridge.

## Ressources LXC recommandees

Ce service est minuscule. Configuration conseillee:

| Ressource | Valeur |
| --- | --- |
| Template | Debian 12 ou Debian 13 |
| Type | LXC non privilegie |
| CPU | 1 coeur |
| RAM | 256 MB |
| Swap | 128 MB |
| Disque | 2 GB |
| Reseau | DHCP avec reservation, ou IP statique |
| Features | rien de special |

128 MB de RAM peuvent suffire, mais 256 MB laisse de la marge pour apt et systemd.

## Creation du LXC via l'interface Proxmox

1. Va dans Proxmox Web UI.
2. Clique sur ton node, puis `Create CT`.
3. General:
   - hostname: `esp32-bridge`
   - unprivileged container: active
4. Template:
   - Debian 12 ou Debian 13
5. Disk:
   - 2 GB
6. CPU:
   - 1 core
7. Memory:
   - 256 MB
   - swap: 128 MB
8. Network:
   - bridge: souvent `vmbr0`
   - IPv4: DHCP ou statique
9. DNS:
   - garde les valeurs par defaut si ton LAN fonctionne deja
10. Finish, puis demarre le conteneur.

Note l'IP du LXC. Exemple:

```text
192.168.1.50
```

Dans ce cas, l'ESP32 devra utiliser:

```cpp
#define PROXMOX_METRICS_URL "http://192.168.1.50:8080/metrics"
```

## Creer le token Proxmox

Dans Proxmox Web UI:

1. Va dans `Datacenter > Permissions > Users`.
2. Cree un utilisateur:
   - User name: `esp32-dashboard`
   - Realm: `Proxmox VE authentication server`
3. Va dans `Datacenter > Permissions > API Tokens`.
4. Ajoute un token:
   - User: `esp32-dashboard@pve`
   - Token ID: `metrics`
   - Privilege Separation: active
5. Copie le secret du token immediatement.
6. Va dans `Datacenter > Permissions`.
7. Ajoute une permission:
   - Path: `/nodes/NOM_DU_NODE`
   - User/Token: `esp32-dashboard@pve!metrics`
   - Role: `PVEAuditor`

Si tu ne sais pas encore le nom du node, regarde la colonne de gauche dans Proxmox. C'est souvent `pve`, mais pas toujours.

Le token complet aura cette forme:

```text
esp32-dashboard@pve!metrics
```

## Installer le bridge dans le LXC

Dans la console du LXC:

```sh
apt update
apt install -y python3 curl
mkdir -p /opt/pve-esp32-bridge
```

Copie ces fichiers du projet vers le LXC:

```text
bridge/proxmox_bridge.py -> /opt/pve-esp32-bridge/proxmox_bridge.py
deploy/pve-esp32-bridge.service -> /etc/systemd/system/pve-esp32-bridge.service
deploy/proxmox-bridge.env.example -> /etc/pve-esp32-bridge.env
```

Puis edite:

```sh
nano /etc/pve-esp32-bridge.env
```

Exemple si:

- IP Proxmox host: `192.168.1.20`
- nom du node: `pve`
- IP du LXC bridge: `192.168.1.50`

```env
PVE_HOST=https://192.168.1.20:8006
PVE_NODE=pve
PVE_TOKEN_ID=esp32-dashboard@pve!metrics
PVE_TOKEN_SECRET=LE_SECRET_COPIE_DEPUIS_PROXMOX
PVE_VERIFY_SSL=false
LISTEN_HOST=0.0.0.0
LISTEN_PORT=8080
```

Active le service:

```sh
systemctl daemon-reload
systemctl enable --now pve-esp32-bridge
systemctl status pve-esp32-bridge
```

## Tester depuis le LXC

Dans le LXC:

```sh
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/metrics
```

Resultat attendu:

```json
{"cpu":12.3,"ram":45.6,"storage":78.9,"uptime":123456,"guests":[{"type":"lxc","vmid":101,"name":"ai-buddy-hub","status":"running","cpu":3.2,"ram":42.0}]}
```

## Tester depuis ton Mac

Depuis ton Mac ou une autre machine du LAN:

```sh
curl http://IP_DU_LXC:8080/metrics
```

Si ca marche depuis le Mac, l'ESP32 pourra lire la meme URL.

## Configurer l'ESP32

Dans `include/config.h`:

```cpp
#define PROXMOX_METRICS_URL "http://IP_DU_LXC:8080/metrics"
```

Exemple:

```cpp
#define PROXMOX_METRICS_URL "http://192.168.1.50:8080/metrics"
```

Puis recompile et upload:

```sh
pio run --target upload
```

## Diagnostic rapide

Voir les logs du bridge:

```sh
journalctl -u pve-esp32-bridge -f
```

Erreurs typiques:

- `PVE_TOKEN_ID and PVE_TOKEN_SECRET are required`: le fichier `/etc/pve-esp32-bridge.env` est incomplet.
- `401` ou `403`: token faux ou permission insuffisante.
- erreur SSL: mets `PVE_VERIFY_SSL=false` si tu utilises le certificat Proxmox par defaut.
- `Connection refused`: mauvais `PVE_HOST`, mauvais port, ou API Proxmox inaccessible depuis le LXC.
- l'ESP32 affiche `http -1` ou `wifi`: probleme Wi-Fi ou URL `PROXMOX_METRICS_URL` inaccessible depuis l'ESP32.
