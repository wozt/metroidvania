# Architecture

## Principe

Le projet ne possède jamais deux moteurs actifs. Le contrôleur de session
appelle le cycle `leave_world` du backend courant, change `active_world`, puis
appelle `enter_world` sur l'autre backend. Seul le backend actif reçoit `tick`
et `render`.

L'interface dans `include/core/backend.h` expose : `init`, `enter_world`,
`tick`, `render`, `leave_world` et `shutdown`. Les implémentations sont dans
`src/backends/metroid/` et `src/backends/castlevania/`. Le code commun
`room_sim.c` n'est qu'un harnais de test, pas un moteur unifié ni une
reproduction des jeux.

## État

`SessionState` sépare trois catégories :

- état de chaque personnage : PV, disponibilité, inventaire et capacités ;
- état de chaque monde : position, vitesse, cible, porte et visite ;
- état partagé explicite : carte, boss, version et jauge de synergie factice.

Le changement de personnage conserve l'instance de salle. Le nouveau profil
est testé contre les solides ; une liste d'offsets stable est essayée, puis le
changement est refusé si aucune position n'est sûre. Les PV ne sont jamais
copiés d'un personnage à l'autre.

La sauvegarde commence par la signature `FUSION1`, une version et la taille du
payload. Le format actuel est un prototype dépendant de l'ABI ; avant toute
compatibilité publique il devra devenir un encodage explicite avec checksum et
migrations.

## Accès ROM

`rom_validate` lit localement le fichier par blocs et calcule SHA-1. Aucun octet
n'est envoyé. Le lancement s'arrête avant SDL si l'une des deux versions USA
n'est pas reconnue. La prochaine couche devra exposer des vues en lecture seule
bornées (`RomView`) et des extracteurs versionnés, sans écrire les données dans
le dépôt.

## Remplacement progressif des stubs

1. Brancher un runtime GBA/recompilation à l'intérieur du backend concerné.
2. Encapsuler VRAM/OAM/palettes, DMA, IRQ/VBlank, audio et entrées derrière une
   couche hôte.
3. Définir un contrat d'instantané par moteur et un adaptateur vers
   `SessionState`.
4. Remplacer une salle simulée par une salle réelle minimale, puis les
   mouvements natifs du personnage du monde.
5. Ajouter le personnage invité sans faire tourner le second moteur.

Une intégration par recompilation statique est prometteuse pour Aria, mais elle
doit être évaluée face à sa licence non commerciale et reproduite localement.
Zero Mission n'a pas encore de runtime natif retenu.

## Synergies

`synergy_placeholder` réserve l'état de présentation. Les futurs hooks devront
être des commandes explicites (`support_attack`, `passive_tick`,
`on_character_swap`) exécutées par le backend actif. Aucun bonus ou soin de
synergie n'est implémenté aujourd'hui.
