# Firmware Refactor Plan

## Objectif

Ce fichier sert de checklist de suivi pour la branche
`codex/firmware-architecture-refactor`.

Il doit etre mis a jour pendant le refactor pour indiquer :

- ce qui est fait ;
- ce qui reste a faire ;
- ce qui est volontairement reporte ;
- les validations effectuees.

## Phase 0 - Preparation de branche

- [x] Nettoyer les fichiers locaux parasites `.DS_Store`.
- [x] Ajouter `*.DS_Store` a `.gitignore`.
- [x] Ajouter `docs/public-release-plan.md` a `.gitignore`.
- [x] Commit le nettoyage `.gitignore` sur `main`.
- [x] Push `main` sur `origin/main`.
- [x] Creer la branche `codex/firmware-architecture-refactor`.
- [x] Creer la doc d'architecture cible.
- [x] Creer la checklist de suivi.

## Phase 1 - Socle pet/

Objectif : commencer a sortir le coeur DigiBat de `main.cpp` sans changement de
comportement.

- [ ] Creer `src/pet/`.
- [ ] Extraire `PetState` dans `src/pet/PetState.h`.
- [ ] Extraire les helpers de progression dans `src/pet/PetProgression.h`.
- [ ] Extraire l'implementation dans `src/pet/PetProgression.cpp`.
- [ ] Conserver les formules actuelles.
- [ ] Conserver la sauvegarde actuelle dans `main.cpp` pour cette phase.
- [ ] Verifier que les includes restent simples.
- [ ] Faire une verification statique du diff.

Fonctions concernees au depart :

```text
xpForNextLevel()
arenaTargetWins()
petSkillRank()
clampU8()
applyLevelUpStats()
addPetXp()
```

Critere de reussite :

- le comportement XP/level/stats reste identique ;
- `main.cpp` utilise les fonctions extraites ;
- aucune feature nouvelle n'est ajoutee dans cette phase.

## Phase 2 - Preparation stats manuelles

Objectif : preparer la mecanique sans encore modifier toute l'UI.

- [ ] Ajouter un champ `statPoints` dans `PetState`.
- [ ] Sauvegarder et charger `statPoints`.
- [ ] Remplacer la croissance auto `ATK/DEF/LCK` par des points disponibles.
- [ ] Garder `HP +2` automatique au level-up, sauf decision contraire.
- [ ] Afficher les points disponibles dans `STATS`.
- [ ] Ne pas encore changer Arena/Hunt.

Question a trancher avant implementation :

- [ ] Combien de points par niveau ?
- [ ] Est-ce que `HP` reste automatique ou devient aussi une stat choisie ?
- [ ] Est-ce qu'on bloque l'Arena si des points sont non depenses ?

## Phase 3 - UI allocation stats

Objectif : rendre les points de stats depensables par le joueur.

- [ ] Ajouter un ecran/menu d'allocation.
- [ ] Permettre de choisir `ATK`, `DEF`, `LCK`.
- [ ] Ajouter une courte explication par stat.
- [ ] Ajouter une confirmation avant depense.
- [ ] Sauvegarder immediatement apres allocation.
- [ ] Gerer plusieurs points disponibles.

Critere de reussite :

- le joueur comprend ce que fait chaque stat ;
- les points non depenses restent sauvegardes ;
- l'ecran reste lisible sur OLED.

## Phase 4 - PetCombat

Objectif : isoler les calculs de combat pour Arena et futur multijoueur.

- [ ] Creer `src/pet/PetCombat.h`.
- [ ] Creer `src/pet/PetCombat.cpp`.
- [ ] Deplacer les calculs Arena sans changer les resultats.
- [ ] Preparer des structures simples pour un futur duel.
- [ ] Eviter toute dependance UI dans `PetCombat`.

Critere de reussite :

- les calculs de combat sont regroupes ;
- le futur multijoueur pourra reutiliser les memes stats.

## Phase 5 - PetActivities

Objectif : separer les actions du compagnon.

- [ ] Creer `src/pet/PetActivities.h`.
- [ ] Creer `src/pet/PetActivities.cpp`.
- [ ] Isoler Hunt.
- [ ] Isoler Eat.
- [ ] Isoler Sleep.
- [ ] Isoler Arena.
- [ ] Isoler STOP et les resumes de queue.

Critere de reussite :

- les regles d'activite ne sont plus dispersees dans les ecrans ;
- les ecrans affichent les resultats mais ne calculent pas les regles.

## Phase 6 - UI

Objectif : isoler le rendu OLED sans changer l'apparence.

- [ ] Creer `src/ui/`.
- [ ] Deplacer les rendus de pages.
- [ ] Deplacer les helpers de barres/jauges.
- [ ] Garder les sprites dans une zone claire.
- [ ] Eviter de modifier le layout pendant le deplacement.

Critere de reussite :

- aucun changement visuel volontaire ;
- le rendu depend de l'etat, pas l'inverse.

## Phase 7 - HAL et boards

Objectif : preparer plusieurs cartes.

- [ ] Creer `src/hal/`.
- [ ] Isoler boutons.
- [ ] Isoler LED.
- [ ] Isoler ecran.
- [ ] Creer `src/boards/`.
- [ ] Ajouter profil NodeMCU-32S.
- [ ] Preparer profils ESP32-C3/S3 sans les activer par defaut.

Critere de reussite :

- `platformio.ini` peut rester simple ;
- les differences de pins sont centralisees.

## Phase 8 - Integrations

Objectif : isoler les integrations externes.

- [ ] Creer `src/integrations/`.
- [ ] Isoler Weather.
- [ ] Isoler Proxmox.
- [ ] Garder la tache reseau non bloquante.
- [ ] Ne pas changer les timeouts sans demande specifique.

Critere de reussite :

- les integrations ne polluent plus la logique DigiBat.

## Phase 9 - Multiplayer preparation

Objectif : preparer le multijoueur sans encore imposer BLE partout.

- [ ] Creer `src/multiplayer/`.
- [ ] Definir `PublicProfile`.
- [ ] Definir les messages `DuelRequest`, `DuelResponse`, `DuelResult`.
- [ ] Garder le protocole independant du transport.
- [ ] Comparer BLE et ESP-NOW avant implementation finale.

Critere de reussite :

- le profil public peut etre construit depuis `PetState` ;
- BLE reste un transport, pas une dependance du coeur de jeu.

## Validations

Ne pas lancer PlatformIO automatiquement par defaut.

Validations autorisees sans hardware :

- [ ] Inspection du diff.
- [ ] `git diff --check`.
- [ ] Recherche de references cassees avec `rg`.
- [ ] Compilation seulement si l'utilisateur le demande explicitement.

Validations reservees a l'utilisateur :

- [ ] Build PlatformIO.
- [ ] Upload USB.
- [ ] OTA.
- [ ] Test OLED.
- [ ] Test boutons.
- [ ] Test batterie.

## Journal d'avancement

### 2026-07-12

- [x] Branche de refactor creee depuis `main` pousse.
- [x] Documentation initiale creee.
- [ ] Premiere extraction `pet/` pas encore commencee.
