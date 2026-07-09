# Admin Dashboard ESP32

Dashboard ESP32 NodeMCU-32S / ESP-32S Kit avec un affichage OLED:

- page d'accueil Tamagotchi avec une chauve-souris pixel-art animee;
- heure, temperature, alertes repas/chaud/connexion et jauges du compagnon;
- pages details pour Proxmox, reseau, VMs et conteneurs LXC;
- mises a jour OTA par Wi-Fi apres le premier flash USB.

Le LCD HD44780 16 pins a ete retire du firmware. L'ancien cablage est conserve dans `docs/legacy-lcd.md`.

## Recommandation d'architecture

Ne fais pas de SSH depuis l'ESP32 vers Proxmox. C'est fragile, lourd, et ca t'oblige a embarquer des secrets sensibles dans le microcontroleur.

La meilleure V1 est:

1. Proxmox expose ses infos via son API locale avec un token limite.
2. Un petit bridge tourne sur le LAN, idealement sur Proxmox ou une VM/LXC.
3. L'ESP32 appelle ce bridge en HTTP et recoit un JSON simple.
4. Le deuxieme ESP32 expose les infos meteo en HTTP JSON.

Pour ce projet, PlatformIO avec le framework Arduino est le meilleur compromis. ESP-IDF est plus professionnel pour un produit final, mais ici Arduino te donne plus vite les bibliotheques stables pour OLED, Wi-Fi et HTTP. Tu pourras migrer vers ESP-IDF ensuite si tu veux ajouter FreeRTOS propre, MQTT robuste, OTA, logs structures, watchdog avance, etc.

## Cablage propose

Le pinout vise explicitement ton modele `ESP32 NODEMCU-32S ESP-32S Kit`.

Pins volontairement evitees:

- GPIO6 a GPIO11: reservees a la flash interne, ne pas cabler.
- GPIO0, GPIO2, GPIO12, GPIO15: pins de boot strapping, a eviter pour une V1 stable.
- GPIO1 et GPIO3: port serie USB, a garder libres pour les logs et l'upload.
- GPIO34 a GPIO39: entrees uniquement. GPIO34 est donc parfait pour lire un potentiometre, mais pas pour piloter un ecran.

Les pins utilisees par le projet sont compatibles avec ce modele:

- I2C OLED: GPIO22 SDA, GPIO23 SCL/SCK.
- Boutons menu: GPIO25, GPIO33, GPIO32.
- LED rouge: GPIO27.
- Potentiometre utilisateur: GPIO34.

### OLED I2C 1.3"

La plupart des OLED 1.3" sont en SH1106 128x64.

| OLED | ESP32 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 22 |
| SCL/SCK | GPIO 23 |

Si ton ecran est SSD1306 au lieu de SH1106, il faudra seulement changer le constructeur U8g2 dans `src/main.cpp`.

## Test diagnostic OLED seul

Si l'OLED reste noir, teste d'abord l'ecran sans Wi-Fi et sans API:

```sh
pio run -e oled-test --target upload
pio device monitor
```

Resultat attendu sur l'OLED:

```text
OLED TEST
SDA22 SCK23
SH1106 SW I2C
```

Le firmware initialise le bus I2C avec `SDA=GPIO22` et `SCK/SCL=GPIO23`.

Si l'ecran reste noir avec ce test:

- verifie que `SDA` OLED va bien sur `GPIO22`;
- verifie que `SCK/SCL` OLED va bien sur `GPIO23`;
- verifie `VCC -> 3V3` et `GND -> GND`;
- teste l'autre adresse/controleur possible: certains OLED 1.3" sont `SSD1306` au lieu de `SH1106`;
- garde en tete qu'un OLED n'a pas de retroeclairage: noir complet veut dire qu'il ne recoit pas/execute pas les commandes d'affichage.

### Boutons menu

Les trois boutons pilotent les pages OLED et le menu Tamagotchi:

- GPIO25: page suivante;
- GPIO32: page precedente;
- select: ouvrir/valider le menu Tamagotchi sur la page d'accueil.

| Bouton | ESP32 |
| --- | --- |
| Suivant | GPIO 25 |
| Select / OK | GPIO 33 |
| Precedent | GPIO 32 |

Cablage: une patte du bouton sur le GPIO, l'autre sur GND. Le firmware utilise `INPUT_PULLUP`, donc un bouton appuye lit `LOW`.

Pages disponibles:

- page 1: accueil Tamagotchi avec temperature, action en cours, niveau et jauges energie/faim/nourriture;
- page 2: details Proxmox avec CPU, RAM, disque et uptime;
- page 3: details reseau avec RSSI Wi-Fi, IP ESP32, age des dernieres donnees PVE/meteo et statut OTA.
- pages suivantes: une page par VM ou conteneur LXC renvoye par Proxmox, avec CPU, RAM, disque et uptime.

Depuis la page d'accueil, `Select` ouvre le menu vertical plein ecran:

- bouton suivant: descend dans le menu;
- bouton precedent: monte dans le menu;
- `Select`: valide l'entree selectionnee;
- `STATS`: affiche la fiche personnage;
- `SLEEP`: met le compagnon au sommeil;
- `EAT`: ouvre le choix du nombre de repas utiles;
- `HUNT`: ouvre le choix du nombre de chasses de 20 minutes;
- `ARENA`: ouvre le choix du nombre de runs de combats automatiques pour gagner de l'XP;
- `SCREEN OFF`: eteint l'OLED;
- `BACK`: ferme le menu.

Pendant une activite, `Select` ouvre un menu d'activite avec `STATS`,
`SCREEN OFF` et `BACK`. Pendant `HUNT`, ce menu ajoute `STOP`. Pendant `SLEEP`,
il ajoute `WAKE UP`.

Quand l'ecran est eteint, n'importe quel bouton le rallume.

La page d'accueil reste disponible meme si le Wi-Fi ou le bridge Proxmox est KO. Dans ce cas, la chauve-souris affiche une bulle `NET KO`. La station meteo ne bloque pas le dashboard; si elle est indisponible, la temperature affiche des tirets jusqu'au premier retour valide.

### LED rouge

La LED rouge sert aux alertes simples du compagnon.

| LED rouge | ESP32 |
| --- | --- |
| GPIO | GPIO 27 |
| Cathode | GND |

Cablage recommande:

```text
GPIO27 -> resistance -> anode LED
cathode LED -> GND
```

Une resistance `220 ohms` fonctionne. Avec une LED rouge autour de `2.0V`, le courant sera environ `(3.3V - 2.0V) / 220 = 5.9 mA`, ce qui est raisonnable pour un GPIO ESP32. `330 ohms` marche aussi et sera un peu plus doux, environ `4 mA`.

Comportement actuel:

- le compagnon demarre avec `food = 1`, `hunger = 50`, `energy = 50`;
- en `IDLE`, la faim baisse de `2` par heure;
- en `IDLE` sans alerte, le panneau d'activite affiche `chill`;
- `HUNT` dure `20 min`, coute `2` energie, coute `0..2` faim et peut rapporter `0`, `1`, `2` ou `4` nourritures;
- les chances de base de `HUNT` sont `+0 20%`, `+1 48%`, `+2 24%`, `+4 8%`;
- les stats `ATK`, `DEF` et `LCK` peuvent ameliorer les resultats de chasse;
- `EAT` dure `3 min`, consomme `1` nourriture et remonte la faim de `20`;
- pendant `HUNT` et `EAT`, la zone d'activite affiche l'action et le temps restant;
- `ARENA` coute `5` energie et `5` faim, resout un combat automatique toutes les `2 min`, affiche les wins en cours et se termine quand les HP du run tombent a `0`;
- a la fin de `ARENA`, une fenetre affiche `WINS`, `XP` et `LV` jusqu'a appui sur `Select`;
- les files `HUNT`, `EAT` et `ARENA` se lancent une action a la fois depuis un selecteur numerique;
- la progression demarre a `LV 1`, `XP 0/10`, `HP 10`, `ATK 2`, `DEF 1`, `LCK 1`;
- le niveau courant s'affiche au-dessus des jauges `E`, `H` et `F` sur l'accueil;
- le stock de nourriture est affiche sur l'accueil sous forme de jauge `F`;
- `SLEEP` remonte l'energie de `10` par heure, baisse la faim de `3` par heure et reveille automatiquement a `08:00`;
- si la temperature passe a `33.0 C`, une notification `HOT` s'affiche pendant `PET_NOTICE_MS`;
- pendant la notification `HOT`, la LED rouge s'allume, puis s'eteint quand la notification est finie;
- quand `HUNT`, `EAT` ou `ARENA` se termine normalement, la LED rouge joue un court motif non bloquant;
- l'alerte chaud peut se relancer apres etre repassee sous `32.0 C`;
- une perte Wi-Fi ou bridge affiche aussi une notification temporaire `NET KO`.

Document gameplay:

- la refonte `ARENA` / progression / `HUNT` est decrite dans `docs/pet-arena-progression-refactor.md`;
- le firmware implemente maintenant une premiere version de cette refonte.

### Potentiometre utilisateur optionnel

Si tu veux un potentiometre utilisateur:

| Potentiometre | ESP32 |
| --- | --- |
| extremite 1 | 3V3 |
| extremite 2 | GND |
| curseur | GPIO 34 |

Le firmware declare deja `USER_POT_PIN`, mais ne l'utilise pas encore.

## Configuration firmware

Le firmware utilise `include/config.h` s'il existe, sinon `include/config.example.h`.

Renseigne dans `include/config.h`:

```cpp
#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."
#define PROXMOX_METRICS_URL "http://IP_DU_BRIDGE:8080/metrics"
#define WEATHER_URL "http://IP_DU_SECOND_ESP32/api"
#define OTA_HOSTNAME "admin-dashboard"
#define OTA_PASSWORD "MOT_DE_PASSE_OTA"
```

Important: `PROXMOX_METRICS_URL` n'est pas l'URL de l'interface web Proxmox et ce n'est pas l'URL brute de l'API Proxmox. C'est l'URL du bridge `proxmox_bridge.py`, qui lui-meme va interroger Proxmox.

En reel, si ton serveur Proxmox a l'IP `192.168.1.20` et que tu fais tourner le bridge dessus:

```cpp
#define PROXMOX_METRICS_URL "http://192.168.1.20:8080/metrics"
```

Le Mac ne sert que pour le test mock. En usage normal, il ne sert a rien.

### Parametres Tamagotchi

Les horaires et seuils sont configurables dans `include/config.h`:

```cpp
#define PET_TIMEZONE "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define PET_WAKE_HOUR 8
#define PET_WAKE_MINUTE 0
#define PET_HOT_ALERT_C 33.0f
#define PET_HOT_CLEAR_C 32.0f
#define PET_NOTICE_MS 3000
#define PET_INITIAL_FOOD 1
#define PET_INITIAL_HUNGER 50
#define PET_INITIAL_ENERGY 50
#define PET_MAX_FOOD 10
#define PET_HUNT_MIN_ENERGY 2
#define PET_HUNT_ENERGY_COST 2
#define PET_HUNT_MAX_HUNGER_COST 2
#define PET_EAT_HUNGER_GAIN 20
#define PET_EAT_MAX_ENERGY_COST 5
#define PET_IDLE_HUNGER_LOSS_PER_HOUR 2
#define PET_SLEEP_ENERGY_GAIN_PER_HOUR 10
#define PET_SLEEP_HUNGER_LOSS_PER_HOUR 3
#define PET_HUNT_DURATION_MS (20UL * 60UL * 1000UL)
#define PET_EAT_DURATION_MS (3UL * 60UL * 1000UL)
#define PET_STAT_TICK_MS (60UL * 60UL * 1000UL)
#define PET_INITIAL_LEVEL 1
#define PET_INITIAL_XP 0
#define PET_INITIAL_HP 10
#define PET_INITIAL_ATK 2
#define PET_INITIAL_DEF 1
#define PET_INITIAL_LCK 1
#define PET_MAX_LEVEL 99
#define PET_ARENA_MIN_ENERGY 5
#define PET_ARENA_ENERGY_COST 5
#define PET_ARENA_HUNGER_COST 5
#define PET_ARENA_COMBAT_INTERVAL_MS (2UL * 60UL * 1000UL)
#define PET_ARENA_XP_PERCENT_AT_TARGET 35
```

La timezone ci-dessus correspond a la France metropolitaine avec changement
heure ete/hiver. L'ESP32 utilise NTP pour les routines horaires.

## Mise a jour OTA par Wi-Fi

L'OTA permet d'envoyer les futures mises a jour sans reconnecter l'ESP32 en USB.
Il faut tout de meme faire un premier flash USB avec le firmware OTA.

### 1. Configurer le mot de passe OTA

Dans `include/config.h`, renseigne:

```cpp
#define OTA_HOSTNAME "admin-dashboard"
#define OTA_PASSWORD "MOT_DE_PASSE_OTA_SOLIDE"
```

Le hostname sert a joindre l'ESP32 via mDNS: `admin-dashboard.local`.

### 2. Premier flash en USB

Branche l'ESP32 en USB, puis lance:

```sh
pio run -e nodemcu-32s --target upload
pio device monitor
```

Quand le Wi-Fi est connecte, le moniteur serie doit afficher:

```text
OTA ready: admin-dashboard
```

### 3. Uploads suivants par Wi-Fi

L'ESP32 doit rester allume et connecte au meme Wi-Fi que ton Mac.

Commande rapide recommandee si `pio` n'est pas dans le `PATH`:

```sh
OTA_PASSWORD='MOT_DE_PASSE_OTA_SOLIDE' /Users/aki/.platformio/penv/bin/pio run -e nodemcu-32s-ota --target upload
```

Si `pio` fonctionne directement dans ton terminal, la version courte est:

```sh
OTA_PASSWORD='MOT_DE_PASSE_OTA_SOLIDE' pio run -e nodemcu-32s-ota --target upload
```

Tu peux aussi exporter le mot de passe une seule fois dans le terminal courant:

```sh
export OTA_PASSWORD='MOT_DE_PASSE_OTA_SOLIDE'
/Users/aki/.platformio/penv/bin/pio run -e nodemcu-32s-ota --target upload
```

Si `admin-dashboard.local` ne resout pas sur ton reseau, remplace temporairement
`upload_port` dans `platformio.ini` par l'adresse IP de l'ESP32:

```ini
upload_port = 192.168.1.xx
```

Si l'upload reste bloque sur `Sending invitation to 192.168.1.xx`, PlatformIO
arrive a lancer l'envoi mais l'ESP32 ne repond pas encore au service OTA. Verifie
dans cet ordre:

- le firmware actuellement flashe sur l'ESP32 contient bien OTA; le tout premier
  firmware OTA doit etre envoye en USB;
- le moniteur serie affiche bien `OTA ready: admin-dashboard` apres la connexion
  Wi-Fi;
- sans USB, va sur la page `NETWORK` de l'OLED et verifie la ligne `OTA:`:
  `ON :3232` veut dire que le service OTA ecoute, `OFF WIFI` indique que le
  Wi-Fi n'est pas connecte, et `OFF INIT` indique que l'OTA n'est pas encore
  initialise;
- l'adresse IP utilisee est bien celle de l'ESP32 dashboard, pas celle du second
  ESP32 meteo ni celle du bridge;
- le Mac et l'ESP32 sont sur le meme reseau Wi-Fi/VLAN, sans isolation client;
- le mot de passe `OTA_PASSWORD` du terminal correspond au firmware deja flashe.

Pour une utilisation stable, reserve l'adresse IP de l'ESP32 dans ta box ou ton
routeur.

## Format JSON attendu

Bridge Proxmox:

```json
{
  "cpu": 12.3,
  "ram": 45.6,
  "ram_used_gb": 7.3,
  "ram_total_gb": 16.0,
  "storage": 78.9,
  "storage_used_gb": 120.5,
  "storage_total_gb": 512.0,
  "uptime": 123456,
  "guests": [
    {
      "type": "lxc",
      "vmid": 101,
      "name": "ai-buddy-hub",
      "status": "running",
      "cpu": 3.2,
      "ram": 42.0,
      "ram_used_gb": 0.4,
      "ram_total_gb": 1.0,
      "storage": 22.5,
      "storage_used_gb": 1.8,
      "storage_total_gb": 8.0,
      "uptime": 123456
    }
  ]
}
```

`ram` et `storage` restent des pourcentages. Les champs `*_gb` donnent les quantites reelles en GiB.
`guests` contient les VMs et LXC, limites a `10` entrees par defaut cote bridge et a `6` pages maximum cote ESP32.

ESP32 meteo:

```json
{"temperature":31.9,"humidity":45,"status":"OPTIMAL","datetime":"19:30 - 02/07/2026","ip":"192.168.1.30"}
```

Si l'IP de ta station meteo ouvre une page HTML, c'est normal. Le firmware ne doit pas appeler la page racine, il doit appeler l'API JSON:

```cpp
#define WEATHER_URL "http://192.168.1.30/api"
```

## Bridge Proxmox

Le script `bridge/proxmox_bridge.py` utilise uniquement la bibliotheque standard Python.

Tu peux le faire tourner directement sur Proxmox, dans une VM, dans un LXC, ou sur une autre machine toujours allumee. Le plus logique dans ton cas: sur le serveur Proxmox lui-meme ou dans un petit LXC.

Pour la procedure LXC complete, avec ressources conseillees, token Proxmox, service systemd et tests, lis:

```text
docs/proxmox-lxc-setup.md
```

Si tu reutilises ton conteneur hub existant, utilise plutot le guide avec commandes copiables:

```text
docs/install-existing-container.md
```

Exemple si le bridge tourne sur le serveur Proxmox lui-meme:

Variables d'environnement:

```sh
export PVE_HOST="https://127.0.0.1:8006"
export PVE_NODE="pve"
export PVE_TOKEN_ID='root@pam!esp32-dashboard'
export PVE_TOKEN_SECRET='SECRET_DU_TOKEN'
export PVE_VERIFY_SSL="false"
python3 bridge/proxmox_bridge.py
```

Endpoint:

```sh
curl http://IP_DU_PROXMOX:8080/metrics
```

L'ESP32 doit appeler cette URL HTTP simple:

```cpp
#define PROXMOX_METRICS_URL "http://IP_DU_PROXMOX:8080/metrics"
```

Le bridge, lui, appelle l'API Proxmox:

```text
https://127.0.0.1:8006/api2/json/nodes/pve/status
```

Donc il y a deux flux differents:

- ESP32 -> bridge: `http://IP_DU_PROXMOX:8080/metrics`
- bridge -> Proxmox API: `https://127.0.0.1:8006/...`

## Build et upload

Avec PlatformIO:

```sh
pio run
pio run --target upload
pio device monitor
```

Le projet cible `board = nodemcu-32s` dans `platformio.ini`.

## Test rapide sans Proxmox

Avant de brancher Proxmox et le deuxieme ESP32, tu peux tester l'OLED avec un serveur mock sur ton Mac.

1. Trouve l'IP locale de ton Mac:

```sh
ipconfig getifaddr en0
```

2. Lance le mock:

```sh
python3 tools/mock_dashboard_api.py
```

3. Dans `include/config.h`, configure les deux URLs vers ton Mac:

```cpp
#define PROXMOX_METRICS_URL "http://IP_DU_MAC:18080/metrics"
#define WEATHER_URL "http://IP_DU_MAC:18080/weather"
```

4. Upload le firmware:

```sh
pio run --target upload
pio device monitor
```

Resultat attendu:

- l'OLED affiche l'accueil Tamagotchi avec temperature, activite, niveau et jauges `E/H/F`;
- les pages Proxmox affichent CPU, RAM, disque et uptime;
- les valeurs mock changent doucement toutes seules.

## Prochaine evolution recommandee

La V2 devrait remplacer le polling HTTP meteo par MQTT si tu veux quelque chose de plus robuste:

- le deuxieme ESP32 publie `home/weather/livingroom`;
- le dashboard s'abonne au topic;
- Proxmox peut rester en HTTP bridge ou publier lui aussi vers MQTT.

Garde le token Proxmox cote serveur, jamais dans le firmware ESP32.
