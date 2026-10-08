# ROM locales

Placez ici vos copies personnelles, obtenues légalement. Ce répertoire est ignoré
par Git, à l'exception de ce fichier. Ne transmettez jamais les ROM dans un ticket,
un chat, une CI ou un service externe.

Versions actuellement acceptées :

- `Castlevania - Aria of Sorrow (USA).gba` — SHA-1 `abd71fe01ebb201bcc133074db1dd8c5253776c7`
- `Metroid - Zero Mission (USA).gba` — SHA-1 `5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8`

Validation locale :

```sh
python3 scripts/verify_roms.py \
  --aria "roms/Castlevania - Aria of Sorrow (USA).gba" \
  --metroid "roms/Metroid - Zero Mission (USA).gba"
```

Le prototype refuse de lancer une salle si les deux empreintes ne correspondent pas.
