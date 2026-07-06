## Statut du document

Ce document décrit la direction gameplay de Tamagotchi V2.

Il ne décrit pas encore l'implémentation technique. Les règles doivent rester
faciles à relire, discuter et modifier avant d'écrire le moteur dans le firmware.

La V2 part de la V1 finalisée, mais change le modèle de comportement. Le pet ne
doit plus choisir automatiquement toutes ses activités. Il doit surtout attendre
les instructions du joueur, sauf cas bloquant clairement indiqué.

## Intention

Le pet doit se comporter comme un petit compagnon semi-autonome.

Par défaut, il ne fait rien de complexe. Il reste en `IDLE`, ses stats évoluent
avec le temps, et le joueur décide quand lancer une action.

La boucle recherchée est simple:

1. Le pet attend.
2. Le joueur observe ses stats.
3. Le joueur choisit une action.
4. L'action prend du temps et modifie les stats.
5. Le pet revient en `IDLE`.
6. Si une stat bloque les actions, l'interface l'indique clairement.

Le pet ne doit pas partir chasser, manger ou dormir tout seul dans cette version,
sauf décision future explicite.

## Stats principales

### Faim

Nom logique: `hunger`.

La faim est une jauge de confort alimentaire:

- `100`: le pet est rassasié;
- `50`: état normal;
- `20`: le pet a faim;
- `0`: le pet est affamé.

La jauge descend avec le temps et certaines actions. Manger la remonte.

### Énergie

Nom logique: `energy`.

L'énergie regroupe la fatigue, le sommeil et la capacité à agir:

- `100`: le pet est plein d'énergie;
- `50`: il peut agir normalement;
- `20`: il est fatigué;
- `0`: il est épuisé.

Il n'y a pas de stat séparée pour le sommeil. Dormir est une action qui remonte
l'énergie.

Si l'énergie tombe à `0`, le pet ne peut plus faire d'action active. L'interface
doit indiquer que seule l'action dormir est pertinente.

### Nourriture

Nom logique: `food`.

La nourriture représente l'inventaire, par exemple des steaks.

La chasse peut ajouter de la nourriture. Manger consomme de la nourriture.

La nourriture reste gérée en interne. Elle n'est pas affichée sur l'accueil dans
le layout actuel.

## Statut et action

La V2 doit distinguer deux notions:

- le statut du pet;
- l'action en cours.

Le statut décrit l'état général: normal, affamé, fatigué, épuisé, etc.

L'action décrit ce que le pet fait maintenant: `IDLE`, `HUNT`, `EAT`, `SLEEP`.

Un pet peut donc être en action `IDLE` tout en ayant un statut `FATIGUÉ` ou
`AFFAMÉ`.

## Actions V2

### IDLE

`IDLE` est l'action de base.

Signification:

- le pet ne fait rien;
- il attend une instruction;
- le joueur peut ouvrir le menu;
- les stats continuent d'évoluer lentement.

Déclenchement:

- au démarrage du matin;
- après la fin d'une action;
- après un réveil;
- quand aucune action n'est en cours.

Effets :

- faim descend de `2` par heure;
- énergie reste stable;
- nourriture ne change pas.

### HUNT

`HUNT` est une action lancée par le joueur.

Signification:

- le pet part chasser;
- l'action dure un certain temps (1h);
- pendant ce temps, le pet n'est plus disponible pour une autre action;
- à la fin, il revient en `IDLE`.

Déclenchement:

- manuel uniquement;
- autorisé si `energy >= 10`;
- coût en énergie: `10`;
- bloqué si l'énergie est trop basse.

Effets attendus:

- consomme `10` énergie;
- consomme une quantité de faim random uniforme entre `0` et `10`;
- peut rapporter de la nourriture selon les probabilités ci-dessous.

Résultat possible:

- rien trouvé: `+0`, chance `50%`;
- petite quantité de nourriture: `+1`, chance `30%`;
- quantité moyenne: `+2`, chance `15%`;
- grosse prise rare: `+4`, chance `5%`.

Valeurs à décider:

- durée de chasse: `1h`;
- coût en énergie: `10`;
- coût en faim: random uniforme `0..10`;
- probabilités de résultat: `+0 50%`, `+1 30%`, `+2 15%`, `+4 5%`;
- quantité maximale gagnée par chasse: `+4`;
- stock maximal de nourriture: `10`.

### EAT

`EAT` est une action lancée par le joueur.

Signification:

- le pet mange une nourriture de l'inventaire;
- l'action dure un temps : 10 min;
- à la fin, il revient en `IDLE`.

Déclenchement:

- manuel uniquement;
- autorisé si `food > 0`;
- bloqué si `food == 0`;
- bloqué si la faim est déjà à `100`.

Effets attendus:

- consomme `1` nourriture;
- remonte la faim de `20`;
- consomme une quantité d'énergie random uniforme entre `0` et `5`.

Valeurs à décider:

- durée du repas: `10 min`;
- gain de faim par nourriture: `+20`;
- coût en énergie: random uniforme `0..5`.

### SLEEP

`SLEEP` est une action lancée par le joueur.

Signification:

- le pet dort;
- dormir remonte l'énergie;
- le joueur peut déclencher le sommeil quand il veut.

Déclenchement:

- manuel uniquement pour cette version;
- toujours autorisé si l'énergie n'est pas pleine;
- obligatoire si `energy == 0`.

Effets attendus:

- remonte l'énergie;
- la faim descend lentement pendant le sommeil;
- au réveil, le pet revient en `IDLE`.

Valeurs à décider:

- durée: illimitée jusqu'au réveil manuel ou au réveil automatique du matin;
- gain d'énergie: `+10` par heure;
- faim pendant le sommeil: `-3` par heure;
- réveil manuel: autorisé;
- réveil automatique: `08:00`, même si l'énergie n'est pas pleine.

## Comportements natifs

Le pet peut avoir des comportements natifs, mais ils ne doivent pas lancer des
actions à la place du joueur.

Comportements natifs autorisés pour l'instant:

- afficher un statut quand il a faim;
- afficher un statut quand il est fatigué;
- afficher un statut quand il est épuisé;
- bloquer les actions impossibles;
- recommander une action dans la bulle ou le menu.

Comportements natifs non retenus pour l'instant:

- partir chasser automatiquement;
- manger automatiquement;
- dormir automatiquement;
- choisir une routine complète sans instruction du joueur.

## Matin et rythme de journée

Règle cible actuelle:

- à `08:00`, le pet est en `IDLE`;
- si le pet dormait, il est réveillé automatiquement;
- il attend une instruction;
- il ne lance aucune action seul.

Les horaires peuvent encore servir à l'ambiance ou à l'affichage, mais pas à
lancer automatiquement des actions en V2.

Décisions:

- le réveil est forcé à `08:00`, même si l'énergie n'est pas pleine;
- pas d'affichage différent le soir pour l'instant;
- aucune action n'est empêchée la nuit.

## Blocages

Les blocages doivent être lisibles.

Exemples:

- énergie à `0`: pas de chasse,  pas d'activité active;
- nourriture à `0`: impossible de manger;
- faim à `100`: manger bloqué;
- action en cours: impossible de lancer une deuxième action.

L'interface doit indiquer la cause, pas seulement refuser l'action.

Messages courts possibles:

- `NO ENERGY`: energy is low and the player tries to launch an action other than sleep;
- `SLEEP`: while sleeping;
- `TIRED`: energy is low in idle;
- `NO FOOD`: no food left and the player tries to eat;
- `EAT`: while eating;
- `HUNT`: while hunting;
- `NO HUNGRY`: hunger is full and the player tries to eat;
- `STARVING`: hunger is low in idle.

## États d'interface à prévoir

### Disponible

Le pet est en `IDLE`, avec assez d'énergie pour agir.

Le menu peut proposer:

- `CHASSER`;
- `MANGER`;
- `DORMIR`;
- `SCREEN OFF`;
- `RETOUR`.

### Action en cours

Le pet est occupé. Le menu est désactivé,

Exception:

- pendant `SLEEP`, le menu spécial propose `WAKE UP` et `SCREEN OFF`.

### Épuisé

Le pet a `energy == 0`.

Les actions actives sont bloquées.

### Sans nourriture

Le pet a `food == 0`.

`MANGER` est bloqué. `CHASSER` reste possible si l'énergie est suffisante.

## Règles décidées

- stats initiales: `food = 1`, `hunger = 50`, `energy = 50`;
- durée de chasse: `1h`;
- coût énergie de chasse: `10`;
- coût faim de chasse: random uniforme `0..10`;
- résultats de chasse: `+0 50%`, `+1 30%`, `+2 15%`, `+4 5%`;
- durée du repas: `10 min`;
- coût énergie du repas: random uniforme `0..5`;
- gain de faim du repas: `+20`;
- sommeil: durée illimitée jusqu'au réveil manuel ou au réveil automatique;
- gain énergie pendant sommeil: `+10/h`;
- perte faim pendant sommeil: `-3/h`;
- réveil automatique: `08:00`, peu importe l'énergie;
- action annulable: non;
- temps restant affiché: dans la bulle, avec le nom de l'action et le temps restant;
- stock maximal de nourriture: `10`;
- langue des messages UI: anglais.

## Hors périmètre pour l'instant

À ne pas intégrer tant que les règles V2 de base ne sont pas validées:

- action `REST`;
- action `PLAY`;
- humeur;
- évolution;
- maladies;
- événements rares;
- plusieurs types de nourriture;
- automatisation complète des actions.
