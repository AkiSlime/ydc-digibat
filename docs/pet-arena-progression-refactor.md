# Refonte Autonomous Pet: Arena, progression et Hunt

## Statut du document

Ce document décrit la refonte gameplay prévue pour le compagnon Tamagotchi.

Une première version de ces règles est maintenant implémentée dans le firmware.
Les formules restent simples et pourront être équilibrées après test matériel.

## Intention

La refonte ajoute une boucle de progression simple, inspirée de jeux rétro et
de combats automatiques type LaBrute, sans ajouter d'inventaire complexe ni de
sprites de récompense.

Le joueur doit pouvoir:

- nourrir et reposer la chauve-souris;
- l'envoyer chasser pour obtenir de la nourriture;
- l'envoyer dans l'arène pour gagner de l'XP;
- améliorer progressivement ses stats;
- consulter une fiche personnage compacte.

## Principes retenus

- Pas d'items de récompense pour l'instant.
- Pas de perks aléatoires.
- Pas de sprites d'ennemis ou d'objets.
- Pas de combat tour par tour affiché.
- Pas de système de mort, maladie ou soin pour l'instant.
- Une seule progression centrale influence `ARENA` et `HUNT`.
- Les rewards sont des stats, de l'XP, des niveaux, un titre et des skills.

## Boucle principale

La boucle cible est:

1. `HUNT` donne de la nourriture.
2. `EAT` restaure la faim.
3. `SLEEP` restaure l'énergie.
4. `ARENA` consomme des ressources et donne de l'XP.
5. L'XP donne des niveaux.
6. Les niveaux augmentent les stats et débloquent des skills.
7. Les stats améliorent les résultats de `ARENA` et de `HUNT`.

## Stats du compagnon

### Stats de survie actuelles

- `hunger`: faim/confort alimentaire.
- `energy`: énergie disponible pour agir.
- `food`: nourriture en stock.

Ces stats restent la base des actions quotidiennes.

### Stats de progression

- `level`: niveau actuel.
- `xp`: expérience actuelle.
- `hp`: points de vie utilisés pour calculer les runs d'arène.
- `atk`: attaque.
- `def`: défense.
- `lck`: chance.
- `arenaBest`: meilleur nombre de combats gagnés dans une arène.
- `arenaLast`: dernier résultat d'arène, si utile pour l'interface.
- `title`: titre actuel de la chauve-souris.
- `skill`: skill débloquée ou skill principale actuelle.

`SPD` est rejeté pour l'instant. La quatrième stat de combat est `LCK`.

## Progression

La progression doit rester prévisible.

Direction retenue:

- gains de stats automatiques à chaque niveau;
- skills débloquées par paliers fixes;
- titres débloqués par paliers ou records;
- pas de choix de build dans la première version.

Rythme implémenté:

```text
LV 1
XP 0/10
HP 10
ATK 2
DEF 1
LCK 1

XP NEXT = LV * 10

Level up:
HP +2
ATK +1 tous les 2 niveaux
DEF +1 tous les 3 niveaux
LCK +1 tous les 4 niveaux
```

Paliers de skills implémentés:

```text
LV 3  BITE
LV 6  HARD WING
LV 9  ECHO
LV 12 BLOOD FANG
LV 15 NIGHT MASTER
```

Ces skills sont affichés dans la fiche personnage. Pour cette première version,
ils sont surtout des paliers de progression lisibles; les effets mécaniques
restent portés par les stats.

## ARENA

### Rôle

`ARENA` est l'activité de progression principale.

Elle permet de transformer les ressources quotidiennes en XP, niveaux et
meilleures stats.

### Déclenchement

Règles implémentées:

- énergie minimum: `5`;
- coût énergie au lancement: `5`;
- coût faim au lancement: `5`;
- rythme de résolution: `1` combat toutes les `2 min`;
- `ARENA` ne consomme pas de nourriture directement;
- si énergie trop basse, l'action est bloquée.

### Résolution hybride

Le run d'arène est simulé en interne combat par combat, mais aucun combat n'est
affiché en détail.

Le firmware calcule une suite de combats abstraits:

1. la chauve-souris démarre avec ses `hp`;
2. un adversaire abstrait est généré selon le nombre de wins déjà obtenues;
3. une formule compare la puissance de la chauve-souris et de l'adversaire;
4. si la chauve-souris gagne, `wins` augmente;
5. la chauve-souris perd des `hp`;
6. le run continue tant que `hp > 0`;
7. quand `hp` tombe à `0`, le run se termine.

Cette résolution garde l'idée d'enchaîner des combats sans demander d'ennemis
visuels, de sprites ou de UI tour par tour.

### Formule Arena

Formule implémentée:

```text
enemyLevel = wins + 1
enemyPower = enemyLevel * 2 + random(0..enemyLevel)

batPower = ATK * 2 + DEF + random(0..LCK + level)

if batPower >= enemyPower:
  wins += 1
  hpLoss = max(1, enemyLevel + random(0..2) - DEF)
else:
  hpLoss = max(1, enemyLevel * 2 - DEF)

hp -= hpLoss
```

À confirmer:

- poids exact de `ATK`;
- réduction exacte de `DEF`;
- rôle exact de `LCK`;
- courbe de difficulté.

Ces points sont à équilibrer après test, mais la structure est en place.

### Impact des stats

- `HP`: augmente le nombre de combats encaissables.
- `ATK`: augmente les chances de gagner chaque combat.
- `DEF`: réduit les dégâts reçus entre combats.
- `LCK`: augmente les bons tirages, les critiques ou les sauvetages limites.
- `level`: peut donner un bonus léger de stabilité.

### Affichage pendant ARENA

Pendant `ARENA`, l'accueil ne doit pas afficher de temps restant.

Le slot du camembert/chronomètre affiche le nombre de combats gagnés en cours.

Format à préciser selon la place disponible:

```text
W 12
```

Le camembert n'est pas utilisé pour `ARENA`.

### Résultat de fin

Quand les `hp` du run tombent à `0`, une fenêtre de résultat reste affichée
jusqu'à confirmation.

Exemple:

```text
ARENA END
WINS 12
XP +18
LV 4
```

La fenêtre se ferme uniquement quand le joueur appuie sur `SELECT`.

Le résultat ne doit pas disparaître automatiquement.

## HUNT refondu

### Rôle

`HUNT` reste l'activité principale pour gagner de la nourriture.

La refonte doit connecter `HUNT` à la progression globale: une chauve-souris
plus forte doit chasser un peu mieux.

### Base conservée

Les règles actuelles peuvent rester la base:

- durée: `20 min`;
- coût énergie: `2`;
- coût faim: random uniforme `0..2`;
- loot de base: `+0 20%`, `+1 48%`, `+2 24%`, `+4 8%`;
- stock maximal: `10`.

### Influence des stats

Direction cible:

- `ATK`: augmente les chances de ramener plus de nourriture;
- `DEF`: réduit les mauvais effets de chasse, comme fatigue ou faim perdue;
- `LCK`: réduit les résultats à `0` et augmente les grosses prises;
- `level`: peut légèrement améliorer le minimum ou plafonner les résultats.

Formule implémentée:

```text
baseFood = loot actuel

si résultat > 0:
  ATK donne jusqu'à 35% de chance de +1 food

si résultat == 0:
  LCK donne jusqu'à 50% de chance de transformer le résultat en +1

DEF donne jusqu'à 50% de chance de réduire le coût faim
```

À équilibrer:

- seuils exacts de bonus;
- plafond de nourriture gagnée après bonus.

## EAT et SLEEP

`EAT` et `SLEEP` restent les actions de stabilisation.

Direction actuelle:

- `EAT` consomme `1` food et restaure la faim;
- `SLEEP` restaure l'énergie;
- pas de soin, maladie ou mort pour l'instant;
- pas d'item médical.

Ces actions permettent de préparer les runs `ARENA`, mais ne doivent pas devenir
des systèmes complexes dans cette refonte.

## Skills et title

Les skills remplacent les perks et les items.

Direction retenue:

- skills fixes;
- débloquées par paliers de niveau;
- effets simples;
- pas de choix aléatoire;
- pas de sprite dédié.

Paliers affichés:

```text
BITE

HARD WING

ECHO

BLOOD FANG

NIGHT MASTER
```

Les titres sont surtout cosmétiques ou légèrement informatifs.

Titres affichés:

```text
TINY BAT
CAVE BAT
NIGHT BAT
FANG BAT
ELDER BAT
```

À équilibrer:

- les titres donnent-ils un bonus ou sont-ils seulement descriptifs;
- si les skills doivent avoir des effets directs en plus des stats.

## Interface

### Menu

Le menu Tamagotchi propose:

- `HUNT`;
- `EAT`;
- `SLEEP`;
- `ARENA`;
- `STATS`;
- `SCREEN OFF`;
- `BACK`.

Ordre implémenté: `STATS`, `SLEEP`, `EAT`, `HUNT`, `ARENA`, `SCREEN OFF`,
`BACK`. Le menu est vertical et plein écran: le bouton suivant descend, le
bouton précédent monte, et `SELECT` valide. `HUNT`, `EAT` et `ARENA` ouvrent un
sélecteur numérique de runs avant lancement. Pendant une activité, un menu
spécial propose `STATS`, `SCREEN OFF` et `BACK`; il ajoute `STOP` pendant
`HUNT`, et `WAKE UP` pendant `SLEEP`.

### Fiche personnage

La fiche personnage est une vue RPG compacte.

Page 1:

```text
BAT STATS
LV 4 | XP 18/30
HP 8 | ATK 5
DEF 3 | LCK 2
ARENA BEST 12
```

Règles d'affichage:

- espace entre label et valeur: `HP 8`;
- séparateur entre stats: `HP 8 | ATK 5`;
- pas de zéro devant les valeurs sous `10`;
- pas de champ `WIN` sur cette fiche;
- écrire `ARENA BEST 12`.

Page 2:

```text
BAT INFO
TITLE NIGHT BAT
SKILL BITE
```

`TITLE` et `SKILL` peuvent être séparés de la page de stats pour garder la
première page lisible.

### Accueil

Le niveau courant s'affiche au-dessus du bloc de jauges:

```text
LV 4
E ...
H ...
F ...
```

## Résumé des décisions validées

- Ajouter `ARENA` comme activité de progression.
- Utiliser une résolution hybride interne, sans combat visuel tour par tour.
- Afficher les wins en cours pendant `ARENA`, pas un temps restant.
- Afficher un résultat de fin bloquant jusqu'à `SELECT`.
- Utiliser les stats `HP`, `ATK`, `DEF`, `LCK`.
- Remplacer `SPD` par `LCK`.
- Éviter items, perks, récompenses visuelles et sprites de reward.
- Connecter les stats à `ARENA` et à `HUNT`.
- Ajouter une fiche personnage compacte avec stats et meilleur score Arena.
- Garder mort, maladie, soin et propreté hors périmètre pour l'instant.

## Questions ouvertes

- Équilibrage du coût de lancement de `ARENA`.
- Équilibrage du rythme de résolution de `ARENA`.
- Équilibrage de la courbe XP.
- Équilibrage des gains de stats par niveau.
- Liste finale des skills.
- Liste finale des titres.
- Formule finale de combat.
- Formule finale de `HUNT` avec stats.
