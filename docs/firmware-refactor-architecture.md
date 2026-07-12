# Firmware Refactor Architecture

## Objectif

Cette branche sert a restructurer le firmware DigiBat vers une architecture
plus propre, sans changer le comportement utilisateur a chaque etape.

La version Arduino/PlatformIO reste la version stable. Le refactor doit rendre
le code plus lisible, plus testable mentalement, plus facile a porter sur
ESP32-C3/S3, et plus facile a reimplementer plus tard en ESP-IDF.

## Direction choisie

La cible est une architecture complete par domaines :

```text
src/
  main.cpp
  app/
  pet/
  ui/
  hal/
  integrations/
  multiplayer/
  boards/
```

Cette structure ne doit pas etre remplie en une seule grosse passe. Les modules
seront crees progressivement, avec une verification apres chaque extraction.

## Regle principale

La logique de jeu ne doit pas dependre directement du materiel.

En pratique, le coeur DigiBat doit eviter les dependances directes a :

- `U8g2` ;
- `WiFi` ;
- `HTTPClient` ;
- `Preferences` ;
- BLE ;
- pins physiques ;
- rendu OLED ;
- boutons.

La logique de progression, de combat et d'activites doit pouvoir etre comprise
et reutilisee sans connaitre la carte exacte.

## Version stable et version future

Modele retenu :

```text
Arduino/PlatformIO = version stable
ESP-IDF = version experimentale future
```

La version ESP-IDF ne doit pas demarrer comme une reecriture aveugle. Elle doit
reprendre une base Arduino deja clarifiee :

- etats propres ;
- fonctions de progression isolees ;
- calculs de combat separes ;
- stockage encapsule ;
- UI separee des regles ;
- couche hardware explicite.

## Modules cibles

### app/

Role :

- coordonner l'application ;
- gerer l'etat global de navigation ;
- appeler les services au bon moment ;
- garder `main.cpp` minimal.

Exemples futurs :

```text
AppController
AppState
MenuController
```

### pet/

Role :

- contenir le coeur DigiBat ;
- garder l'etat du compagnon ;
- gerer progression, XP, stats, activites et combats.

Exemples futurs :

```text
PetState
PetProgression
PetCombat
PetActivities
PetStorage
PetProfile
```

Premiere zone a extraire, car les stats manuelles dependront de cette couche.

### ui/

Role :

- dessiner les ecrans ;
- afficher les menus ;
- afficher les sprites ;
- afficher les resultats d'activite.

La UI lit l'etat, mais ne decide pas des regles de jeu.

### hal/

Role :

- cacher les details materiels ;
- isoler boutons, LED, ecran, temps, pins.

Cette couche doit aider a supporter plusieurs cartes sans dupliquer toute
l'application.

### integrations/

Role :

- isoler les services externes ;
- garder Weather et Proxmox hors de la logique DigiBat.

Les appels reseau doivent rester non bloquants pour l'UI et les animations.

### multiplayer/

Role :

- preparer le profil public DigiBat ;
- isoler le protocole multijoueur ;
- garder BLE comme transport possible, pas comme logique metier.

Principe :

```text
PublicProfile = donnees partagees
MultiplayerProtocol = messages de jeu
BleTransport = transport BLE concret
```

Le protocole doit rester reutilisable avec BLE, ESP-NOW, Serial ou simulateur.

### boards/

Role :

- declarer les profils materiels ;
- centraliser pins et capacites ;
- preparer NodeMCU-32S, ESP32-C3 Super Mini et ESP32-S3 Super Mini.

## Non-objectifs de cette branche

Cette branche ne doit pas :

- reecrire le firmware en ESP-IDF ;
- changer tout le gameplay d'un coup ;
- ajouter BLE immediatement ;
- ajouter les stats manuelles avant le socle de progression ;
- casser les sauvegardes existantes ;
- supprimer les integrations existantes ;
- changer l'UI sans raison fonctionnelle.

## Definition d'une etape reussie

Une etape de refactor est reussie si :

- le comportement utilisateur reste identique ;
- les fichiers modifies sont limites au domaine concerne ;
- `main.cpp` perd de la responsabilite ;
- les dependances deviennent plus claires ;
- les sauvegardes restent compatibles ;
- les prochaines etapes deviennent plus simples.

## Premiere cible concrete

La premiere extraction doit concerner `pet/` :

```text
PetState
PetProgression
```

Pourquoi :

- c'est le coeur des stats ;
- c'est necessaire pour les stats manuelles ;
- c'est plus petit que UI ou activities ;
- c'est le meilleur point de depart pour separer logique et materiel.

## Etat actuel connu

Au demarrage de cette branche, le firmware contient encore principalement :

- `PetState` dans `src/main.cpp` ;
- sauvegarde/chargement `Preferences` dans `src/main.cpp` ;
- progression XP et level-up dans `src/main.cpp` ;
- rendu `STATS` dans `src/main.cpp` ;
- calculs Arena dans `src/main.cpp`.

Le refactor doit partir de cette realite et avancer par petites extractions
controlees.
