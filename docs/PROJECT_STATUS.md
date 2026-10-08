# État du projet

Dernière mise à jour : **2026-10-08**.

## Vision et décisions figées

Le projet vise un crossover réel de deux jeux, avec deux moteurs distincts et
un seul backend actif. Les ROM personnelles USA sont obligatoires et restent
locales. Les sources amont sont isolées en sous-modules. Linux/SDL3 est la cible
initiale ; la GBA est une étude ultérieure non garantie.

## État réellement atteint

| Élément | État | Preuve / limite |
|---|---|---|
| Validation des deux ROM | Réalisé | SHA-1 Python et C, refus avant SDL |
| Sources `mzm` et `cvaos` | Réalisé | sous-modules épinglés et audit ciblé |
| Interface deux backends | Réalisé | cycle exclusif init/enter/tick/render/leave/shutdown |
| Deux salles de diagnostic | Réalisé | SDL3, géométries distinctes, ROM obligatoires |
| 4 profils personnage/monde | Réalisé, simulé | paramètres séparés, pas les physiques originales |
| PV/KO/persistance | Réalisé | tests cœur et sauvegarde v1 |
| Correction de collision au switch | Réalisé | offsets déterministes + refus ; test dédié |
| HUD/debug/synergie | Réalisé, partiel | deux PV et overlay ; synergie explicitement factice |
| Moteur Aria authentique | Non commencé | piste AOT auditée, non reproduite |
| Moteur MZM authentique | Non commencé | aucune stratégie retenue |
| Assets/cartes/audio originaux | Non commencé | aucun asset extrait dans le dépôt |
| Port vraie GBA | Non commencé | faisabilité inconnue |

## Historique

- 2026-10-08 — Initialisation du dépôt, sous-modules `mzm` et `cvaos`, audit des
  références, installation vérifiée de Ghidra 12.1.4.
- 2026-10-08 — Ajout du squelette C11/SDL3, validation ROM, deux backends de
  diagnostic, session, sauvegarde, tests et documentation. Aucun commit local
  n'existe encore.

## Commandes et résultats

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/fusion_dev --validate-only
SDL_VIDEODRIVER=dummy timeout 2s ./build/fusion_dev
```

Résultats observés lors du premier passage : configuration et compilation à
100 % sans avertissement ; 4/4 tests passés (cœur, collisions, validateur et
anti-fuite) ; les deux ROM acceptées ; smoke test SDL resté actif deux secondes
puis arrêté par `timeout` (code 124), sans erreur. Une seconde compilation avec
AddressSanitizer et UndefinedBehaviorSanitizer a également passé les 4/4 tests.

## Écart entre prototype et jeux authentiques

Le prototype ne lit actuellement les ROM que pour les authentifier. Les salles
sont dessinées par SDL3, les collisions viennent de `room_sim.c` et les profils
sont des valeurs provisoires. Aucun code ARM, asset, salle, ennemi, boss, audio
ou format de sauvegarde original n'est exécuté. Le mot « backend » désigne ici
la frontière d'intégration et son double de test.

## Risques et prochaines actions

1. **P0 — Bloquant produit :** intégrer une frame authentique Aria, puis MZM.
2. **P0 — Licence :** décider si PolyForm Noncommercial est acceptable avant
   toute adoption de `gbarecomp`.
3. **P0 — Technique :** choisir pour MZM entre HAL source et recompilation après
   preuve de concept.
4. **P1 — État :** inventorier en Ghidra EWRAM/IWRAM, globals de salle et
   frontières de snapshot pour les deux ROM.
5. **P1 — Tests :** traces frame par frame, captures de rendu et tests de cycle
   de vie réel.
6. **P2 — Contenu :** adaptations des personnages, progression et synergies.

## Questions ouvertes non bloquantes

- Une licence strictement non commerciale est-elle acceptable pour le projet ?
- Le premier jalon authentique doit-il privilégier Aria, plus avancé côté AOT,
  ou MZM, plus lisible côté décompilation C ?
- Quelle politique de résurrection et quels flags de progression seront partagés ?

## Validation du prochain jalon

- lancement toujours conditionné aux deux SHA-1 ;
- une vraie frame de jeu issue de la ROM Aria ou MZM, sans asset committé ;
- backend opposé totalement suspendu ;
- capture et trace reproductibles ;
- documentation de la méthode d'exécution (native, recompilée, interprétée ou
  émulée) et de tout fallback ;
- tests automatiques conservés et audit de licence mis à jour.
