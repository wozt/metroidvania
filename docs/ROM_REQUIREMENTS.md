# Exigences ROM

Seules deux révisions sont acceptées :

| Jeu | Région | Taille observée | SHA-1 |
|---|---:|---:|---|
| Castlevania: Aria of Sorrow | USA | 8 388 608 octets | `abd71fe01ebb201bcc133074db1dd8c5253776c7` |
| Metroid: Zero Mission | USA | 8 388 608 octets | `5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8` |

Les empreintes sont celles documentées par les dépôts amont et ont été
confirmées sur les deux fichiers locaux le 2026-10-08. Les noms ne constituent
pas une preuve : seul le contenu haché est accepté.

Les scripts lisent les ROM localement. Aucun upload, télémétrie ou requête
contenant leurs octets n'est effectué. Les futurs artefacts extraits doivent
rester dans `extracted/`, ignoré par Git. Le contrôle `no_proprietary_files`
refuse les extensions ROM et les gros fichiers hors sous-modules.

Le mode réel futur devra exiger ces contrôles avant extraction ou génération de
code. Une région différente doit être refusée jusqu'à ce qu'un audit, des
symboles et des tests spécifiques existent.
