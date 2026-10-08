# PC-native Metroid Vania: architecture contract

## Product flow (non-negotiable)

1. Start in a unified title menu with a deliberate choice of initial world.
2. Play the selected world without cross-world teleport shortcuts.
3. Travel to another world only by interacting with an explicitly linked
   save room. Links are bidirectional unless a future schema says otherwise.
4. A rejected transition must leave the player and the source world unchanged.
5. Preserve both world-specific progression and a shared session/save layer.
6. Each world has its own gameplay rules, but presentation, asset handling,
   world-graph data and PC tooling are project-owned and shared.

## Separation of responsibilities

- `fusion_dev`: SDL3 PC frontend and common state/session coordination.
- `core/world_graph`: versioned, validated, project-owned room/link manifest.
- `fusion_map_editor`: optional GTK4 front-end over that exact manifest.
- Metroid module: distinct movement, combat, upgrades and world entities.
- Aria module: distinct movement, combat, souls and world entities.
- mGBA (`--authentic-*`): **research and reference only**. It executes the
  original ROMs to compare graphics/state/physics, not as the PC production
  implementation. No automated assumption that decompiled GBA C runs on PC.

## Patch 0034 scope

`MVGRAPH 1` contains ONLY room ID, world ID, save-room flag, editor display
coordinates and bidirectional link enabled status. It does NOT represent
collision maps, tilesets, enemies, cutscene scripts or verified original save
room identifiers. Two `:demo:` rooms are project-authored placeholders.

The default diagnostic SDL game currently models the right-hand portal of
both demo rooms as a save pad. Its `M` interaction is gated by character
position and the enabled graph edge. That is a temporary **test adapter**,
not a claim that actual GBA save rooms have been identified or ported.

The GTK editor can move graph nodes and toggle a link, persisting it atomically
through `fusion_graph_save`. It is intentionally a *graph editor foundation*;
actual tilemap painting, room imports and assets are a later workstream.

## Next integration milestones

1. Read-only local ROM map/tileset extraction into ignored assets, with verified
   provenance and legal review prior to distribution.
2. PC-native room format v2: room dimensions, layers, tileset references,
   object placements, solid/hazard grids, spawn and save-pad volumes.
3. GTK4 tile/door/collision editor using the common v2 format.
4. SDL3 renderer/collision adapters consume this format; test representative
   rooms against the reference mGBA output and decomp behavior.
5. Genuine transitions from verified source save rooms to configured destination
   rooms, with transaction preflight, HP and progression mapping, and rollback.

Never conflate the prototype native-room arrival probes with a PC-native
reimplementation of either original game's engine.
