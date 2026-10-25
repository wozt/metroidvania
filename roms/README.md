# Local ROMs

Place your own legally obtained dumps here. Git ignores this directory except
for this file. Never send ROMs through an issue, chat, CI system, or external
service.

Currently supported revisions:

- `Castlevania - Aria of Sorrow (USA).gba` - SHA-1 `abd71fe01ebb201bcc133074db1dd8c5253776c7`
- `Metroid - Zero Mission (USA).gba` - SHA-1 `5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8`

Local validation:

```sh
cmake --build build -j8
build/fusion_importer --verify --rom-dir roms
```

Importers reject unknown revisions before reading any game structure.
