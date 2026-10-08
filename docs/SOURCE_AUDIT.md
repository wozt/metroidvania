# Audit des sources

Observation locale : **2026-10-08**. Les révisions obligatoires sont épinglées
comme sous-modules et n'ont pas été modifiées.

## Metroid: Zero Mission — `metroidret/mzm`

- URL : <https://github.com/metroidret/mzm>
- Révision : `43b7fd52f552e4d38c1521ff9d4df5ee57e61493`
- Date de révision : 2026-08-23.
- Licence : MIT (`third_party/mzm/LICENSE`).
- Statut annoncé par le README : décompilation en cours, 2718/2721 fonctions
  (99,89 %) et données hors blobs à 100 %. Ce chiffre vient de l'amont ; il n'a
  pas été recalculé ici.
- Build GBA : `agbcc`, `binutils-arm-none-eabi`, Python, g++ et baserom ;
  extraction par `tools/extractor.py`, puis `make`.
- ROM US documentée : SHA-1 `5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8`.

Points vérifiés :

- boucle et modes : `src/agbmain.c`, `AgbMain`, appel de `InGameHandler` ;
- scheduling en jeu : `src/in_game.c`, `InGameHandler`,
  `VBlankCodeInGame`, mises à jour Samus/sprites/projectiles/HUD ;
- salle : `src/room.c`, `RoomLoad`, `RoomLoadEntry`, `RoomLoadBackgrounds`,
  `RoomUpdate` ;
- joueur : `src/samus.c` et `include/samus.h`, notamment `SamusUpdate`,
  `SamusCheckCollisions`, `SamusUpdatePhysics` ;
- portes : `src/connection.c` et `src/color_fading.c` ;
- sauvegarde : `src/save_file.c`, `src/save_file_load.c`,
  `include/structs/save_file.h` ;
- matériel : `src/dma.c`, `src/display.c`, macros de registres dans
  `include/gba.h`, transferts VRAM/OAM dans `src/in_game.c` ;
- audio : `src/audio.c`, assembleur m4a et données dans `sound/`.

Le C est fortement couplé aux adresses GBA, registres, buffers globaux et au
timing VBlank. Les règles de gameplay sont réutilisables après abstraction,
mais ce dépôt n'est pas directement une bibliothèque Linux.

## Castlevania: Aria of Sorrow — `testyourmine/cvaos`

- URL : <https://github.com/testyourmine/cvaos>
- Révision : `bc23d849d578c35ae12a5cec4e66549c3021a5be`
- Date de révision : 2026-08-01.
- Licence : MIT (`third_party/cvaos/LICENSE`).
- Statut : décompilation matching en cours de la version USA. Aucun pourcentage
  de couverture n'est publié dans le README actuel ; plusieurs symboles restent
  nommés par adresse, donc l'analyse sémantique est moins avancée que celle de
  MZM.
- Build GBA : `agbcc`, `binutils-arm-none-eabi`, baserom, extracteur, puis
  `make`.
- ROM US documentée : SHA-1 `abd71fe01ebb201bcc133074db1dd8c5253776c7`.

Points vérifiés :

- boucle et VBlank : `src/main.c`, `AgbMain`, `VblankInterrupt`,
  `GameModeUpdate` ;
- jeu : `src/code_0800B700.c`, `GameModeInGameUpdate` ;
- entrée/entités : `src/code_080009A0.c`, `SetPlayerInput`,
  `EntityDeleteAll` ;
- transitions : `src/code_08001194.c`, `CheckRoomTransition` ;
- DMA/VRAM : `src/code_08001194.c`, `DmaQueue_Process`,
  `BgCmdBuffer_TransferToVram` ;
- sauvegarde : `src/code_08012744.c`, `SaveData_LoadSlotFromSram` et
  `SaveData_SaveSlotToSram`, plus `src/agb_sram.c` ;
- modèle mémoire : `include/structs/ewram.h` et `gEwramData`.

Les gros agrégats EWRAM, accès registres et nombreux symboles provisoires rendent
un port source direct risqué. La piste de recompilation statique mérite une
expérience isolée avant de modifier le backend.

## Pistes supplémentaires vérifiées, non intégrées

### `sergiomanzur/ariaOfSorrow-recomp`

- Révision observée : `f00abd91ee8338b4378e279e1aa4a5a3a8ab8525`
  (2026-09-18).
- Le README revendique une recompilation AOT complète, sans instruction
  interprétée, produite localement depuis la ROM utilisateur. Cette affirmation
  n'a pas été reproduite par notre build et reste donc **à confirmer**.
- Dépend de `gbarecomp`, annoncé sous PolyForm Noncommercial 1.0.0, ainsi que de
  SDL2 et de code C++20. Une adoption imposerait une contrainte non commerciale
  et une revue de compatibilité.

### `LTSchmiddy/metroid-zero-mission-pc-edition`

- Révision observée : `25cd4a1448ba24fa5b04b314afa51fca25e41b13`
  (2020-07-28).
- Le README confirme une intégration de données du jeu dans VBA-M/SDL2. C'est un
  emballage/modification d'émulateur, pas un port natif du code décompilé.
- Base VBA-M sous GPLv2, avec composants additionnels. Le dépôt contient aussi
  des répertoires d'assets : il ne sera pas importé dans ce projet.

## Inconnues prioritaires

- Runtime/recompilation retenu pour MZM : à confirmer.
- Exactitude du runtime Aria sous Linux et couverture réelle : à reproduire.
- Frontières minimales de snapshot des deux jeux : à établir dans Ghidra et par
  instrumentation.
- Licences et redistribuabilité du code généré depuis une ROM : avis juridique
  nécessaire avant diffusion.
