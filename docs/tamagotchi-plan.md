# Tamagotchi Dashboard Plan

## Objectif

Ajouter un compagnon pixel-art sur la page d'accueil OLED du dashboard ESP32.
Le gameplay doit rester simple: le pet accompagne les routines de la journee,
affiche quelques alertes utiles, et garde une presence visuelle sans devenir une
simulation complexe.

## V1 validee

### Page d'accueil principale

La page d'accueil devient la page Tamagotchi + dashboard rapide:

- heure;
- temperature;
- etat reseau simple;
- pet pixel-art au centre;
- jauges compactes avec icones 16x16;
- bulle courte quand une action est attendue.

Les pages Proxmox, reseau, VM et LXC restent disponibles via la navigation.

### Etats simples

La V1 ne doit pas gerer de systeme emotionnel complexe. Les etats utiles sont:

- reveille;
- dodo;
- faim;
- chaud;
- connexion KO.

Wi-Fi KO et bridge KO utilisent le meme traitement visuel: une bulle courte et
un sprite pas content. Pas de separation stress/inquiet/triste.

### Horaires V1

Les horaires sont des constantes configurables dans le firmware:

- repas: 13:00;
- dodo automatique: 22:30;
- reveil automatique: 08:00.

Comportement:

- a partir de 13:00, si le pet n'a pas ete nourri aujourd'hui, il affiche une
  alerte repas;
- quand l'action nourrir est validee, l'alerte repas disparait jusqu'au
  lendemain;
- a partir de l'heure de dodo, le pet passe en dodo automatiquement;
- a partir de l'heure de reveil, il se reveille automatiquement;
- l'utilisateur peut aussi activer ou quitter le dodo manuellement.

### Temperature et LED

La LED rouge devient un indicateur simple:

- temperature >= 30.0 C: LED allumee, bulle `CHAUD`;
- temperature < 29.0 C: alerte chaud retiree;
- repas attendu: LED allumee;
- si plusieurs alertes sont actives, la LED reste allumee.

Le seuil bas a 29.0 C evite que la LED clignote sans arret autour de 30.0 C.

### Boutons

Les trois boutons restent suffisants.

Hors menu:

- gauche/droite: changer de page;
- select: ouvrir le menu d'action sur la page d'accueil.

Dans le menu:

- gauche/droite: choisir une action;
- select: valider.

Actions V1:

- nourrir;
- dodo / reveil;
- pages;
- ecran off;
- retour.

L'extinction de l'ecran reste disponible, mais elle n'est plus l'action directe
du bouton select.

### Nuit et OLED

Le mode nuit peut laisser l'ecran allume, mais l'affichage doit etre sobre:

- sprite dodo minimal;
- peu de pixels allumes;
- deplacement leger periodique possible pour limiter le marquage OLED;
- pas de gros blocs fixes.

### Stockage

Utiliser `Preferences` ESP32, pas `EEPROM`.

Donnees a persister:

- dodo manuel ou automatique;
- dernier jour ou le pet a ete nourri;
- etat ecran on/off;
- eventuellement dernier jour de reset des routines.

## V2: checks horaires periodiques

La V2 introduit des checks de routine, facon cron leger.

Principe:

- le firmware verifie l'heure toutes les 1 a 2 heures;
- au lieu de declencher exactement a 08:00 ou 22:30, il regarde dans quelle
  plage horaire on se trouve;
- si l'heure courante a passe un seuil depuis le dernier check, l'etat est mis a
  jour.

Exemples:

- si le dernier check etait avant 08:00 et le check courant est apres 08:00, le
  pet se reveille;
- si le check courant est apres 13:00 et que le pet n'a pas ete nourri
  aujourd'hui, l'alerte repas apparait;
- si le check courant est apres 22:30, le pet passe en dodo.

Ce modele accepte naturellement un declenchement a 09:00, 09:59 ou 10:00 si le
check n'a pas tourne exactement a l'heure prevue.

## V3: routines contextuelles

La V3 peut ajouter des routines sans complexifier le coeur:

- messages ou animations selon le jour de la semaine;
- petites animations dediees au repas, au reveil, au dodo ou a la chaleur;
- routines week-end differentes;
- reactions simples a la temperature;
- evenements rares mais non bloquants.

Le principe a garder: une table de regles lisible plutot qu'une grosse logique
cachee.

Exemple de structure future:

```cpp
struct PetRoutineRule {
  uint8_t startHour;
  uint8_t startMinute;
  uint8_t weekdaysMask;
  PetRoutineAction action;
};
```

## Direction artistique

Le personnage retenu pour la V1 est une chauve-souris pixel-art monochrome.
Deux spritesheets sont conservees dans `assets/tamagotchi/`:

- `bat_normal.png`;
- `bat_angry.png`.

Chaque spritesheet fait 4 colonnes x 3 lignes, avec des frames 32x32:

- ligne 1: idle;
- ligne 2: deplacement vers la droite;
- ligne 3: hurt / reaction negative.

Le deplacement vers la gauche pourra utiliser les frames de droite inversees au
moment du rendu si necessaire. La variante `angry` sert aux alertes simples:
faim, chaud, connexion KO.

Le style vise un pixel-art JRPG monochrome inspire par l'esprit Dragon Quest /
Final Fantasy, sans recopier de sprites existants:

- creature simple et expressive;
- silhouettes lisibles en 32x32 ou 40x40;
- bulles courtes;
- cadres sobres facon boite de dialogue RPG;
- animations de 2 a 4 frames maximum.
