# Faisabilité d'un port GBA ultérieur

Le jalon Linux reste prioritaire. Aucun résultat actuel ne prouve que le
crossover tiendra sur une vraie GBA.

Contraintes à mesurer :

- ROM adressable de 32 Mio et coût d'une éventuelle cartouche/mapper ;
- 256 Kio EWRAM, 32 Kio IWRAM, 96 Kio VRAM, 1 Kio OAM et 1 Kio palettes ;
- budgets par scanline, VBlank, DMA et nombre de sprites ;
- superposition ou banques des deux ensembles de code/données ;
- coexistence des moteurs audio m4a, tables de voix et mémoire de mixage ;
- SRAM/Flash, format de sauvegarde et espace pour deux progressions ;
- toolchain `agbcc`/devkitARM, linking, overlays et relocalisation ;
- taille des cartes, tilesets, sprites, musiques et duplication des ressources.

Une stratégie plausible serait un seul moteur résident à la fois, avec état
commun compact et données de l'autre monde en ROM. Mais la GBA n'offre pas de
chargement dynamique général : des overlays ou appels bancarisés exigeraient un
linker et une discipline de relocalisation spécifiques. Il faut d'abord mesurer
les maps linker des deux builds originaux, la marge ROM/RAM et le pire temps de
frame. Statut : **non commencé, faisabilité inconnue**.
