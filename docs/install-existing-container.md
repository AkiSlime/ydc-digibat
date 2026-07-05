# Installer le bridge dans ton conteneur existant

Ce guide part du principe que:

- tu as deja un conteneur hub;
- tu peux te connecter en SSH dessus avec `root` ou avec un user qui a `sudo`;
- tu lances les commandes depuis ton Mac, dans le dossier du projet `double_dashboard`.

Regle simple:

- les commandes `ssh`, `scp`, `chmod +x deploy/...` et `./deploy/push_bridge_to_container.sh` se lancent sur ton Mac;
- les commandes `apt`, `systemctl`, `journalctl` tournent dans le conteneur, mais le script les lance pour toi a distance via SSH, avec `sudo` si tu n'utilises pas `root`;
- le token se cree dans l'interface web Proxmox, pas dans le terminal.

## 1. Remplace ces valeurs

Dans les commandes ci-dessous, remplace:

- `IP_DU_CONTENEUR` par l'IP de ton conteneur hub;
- `IP_DU_PROXMOX` par l'IP de ton serveur Proxmox;
- `pve` par le vrai nom de ton node Proxmox si different;
- `LE_SECRET_DU_TOKEN` par le secret du token Proxmox.

Exemple:

```text
IP_DU_CONTENEUR = 192.168.1.50
IP_DU_PROXMOX = 192.168.1.20
PVE_NODE = pve
```

## 2. Creer le token dans Proxmox

Le secret du token vient de l'interface web Proxmox. Il est affiche une seule fois au moment de la creation.

Dans Proxmox Web UI:

1. Ouvre `https://IP_DU_PROXMOX:8006`.
2. Va dans `Datacenter > Permissions > Users`.
3. Clique `Add`.
4. Cree l'utilisateur:
   - User name: `esp32-dashboard`
   - Realm: `Proxmox VE authentication server`
   - Password: mets un mot de passe fort, meme si on utilisera surtout le token
5. Va dans `Datacenter > Permissions > API Tokens`.
6. Clique `Add`.
7. Mets:
   - User: `esp32-dashboard@pve`
   - Token ID: `metrics`
   - Privilege Separation: active
8. Valide.
9. Proxmox affiche un secret. Copie-le tout de suite.

Tu obtiens alors deux valeurs:

```text
PVE_TOKEN_ID=esp32-dashboard@pve!metrics
PVE_TOKEN_SECRET=le_secret_affiche_par_proxmox
```

Ensuite donne le droit de lecture au token:

1. Va dans `Datacenter > Permissions`.
2. Clique `Add > API Token Permission`.
3. Mets:
   - Path: `/nodes/NOM_DU_NODE`
   - API Token: `esp32-dashboard@pve!metrics`
   - Role: `PVEAuditor`
   - Propagate: active

Si ton node s'appelle `pve`, le path est:

```text
/nodes/pve
```

## 3. Verifier que le conteneur repond en SSH

Depuis ton Mac:

```sh
ssh TON_USER@IP_DU_CONTENEUR
```

Si tu arrives dans le shell du conteneur, tape:

```sh
exit
```

Ensuite verifie que ce user a sudo:

```sh
ssh -t TON_USER@IP_DU_CONTENEUR "sudo -v"
```

Si ca demande ton mot de passe puis revient sans erreur, c'est bon.

Si tu as `TON_USER is not in the sudoers file`, ouvre la console du conteneur depuis Proxmox et lance dans le conteneur:

```sh
apt update
apt install -y sudo
usermod -aG sudo TON_USER
```

Ensuite ferme puis rouvre ta session SSH:

```sh
ssh TON_USER@IP_DU_CONTENEUR
exit
```

## 4. Installer automatiquement le bridge

Cette commande se lance sur ton Mac, pas dans le conteneur.

Pourquoi: le script utilise `ssh` et `scp` pour installer les fichiers dans le conteneur a ta place.

Depuis ton Mac, dans le dossier du projet:

```sh
cd /Users/aki/DEV_REPO/double_dashboard
chmod +x deploy/push_bridge_to_container.sh
```

Puis lance ceci en remplacant les valeurs:

```sh
PVE_HOST="https://IP_DU_PROXMOX:8006" \
PVE_NODE="pve" \
PVE_TOKEN_ID='esp32-dashboard@pve!metrics' \
PVE_TOKEN_SECRET='LE_SECRET_DU_TOKEN' \
BRIDGE_PORT="8080" \
./deploy/push_bridge_to_container.sh TON_USER@IP_DU_CONTENEUR
```

Si ton terminal affiche `dquote>`, tu as un guillemet double non ferme. Appuie sur `Ctrl+C`, puis utilise plutot cette version en plusieurs lignes simples:

```sh
export PVE_HOST='https://IP_DU_PROXMOX:8006'
export PVE_NODE='pve'
export PVE_TOKEN_ID='esp32-dashboard@pve!metrics'
export PVE_TOKEN_SECRET='LE_SECRET_DU_TOKEN'
export BRIDGE_PORT='8080'
./deploy/push_bridge_to_container.sh TON_USER@IP_DU_CONTENEUR
```

Exemple concret:

```sh
PVE_HOST="https://192.168.1.20:8006" \
PVE_NODE="pve" \
PVE_TOKEN_ID='esp32-dashboard@pve!metrics' \
PVE_TOKEN_SECRET='xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx' \
BRIDGE_PORT="8080" \
./deploy/push_bridge_to_container.sh aki@192.168.1.50
```

Les quotes simples autour de `PVE_TOKEN_ID` sont importantes avec `zsh`: sinon le `!metrics` est interprete comme une commande d'historique et tu obtiens `zsh: event not found: metrics`.

Le script fait tout ca:

- installe `python3` et `curl`;
- copie `bridge/proxmox_bridge.py`;
- copie le service systemd;
- cree `/etc/pve-esp32-bridge.env`;
- active et demarre le service.

Si tu utilises un user non-root, le script ouvre un terminal SSH interactif pour `sudo`. Il peut donc te demander ton mot de passe pendant l'installation. C'est normal.

## 5. Tester depuis ton Mac

Toujours depuis ton Mac:

```sh
curl http://IP_DU_CONTENEUR:8080/health
curl http://IP_DU_CONTENEUR:8080/metrics
```

Exemple:

```sh
curl http://192.168.1.50:8080/health
curl http://192.168.1.50:8080/metrics
```

Resultat attendu:

```json
{"cpu":12.3,"ram":45.6,"ram_used_gb":7.3,"ram_total_gb":16.0,"storage":78.9,"storage_used_gb":120.5,"storage_total_gb":512.0,"uptime":123456,"guests":[{"type":"lxc","vmid":101,"name":"ai-buddy-hub","status":"running","cpu":3.2,"ram":42.0,"ram_used_gb":0.4,"ram_total_gb":1.0,"storage":22.5,"storage_used_gb":1.8,"storage_total_gb":8.0,"uptime":123456}]}
```

Les valeurs seront differentes, mais les champs doivent exister.

## 6. Configurer l'ESP32

Dans `include/config.h`, mets:

```cpp
#define PROXMOX_METRICS_URL "http://IP_DU_CONTENEUR:8080/metrics"
```

Exemple:

```cpp
#define PROXMOX_METRICS_URL "http://192.168.1.50:8080/metrics"
```

Puis upload:

```sh
pio run --target upload
```

## 7. Si le port 8080 est deja pris

Dans le conteneur:

```sh
ss -lntp
```

Si tu vois deja `:8080`, utilise `18080`.

Installation:

```sh
PVE_HOST="https://IP_DU_PROXMOX:8006" \
PVE_NODE="pve" \
PVE_TOKEN_ID='esp32-dashboard@pve!metrics' \
PVE_TOKEN_SECRET='LE_SECRET_DU_TOKEN' \
BRIDGE_PORT="18080" \
./deploy/push_bridge_to_container.sh TON_USER@IP_DU_CONTENEUR
```

Config ESP32:

```cpp
#define PROXMOX_METRICS_URL "http://IP_DU_CONTENEUR:18080/metrics"
```

## 8. Voir les logs

Depuis ton Mac:

```sh
ssh TON_USER@IP_DU_CONTENEUR "sudo journalctl -u pve-esp32-bridge -f"
```

Voir le statut:

```sh
ssh TON_USER@IP_DU_CONTENEUR "sudo systemctl --no-pager status pve-esp32-bridge"
```

Redemarrer:

```sh
ssh TON_USER@IP_DU_CONTENEUR "sudo systemctl restart pve-esp32-bridge"
```

## 9. Erreur `Name or service not known`

Si `/health` marche mais `/metrics` renvoie:

```json
{"error": "<urlopen error [Errno -2] Name or service not known>"}
```

Le service bridge tourne, mais `PVE_HOST` pointe vers un nom ou une valeur que le conteneur ne sait pas resoudre. Verifie la config sans afficher le secret:

```sh
ssh TON_USER@IP_DU_CONTENEUR "sudo grep -E '^(PVE_HOST|PVE_NODE|LISTEN_PORT)=' /etc/pve-esp32-bridge.env"
```

Puis remplace `PVE_HOST` par l'IP reelle de ton serveur Proxmox:

```sh
ssh -t TON_USER@IP_DU_CONTENEUR "sudo sed -i 's|^PVE_HOST=.*|PVE_HOST=https://IP_DU_PROXMOX:8006|' /etc/pve-esp32-bridge.env && sudo systemctl restart pve-esp32-bridge"
```

Teste ensuite:

```sh
curl http://IP_DU_CONTENEUR:8080/metrics
```

## 10. Erreur `403 Permission check failed`

Si `/metrics` renvoie:

```json
{"error": "HTTP Error 403: Permission check failed (/nodes/pve, Sys.Audit)"}
```

Le bridge arrive bien a joindre Proxmox, mais le token n'a pas le droit de lire le statut du node.

Dans Proxmox Web UI:

1. Va dans `Datacenter > Permissions`.
2. Clique `Add > User Permission`.
3. Mets:
   - Path: `/nodes/pve`
   - User: `esp32-dashboard@pve`
   - Role: `PVEAuditor`
   - Propagate: active
4. Clique encore `Add > API Token Permission`.
5. Mets:
   - Path: `/nodes/pve`
   - API Token: `esp32-dashboard@pve!metrics`
   - Role: `PVEAuditor`
   - Propagate: active

Si ton node ne s'appelle pas `pve`, remplace `/nodes/pve` par le vrai nom, par exemple `/nodes/proxmox01`.

Puis redemarre le bridge:

```sh
ssh TON_USER@IP_DU_CONTENEUR "sudo systemctl restart pve-esp32-bridge"
```

Et teste:

```sh
curl http://IP_DU_CONTENEUR:8080/metrics
```

## 11. Mettre a jour le bridge deja installe

Quand `bridge/proxmox_bridge.py` change dans ce projet, relance simplement le script d'installation depuis ton Mac. Il remplace le fichier Python dans le conteneur et redemarre le service.

Si tu veux seulement mettre a jour le code sans retaper le token ni reecrire `/etc/pve-esp32-bridge.env`, utilise:

```sh
cd /Users/aki/DEV_REPO/double_dashboard
chmod +x deploy/update_bridge_code.sh
./deploy/update_bridge_code.sh TON_USER@IP_DU_CONTENEUR
```

Exemple:

```sh
./deploy/update_bridge_code.sh aki@192.168.1.41
```

Reinstallation complete, si tu veux aussi reecrire `/etc/pve-esp32-bridge.env`:

```sh
cd /Users/aki/DEV_REPO/double_dashboard
export PVE_HOST='https://IP_DU_PROXMOX:8006'
export PVE_NODE='pve'
export PVE_TOKEN_ID='esp32-dashboard@pve!metrics'
export PVE_TOKEN_SECRET='LE_SECRET_DU_TOKEN'
export BRIDGE_PORT='8080'
./deploy/push_bridge_to_container.sh TON_USER@IP_DU_CONTENEUR
```

Puis teste:

```sh
curl http://IP_DU_CONTENEUR:8080/metrics
```
