# Journal des décisions

## D-001 — Deux backends exclusifs

Décision : conserver un backend Metroid et un backend Castlevania séparés. Un
seul reçoit simulation et rendu. Motif : préserver les règles et le feeling de
chaque jeu, éviter une moyenne qui ne reproduirait aucun des deux.

## D-002 — ROM obligatoires dès le prototype

Décision mise à jour le 2026-10-08 à la demande du propriétaire du projet : les
deux salles ne démarrent pas sans les deux ROM USA valides. Les tests unitaires
restent sans ROM propriétaire. Cela ne transforme pas les salles simulées en
port authentique ; l'interface le dit explicitement.

## D-003 — Sous-modules pour les sources amont

Décision : épingler `mzm` et `cvaos` en sous-modules, sans modifier leur contenu.
Motif : provenance, historique et frontière de licence vérifiables.

## D-004 — SDL3/C11 pour l'hôte

Décision : C11 et SDL3 pour le contrôleur Linux. Une future couche C++ ne sera
ajoutée que si le runtime de recompilation retenu l'exige.

## D-005 — Recompilation Aria non adoptée pour l'instant

Décision : auditer `ariaOfSorrow-recomp`, mais ne pas l'intégrer avant une
reproduction Linux et une décision sur PolyForm Noncommercial. Ses déclarations
amont ne valent pas validation locale.

## D-006 — Ghidra officiel

Décision : utiliser Ghidra 12.1.4 officiel, SHA-256
`ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db`,
installé dans `/opt/ghidra_12.1.4_PUBLIC`.
