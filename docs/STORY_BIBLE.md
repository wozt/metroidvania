# METROID VANIA story bible

Status: first production baseline derived from the 2026-10-08 product brief.
This document defines project canon. Facts inherited from either original game
must remain distinguishable from crossover additions and technical assumptions.

## Core premise

Two dimensional incidents occur in parallel. Samus Aran is pulled into Dracula's
castle while Soma Cruz is pulled onto Zebes. Each survives an altered opening,
obtains a world-specific dimensional ability, and reaches a new shared place
called the Interzone. From that meeting onward both heroes exist physically at
the same time: one is controlled by the player and the other is an AI companion.

The campaign interleaves Zero Mission and Aria of Sorrow. Neither world may be
completed as a self-contained campaign before the other. Travel is authorized
through explicit linked save rooms and story portals; there is no unrestricted
global world-switch key in the final design.

## Canon rules

- The title is **METROID VANIA**.
- Zero Mission and Aria retain distinct movement, combat, enemies, rooms,
  audiovisual identities, and progression logic.
- Original story facts remain canon unless an event is explicitly marked as a
  crossover divergence.
- The complete true-final routes of both games remain mandatory.
- A crossover mechanic may reinterpret an ability, but it must not silently
  erase the original reason that an area, boss, or ending matters.
- Every authored event has a stable ID and declares its track, dependencies,
  required flags, produced flags, room status, and canonical status.
- Unknown original room IDs, addresses, flags, or statistics stay unknown until
  verified. Narrative convenience is never evidence for a ROM fact.

## Start choice and inverted prologues

The title screen offers the two source campaigns, but the choice determines
which displaced hero the player controls first:

| Selected campaign | Playable opening | First mandatory encounter | Result |
|---|---|---|---|
| Aria of Sorrow | Samus in Dracula's castle | Creaking Skull | The Ancient Dimensional Mechanism attaches to the Arm Cannon and opens a controlled breach. |
| Zero Mission | Soma on Zebes | Deorem | Soma absorbs an alien soul whose dimensional response opens a controlled breach. |

Both prologues occur in the same canonical time window. The selected prologue is
played first; the other is then played as a parallel chapter. This changes
presentation order, not chronology or canon.

"Ancient Dimensional Mechanism" is a working name. The production name must fit
both Chozo material culture and the castle's occult vocabulary without claiming
that either civilization originally created the complete crossover system.

## Protagonists

### Samus Aran

Samus begins in the castle with her Arm Cannon and suit identity intact, but her
usual progression cannot solve every castle problem. The dimensional mechanism
becomes the interface between Chozo technology and the castle's supernatural
systems. Later she can equip selected daggers, swords, and spears. These weapons
must use a dedicated Samus handling and animation layer rather than Soma's exact
poses. Samus gains experience and levels independently of her original suit
upgrades.

Her arc is about treating an apparently mystical world as a system she can
observe without reducing it to machinery. Her ending test is solitary: after
the main separation, she faces an autonomous manifestation of Dracula's power
on Zebes without Soma present.

### Soma Cruz

Soma begins on Zebes without Samus's traversal kit. The Deorem encounter proves
that his power of dominance can absorb alien life. Every supported Zero Mission
species therefore needs a soul definition or an explicit, reviewed exception.
The special Metroid soul grants a compact floating form that fills the broad
traversal role of Morph Ball while retaining Soma's own risk and resource rules.

His arc is about using Dracula's power without accepting Dracula's identity.
His ending test is solitary: after the main separation, he enters a new castle
wing and defeats a dormant or copied Mecha Ridley without Samus present.

## Shared pair

After the Interzone meeting, both characters remain present in the active room.
The player can switch the active character when the local engine and room state
allow it. The inactive hero follows as an AI companion with these narrative and
gameplay constraints:

- the companion must traverse the room rather than teleport continuously;
- bounded recovery is allowed when pathfinding cannot resolve a separation;
- recovery cannot bypass progression gates, hazards, boss locks, or cutscenes;
- authored scenes can temporarily lock control to a named speaker or fighter;
- defeat, separation, and reunion must be represented by explicit flags;
- each character keeps separate health, equipment, experience, and native
  abilities unless a field is deliberately declared shared.

Chozo armor fragments are crossover equipment usable by both heroes. Their
benefits can differ per character, but acquisition is a shared campaign fact.

## Campaign structure

### Act 0 - Selection and displacement

The title choice establishes presentation order. A paired dimensional event
places Samus in the castle and Soma on Zebes. Neither understands the other
world, and neither portal is yet stable.

### Act I - Parallel first blood

Samus defeats Creaking Skull and recovers the dimensional mechanism. Soma
defeats Deorem and absorbs the first alien soul. Each opens a one-way breach.

### Act II - The Interzone

The heroes arrive from opposite sides of a new neutral space. A full dialogue
establishes identity, immediate goals, and distrust without manufacturing a
fight between them. Two dormant portals become the first explicit world links.

### Act III - Coupled exploration

The pair tests companion behavior, character switching, alien souls, Samus's
melee handling, and linked save rooms. Early gates force travel in both worlds.
The crossover must become mechanically real before either campaign reaches its
midpoint.

### Act IV - Converging threats

Kraid/Ridley-side discoveries and the castle's central bosses reveal that the
breaches react to both Chozo technology and Dracula's power. Shared armor
fragments and paired progression gates deepen the dependency between worlds.

### Act V - False solutions

Graham's claim and the apparent Zero Mission climax each offer incomplete
solutions. The campaign makes clear that defeating Mother Brain alone or taking
Aria's normal branch alone cannot repair the dimensional system.

### Act VI - True routes

Aria must satisfy the three-soul condition, pass through Graham's true branch,
confront Julius, enter the Chaotic Realm, and defeat Chaos. Zero Mission must
continue through Mother Brain, Chozodia, the Ruins Test, the Legendary Power
Suit, and Mecha Ridley.

### Act VII - Separation

The repaired Interzone can separate the worlds, but doing so also isolates the
heroes. Their farewell is a deliberate choice rather than an accidental portal
failure. Shared campaign flags remain available for the epilogues.

### Act VIII - Secret solitary epilogues

Samus fights the autonomous Dracula-power manifestation on Zebes. Soma explores
the new castle wing and defeats the dormant or copied Mecha Ridley. These fights
confirm that each hero internalized something from the other world without
turning either character into a replacement for the other.

### Epilogue and credits

The final scenes resolve both worlds, then show credits and a combined statistics
screen. Statistics distinguish story order from canonical order and report at
least completion time, endings reached, bosses defeated, save-room links used,
character switches, companion recoveries, souls collected, and shared armor
fragments found.

## Required original conclusions

The Aria route is not complete without Graham's true branch, the required three
souls, Julius, the Chaotic Realm, and Chaos. The Zero Mission route is not
complete without Mother Brain, the post-Tourian continuation, Chozodia, the
Ruins Test, the Legendary Power Suit, and Mecha Ridley.

Alternative and failed branches may be represented for completeness, but they
cannot replace the combined canonical route. Branch metadata belongs in the
timeline and boss/savepoint matrix rather than being hidden in dialogue logic.

## Narrative delivery

Cutscenes use deterministic project-authored scripts with explicit speakers,
portraits, staging, input locks, flags, and skip behavior. The planned source
format is TOML. Runtime playback and editor visualization must consume the same
validated data. Dialogue localization is separate from event logic.

## Guardrails for implementation

- Do not implement world travel as free `M` switching.
- Do not complete one source campaign before beginning meaningful progression
  in the other.
- Do not present emulated execution as the final PC engine.
- Do not invent original room IDs, ROM addresses, statistics, or flags.
- Do not let companion recovery cross a locked door or satisfy a gate.
- Do not collapse Samus and Soma into one inventory or one movement model.
- Do not commit extracted proprietary assets with narrative or editor data.

## Open authorial decisions

- Final name and origin wording for the dimensional mechanism.
- Exact first Interzone dialogue and the emotional tone of the farewell.
- Which Chozo armor fragments exist and how each character interprets them.
- Exact form, attacks, and visual language of the Dracula manifestation.
- Whether the copied Mecha Ridley is created by Chaos residue, castle alchemy,
  or another explicitly foreshadowed mechanism.
- Resurrection and defeat policy for the companion outside boss arenas.
