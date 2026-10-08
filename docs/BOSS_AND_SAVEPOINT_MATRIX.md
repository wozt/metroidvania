# Boss and savepoint matrix

Status: verified Zero Mission inventory and provisional Aria inventory.

The machine-readable source is `data/story/world_inventory.toml`. This document
separates three kinds of information:

- `verified_source`: confirmed in a pinned decompilation or exact local ROM
  structure;
- `verified_guide`: confirmed by the cited contemporary gameplay guides but not
  yet mapped to a decompilation symbol;
- `unverified`: deliberately blank until the project can prove it.

No crossover link in this matrix is an original-game fact. Future design links
must use the separate `cross_world_link` field.

## Sources and pinned baselines

- Zero Mission technical facts: `third_party/mzm` at commit
  `43b7fd52f552e4d38c1521ff9d4df5ee57e61493`, especially
  `src/data/rooms_data.c`, `src/data/rooms/*`, `src/data/spriteset.c`,
  `src/data/sprite_data.c`, `include/constants/event.h`, and the individual
  sprite AI files.
- Aria technical baseline: `third_party/cvaos` at commit
  `bc23d849d578c35ae12a5cec4e66549c3021a5be`. Its current naming coverage
  confirms the Graham/Julius/Chaos event branches and Chaos music, but does not
  yet expose a trustworthy named room/boss table for all eleven encounters.
- Aria roster and route classification:
  [bluberry's boss guide](https://gamefaqs.gamespot.com/gba/589456-castlevania-aria-of-sorrow/faqs/25095)
  and [Zenalasca's route guide](https://gamefaqs.gamespot.com/gba/589456-castlevania-aria-of-sorrow/faqs/56284).
- Zero Mission naming and encounter cross-check:
  [Metroid Recon boss guide](https://metroid.retropixel.net/games/metroidzm/bosses.php)
  and [BakonBitz's guide](https://gamefaqs.gamespot.com/gba/914982-metroid-zero-mission/faqs/60237).

Community guides are used for human-facing names and route taxonomy, not ROM
addresses, flags, room IDs, statistics, or engine behavior.

## Zero Mission: nine major encounters

| ID | Encounter | Classification | Verified source room(s) | Map coordinate | Completion flag/result |
|---|---|---|---|---|---|
| `mzm.deorem` | Deorem | boss, multi-location | Brinstar 12 and 19 | `(13,12)`, `(22,8)` | three encounter/kill flags; Charge Beam result |
| `mzm.mua` | Mua / Acid Worm | boss, sequence-break skippable | Kraid 5 | `(12,3)` | `EVENT_ACID_WORM_KILLED` |
| `mzm.kraid` | Kraid | major boss | Kraid 30 | `(9,12)` | `EVENT_KRAID_KILLED` |
| `mzm.kiru_giru` | Kiru Giru / Imago larva | special rescue encounter | Norfair 42 | `(19,11)` | `EVENT_CATERPILLAR_KILLED` |
| `mzm.imago` | Imago | boss, variant rooms | Ridley 19 and 27 | `(9,0)` for both | `EVENT_IMAGO_KILLED` |
| `mzm.ridley` | Ridley | major boss | Ridley 12 | `(7,6)` | `EVENT_RIDLEY_KILLED` |
| `mzm.mother_brain` | Mother Brain | apparent final boss | Tourian 4 | `(19,10)` | `EVENT_MOTHER_BRAIN_KILLED` |
| `mzm.ruins_test` | Ruins Test | special mandatory trial | Chozodia 42 | `(5,3)` | `EVENT_FULLY_POWERED_SUIT_OBTAINED` |
| `mzm.mecha_ridley` | Mecha Ridley | true final boss | Chozodia 72 | `(23,0)` | `EVENT_MECHA_RIDLEY_KILLED` |

Raw `SpriteStats` values are recorded in structured data as entity-table facts,
not automatically presented as complete boss HP. Multipart bosses can keep
health or vulnerability state in secondary sprites and AI work fields.

Kiru Giru is classified as a special encounter because the objective is to free
the larva; the source sets the caterpillar event when the relevant Imago larva
dies/leaves the encounter. The guide-facing name and the engine-facing Imago
larva symbols are both retained.

The two Imago room records share map coordinates and boss spriteset but use
different room resources. Their exact difficulty/route selection relationship
is still marked unverified rather than guessed.

## Aria of Sorrow: eleven campaign bosses

| ID | Boss | Guide-verified zone | Original-route role | Project campaign role |
|---|---|---|---|---|
| `aria.creaking_skull` | Creaking Skull | Castle Corridor | first linear boss | Samus prologue boss |
| `aria.manticore` | Manticore | Chapel | early route | required roster encounter |
| `aria.great_armor` | Great Armor | Study | unlocks Malphas route | required roster encounter |
| `aria.big_golem` | Big Golem | Dance Hall | grants Skeleton Blaze route | required roster encounter |
| `aria.headhunter` | Headhunter | Inner Quarters | optional in original route | required full-roster encounter |
| `aria.death` | Death | Clock Tower | grants Skula route | required roster encounter |
| `aria.legion` | Legion | Underground Cemetery | optional in original route | required full-roster encounter |
| `aria.balore` | Balore | The Arena | grants Giant Bat soul | required true-route gate |
| `aria.graham` | Graham Jones | Top Floor | bad/true branch pivot | mandatory true branch |
| `aria.julius` | Julius Belmont | Floating Garden mist door | true-route confrontation | mandatory true route |
| `aria.chaos` | Chaos | Chaotic Realm | true final boss | mandatory true final |

The Aria table is exhaustive for the requested campaign roster, including the
bad/true Graham branch and post-Graham route. Original room IDs, coordinates,
entity IDs, ROM addresses, boss stats, flags, and resource pointers remain
`unverified` until extracted from ROM structures or named decompilation data.

Man-Eater is tracked as a Boss Rush encounter outside this eleven-boss campaign
matrix. It must not silently become a story requirement.

## Zero Mission savepoints

The pinned source contains 29 unique rooms whose active room sprite data includes
`PSPRITE_SAVE_PLATFORM` or `PSPRITE_SAVE_PLATFORM_CHOZODIA`. This is stronger
evidence than counting map icons or filtering only on music/backgrounds.

| Area | Room IDs | Count |
|---|---|---:|
| Brinstar | 33, 34, 36, 39 | 4 |
| Kraid | 20, 31, 32, 36, 39 | 5 |
| Norfair | 36, 39, 41, 44, 45 | 5 |
| Ridley | 1, 20, 24, 25 | 4 |
| Tourian | 6, 11, 17 | 3 |
| Chozodia | 4, 15, 21, 27, 40, 61, 74, 75 | 8 |
| Crateria | none found with an active save-platform sprite | 0 |
| **Total** | | **29** |

Chozodia uses `PSPRITE_SAVE_PLATFORM_CHOZODIA` and
`MUSIC_SAVE_ELEVATOR_ROOM_2`, which explains why the earlier standard-background
candidate filter missed it. Chozodia room 4 has event-dependent spriteset
variants that both contain the save platform; it is one room, not two savepoints.

Structured entries include stable ID, area, source room, map coordinate, platform
symbol, music symbol, and verification source. Access requirements, adjacent
doors, boss context, and crossover destinations remain empty until verified.

## Aria savepoint coverage

Aria savepoints are not yet listed. Visual map counting is explicitly rejected.
The required next extraction must identify save-room records from the exact USA
ROM's room/entity structures, assign stable region/room IDs and coordinates, and
then cross-check the result against runtime observation. Until that pipeline
exists, the coverage state is `unverified` and the matrix makes no completeness
claim for Aria save rooms.

## Remaining verification work

1. Decode the Aria area/room/entity tables and derive its complete savepoint set.
2. Map all eleven Aria bosses to stable room/entity IDs, ROM addresses, flags,
   stats, attacks, music, resources, and spawn conditions.
3. Resolve the two Imago room variants and verify which runtime condition selects
   each record.
4. Extract door connections and access gates for all 29 MZM savepoints.
5. Add design-only cross-world save links after the progression graph exists.
