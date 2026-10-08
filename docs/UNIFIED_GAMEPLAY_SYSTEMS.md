# Common gameplay and room authoring contracts (planned)

These are accepted requirements, not claims that native gameplay exists.
Common authoring tools must preserve each game's original mechanics.

## Native room creation

- Right-click **Zero rooms** or **Aria rooms** -> Create room -> choose
  world/region, geometry, layers/tileset, collision, spawn point, doors,
  transitions, resources, entities, event triggers, and music.
- One editor must handle original imported rooms and project-authored rooms,
  with clear provenance, history, validation and save/load.
- Project-owned authored rooms need stable IDs, engine-specific resource limits,
  valid exits, area links and collision-safe start positions.
- Native adapters for each engine must consume validated room artifacts in the
  actual game before enabling Create room; no phantom playable rooms.
- The underlying ROMs and imported original files must never be overwritten.

## Powers and progression

- A canonical capability registry tracks acquired abilities, permanent flags,
  inventory states, unlock predicates and progression across both worlds.
- Samus retains upgrades, suits, beam and missile/bomb variants, energy and ammo.
- Soma retains HP/MP, attributes, soul categories/equipment, weapons and items.
- Gameplay effects resolve through character/world-specific adapters; avoid
  equating unrelated mechanics or inventing unsupported stats.
- Doors, bosses, savepoints, events and cutscenes reference the same capability
  conditions without losing original gameplay behavior.

## Shared status menus

- Unified menu navigation and save semantics with two character-appropriate
  presentations for stats, upgrades, items and acquired capabilities.
- Hide unavailable data rather than filling imaginary values.
- Save/load and cross-world transfers preserve both character-specific state
  and shared narrative progression flags.

## Controllers

- Actions must be device independent: move, jump, attack, secondary, confirm,
  cancel, status, map, pause, switch character and contextual actions.
- SDL3 gamepad discovery, hotplug, custom mapping, deadzones and keyboard parity
  belong in a common input service connected to each native game adapter.
- Eventually support gamepad-only UI, previews of bindings and per-device
  profiles; define focus, remapping and input conflict policies.
- Regression coverage must include device disconnect/reconnect, remapping,
  analog movement and menu navigation.

## Acceptance sequencing

1. Native authored-room schema, validation, project-owned save/load.
2. Both world-specific room adapters with playable runtime loader tests.
3. Enable Create room on both browsers after end-to-end game validation.
4. Shared progression/capability registry with per-character behavior.
5. Character-specific stats panels in one menu framework.
6. Device-independent actions and controller/keyboard parity.
7. Native event/cutscene adapters using the common schema.
