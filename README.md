# Metroidvania Fusion

Prototype Linux d'un crossover **Castlevania: Aria of Sorrow × Metroid: Zero
Mission**. L'architecture impose deux backends séparés : les règles Metroid ne
sont pas fusionnées avec les règles Castlevania. Le personnage peut changer
instantanément dans un monde ; un changement de monde suspend le backend actif
avant d'activer l'autre.

## État honnête

L'exécutable actuel est un **banc d'intégration SDL3**, pas encore un port des
deux jeux. Les salles, collisions et profils sont des formes de diagnostic. Les
ROM personnelles sont désormais obligatoires au lancement et contrôlées par
SHA-1, mais leur code, leurs cartes et leurs assets ne sont pas encore exécutés.
Les backends affichent explicitement `BACKEND SIMULE`.

Déjà fonctionnel :

- deux salles et deux backends distincts ;
- Samus et Soma dans chacun des deux mondes, avec quatre profils physiques ;
- PV séparés, KO individuel et poursuite avec l'autre personnage ;
- cible persistante lors d'un changement de personnage ou d'un aller-retour de monde ;
- correction déterministe ou refus d'un changement en collision ;
- sauvegarde versionnée, HUD double, jauge de synergie factice et overlay debug ;
- validation locale stricte des deux ROM, sans transfert réseau.

## Prérequis Debian 13

```sh
sudo apt install build-essential cmake libsdl3-dev python3 git
```

Ghidra 12.1.4 est installé sur cette machine dans
`/opt/ghidra_12.1.4_PUBLIC`. Les commandes `ghidra` et
`ghidra-analyze-headless` sont disponibles dans le `PATH`.

## ROM personnelles

Copiez vos deux fichiers légalement obtenus dans `roms/`. Les noms par défaut,
empreintes et commandes de validation sont décrits dans
[`roms/README.md`](roms/README.md). Les ROM et extractions sont ignorées par Git.

```sh
python3 scripts/verify_roms.py \
  --aria "roms/Castlevania - Aria of Sorrow (USA).gba" \
  --metroid "roms/Metroid - Zero Mission (USA).gba"
```

## Compiler, tester et lancer

```sh
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/fusion_dev
```

Des chemins différents peuvent être donnés avec `--aria` et `--metroid`.
`--validate-only` valide sans ouvrir de fenêtre.

Touches : flèches ou `Q`/`D` pour se déplacer, `Espace` pour sauter, `J` pour
attaquer, `Tab` pour changer de personnage, `M` pour changer de monde, `F3`
pour le debug, `K` pour infliger des dégâts de test, `F5`/`F9` pour
sauvegarder/charger et `Échap` pour quitter.

## Sources de recherche

`third_party/mzm` et `third_party/cvaos` sont des sous-modules épinglés. Leur
code et leurs licences restent indépendants du nôtre. Voir
[`docs/SOURCE_AUDIT.md`](docs/SOURCE_AUDIT.md) et le suivi vivant
[`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md).

Ce dépôt ne contient et ne doit distribuer aucune ROM, image BIOS, sauvegarde,
musique, carte, sprite ou donnée propriétaire extraite. Le statut juridique
d'un futur patch ou paquet distribuable devra être examiné séparément.
