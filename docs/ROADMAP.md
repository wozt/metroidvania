# Feuille de route

## P0 — Première logique authentique

1. Reproduire sous Linux le build et les tests de `ariaOfSorrow-recomp` dans un
   bac isolé, documenter chaque dépendance et confirmer zéro fallback interprété.
2. Prototyper un backend Aria qui démarre, avance exactement une frame et rend
   un framebuffer, sans encore modifier le gameplay.
3. Étudier une voie équivalente pour MZM : port source avec HAL ou recompilation
   statique. Produire une comparaison mesurée avant de choisir.
4. Définir les snapshots minimaux et l'arrêt propre de chaque runtime.

Critère de sortie : une salle authentique issue de chaque ROM peut être chargée
séparément, avec timing, entrées et rendu vérifiés. Les rectangles actuels ne
comptent pas.

## P1 — Deux moteurs et transition

- encapsuler mémoire, PPU, DMA, VBlank, audio et sauvegarde par backend ;
- faire l'aller-retour Aria → MZM → Aria avec état restauré ;
- comparer traces déterministes et captures à une référence ;
- garantir qu'aucun backend suspendu ne touche rendu ou simulation.

## P2 — Personnages invités

- conserver Soma natif dans Aria et Samus native dans MZM ;
- définir Samus/Aria puis Soma/MZM par adaptation locale, sans démarrer le
  second moteur ;
- tables de capacités, animations, dégâts et collisions testées.

## P3 — Progression et contenu

- schéma stable de sauvegarde croisée et migrations ;
- cartes, portes, boss et flags partagés explicitement ;
- résurrection configurable ;
- premiers hooks de synergie, chacun marqué expérimental.

## P4 — Distribution et étude GBA

- pipeline local sans assets, manifeste de licences et analyse juridique ;
- mesures de performance, reproductibilité et paquets Linux ;
- seulement ensuite, budget mémoire/ROM et prototype de linker GBA.
