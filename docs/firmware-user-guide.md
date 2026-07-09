# Guide utilisateur du firmware ESP32 Dashboard

Ce firmware transforme un ESP32 avec un petit écran OLED en dashboard domestique
et en mini compagnon type Tamagotchi.

Il sert à deux choses principales:

- afficher rapidement l'état de services locaux, comme Proxmox, le réseau et la
  météo;
- faire vivre une petite chauve-souris pixel-art avec de l'énergie, de la faim,
  de la nourriture, des niveaux et des actions.

Le firmware continue d'afficher la page d'accueil même si le Wi-Fi, la météo ou
le bridge Proxmox ne répondent pas. Dans ce cas, certaines informations
deviennent indisponibles, mais le compagnon reste utilisable.

## Commandes

Le firmware se pilote avec trois boutons:

- `Suivant`: passe à la page suivante ou descend dans un menu;
- `Précédent`: passe à la page précédente ou remonte dans un menu;
- `Select`: ouvre un menu, valide un choix ou ferme certains écrans.

Quand l'écran est éteint, n'importe quel bouton le rallume.

## Pages principales

### Accueil Tamagotchi

La première page est la page principale. Elle affiche:

- la température reçue depuis la station météo;
- l'activité ou l'état actuel du compagnon;
- la chauve-souris animée;
- le niveau actuel;
- les jauges `E`, `H` et `F`.

Les jauges signifient:

- `E`: énergie;
- `H`: faim;
- `F`: nourriture en stock.

Quand tout va bien et qu'aucune action n'est en cours, l'activité affiche
`chill`.

### Page Proxmox

La page Proxmox affiche un résumé de la machine surveillée:

- CPU;
- RAM;
- disque;
- uptime.

Ces informations viennent du bridge Proxmox configuré sur le réseau local.

### Page réseau

La page réseau affiche les informations utiles pour diagnostiquer le dashboard:

- signal Wi-Fi;
- adresse IP de l'ESP32;
- âge des dernières données Proxmox et météo;
- état du service OTA.

Cette page est utile pour vérifier que l'ESP32 est bien connecté et prêt pour
les mises à jour par Wi-Fi.

### Pages VM et LXC

Après les pages principales, le firmware peut afficher une page par VM ou
conteneur LXC renvoyé par Proxmox.

Chaque page affiche le nom, l'état, le CPU, la RAM, le disque et l'uptime de
l'élément concerné.

## Compagnon Tamagotchi

Le compagnon est une chauve-souris pixel-art. Il ne choisit pas ses actions tout
seul: le joueur observe ses stats et décide quoi faire.

Ses besoins principaux sont:

- énergie: nécessaire pour chasser ou aller en arène;
- faim: baisse avec le temps ou certaines actions;
- nourriture: sert à manger et remonter la faim.

Il a aussi une progression de type RPG:

- niveau;
- XP;
- HP;
- ATK;
- DEF;
- LCK;
- meilleur score d'arène;
- titre;
- skill débloquée.

Les stats de progression influencent surtout la chasse et l'arène.

## Menu Tamagotchi

Depuis l'accueil, `Select` ouvre le menu principal.

Le menu propose:

- `STATS`: consulter la fiche du compagnon;
- `SLEEP`: dormir pour récupérer de l'énergie;
- `EAT`: manger pour remonter la faim;
- `HUNT`: chasser pour obtenir de la nourriture;
- `ARENA`: faire des combats automatiques pour gagner de l'XP;
- `SCREEN OFF`: éteindre l'écran;
- `BACK`: fermer le menu.

Le menu est vertical. `Suivant` descend, `Précédent` remonte, `Select` valide.

## Fiche STATS

`STATS` ouvre une fiche en plusieurs pages.

Elle permet de voir:

- le niveau, l'XP et les stats de combat;
- les besoins actuels: énergie, faim et nourriture;
- un petit rappel des lettres `E`, `H` et `F`;
- le titre et la skill actuelle du compagnon.

Dans cette vue, `Suivant` et `Précédent` changent de page. `Select` ferme la
fiche.

## Actions

### HUNT

`HUNT` lance une chasse.

Une chasse:

- dure 20 minutes;
- consomme de l'énergie;
- peut faire baisser la faim;
- peut rapporter de la nourriture.

Les résultats de base sont:

- `+0` nourriture: 20%;
- `+1` nourriture: 48%;
- `+2` nourritures: 24%;
- `+4` nourritures: 8%.

Les stats `ATK`, `DEF` et `LCK` peuvent améliorer le résultat ou réduire les
effets négatifs.

Avant de lancer `HUNT`, le firmware affiche un sélecteur de quantité. Il permet
de choisir combien de chasses enchaîner selon l'énergie disponible.

Pendant une chasse, `Select` ouvre un menu d'activité. `STOP` permet d'arrêter
la chasse en cours. Une chasse stoppée consomme quand même ses coûts, mais ne
donne pas de nourriture.

### EAT

`EAT` permet au compagnon de manger.

Un repas:

- dure 3 minutes;
- consomme 1 nourriture;
- remonte la faim;
- peut coûter un peu d'énergie.

Le firmware ne propose pas de gaspiller de nourriture inutilement: si la faim
est déjà pleine, manger est bloqué.

Comme pour la chasse, un sélecteur permet de choisir plusieurs repas utiles si
le stock de nourriture et la faim le permettent.

### SLEEP

`SLEEP` met le compagnon au sommeil.

Pendant le sommeil:

- l'énergie remonte avec le temps;
- la faim baisse lentement;
- le compagnon peut se réveiller automatiquement à 08:00;
- le joueur peut aussi le réveiller manuellement avec `WAKE UP`.

Le sommeil n'est pas une action chronométrée comme `HUNT` ou `EAT`. Il dure
jusqu'au réveil manuel ou automatique.

### ARENA

`ARENA` lance des combats automatiques.

Un run d'arène:

- consomme de l'énergie et de la faim au lancement;
- résout un combat automatique toutes les 2 minutes;
- affiche le nombre de victoires en cours;
- se termine quand les HP du run tombent à 0;
- donne de l'XP selon le résultat.

À la fin, une fenêtre affiche:

- `WINS`;
- `XP`;
- `LV`.

Cette fenêtre reste affichée jusqu'à ce que le joueur appuie sur `Select`.

Si plusieurs runs d'arène ont été sélectionnés, le run suivant démarre après la
fermeture du résultat précédent.

## Alertes et messages

Le panneau d'activité peut afficher des messages courts:

- `NET KO`: problème Wi-Fi ou bridge Proxmox indisponible;
- `HOT`: température au-dessus du seuil configuré;
- `NO ENERGY`: énergie insuffisante;
- `NO FOOD`: plus de nourriture;
- `NO HUNGRY`: faim déjà pleine;
- `BUSY`: une action empêche d'en lancer une autre;
- `STARVING`: faim très basse;
- `TIRED`: énergie basse.

Ces messages servent à comprendre rapidement pourquoi une action est possible ou
bloquée.

## LED rouge

La LED rouge a deux rôles:

- signaler une alerte de chaleur avec `HOT`;
- jouer un petit motif lumineux quand `HUNT`, `EAT` ou `ARENA` se termine
  normalement.

Le motif de fin de tâche ne bloque pas le firmware. Le dashboard continue de
fonctionner pendant ce signal.

## Mises à jour OTA

Après un premier flash USB, le firmware peut être mis à jour par Wi-Fi avec OTA.

La page réseau permet de vérifier si OTA est prêt:

- `ON :3232`: service OTA disponible;
- `OFF WIFI`: Wi-Fi non connecté;
- `OFF INIT`: OTA pas encore initialisé.

OTA ne change pas l'utilisation quotidienne du dashboard. C'est seulement une
façon plus pratique d'envoyer une nouvelle version du firmware.

## Points importants

- Le compagnon n'a pas de mort, maladie ou soin dans cette version.
- Il ne part pas chasser, manger ou dormir tout seul.
- Le joueur garde le contrôle des actions.
- La nourriture est limitée par un stock maximal.
- Les valeurs peuvent être ajustées dans la configuration du firmware.
- Si Proxmox ou la météo ne répondent pas, l'accueil Tamagotchi reste utilisable.
