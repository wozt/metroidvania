# Concurrent campaign timeline

This is the human-readable view of the canonical three-track timeline. The
machine-readable source is `data/story/timeline.toml`. Event order is partial:
dependencies define canon, while `min_order` and `max_order` bound presentation
and progression without pretending that every optional action has one timestamp.

## Tracks

| Track | Scope |
|---|---|
| `metroid` | Zebes, Zero Mission progression, Soma's displaced route, and Samus's solitary epilogue. |
| `castlevania` | Dracula's castle, Aria progression, Samus's displaced route, and Soma's solitary epilogue. |
| `shared` | Start choice, dimensional state, Interzone, paired progression, separation, credits, and statistics. |

Room IDs beginning with `fusion:` are project-authored. Blank original room IDs
are deliberately unresolved and must be filled only from verified source data.

## Canonical sequence

| Order window | Event ID | Track | Event | Hard dependency | Output |
|---:|---|---|---|---|---|
| 0 | `shared.start_choice` | shared | Choose presentation order | none | `start_choice_locked` |
| 10-19 | `cv.samus_displaced` | castlevania | Samus enters the castle | start choice | `samus_in_castle` |
| 10-19 | `mzm.soma_displaced` | metroid | Soma enters Zebes | start choice | `soma_on_zebes` |
| 20-29 | `cv.creaking_skull` | castlevania | Samus defeats Creaking Skull | Samus displaced | `creaking_skull_defeated` |
| 20-29 | `mzm.deorem` | metroid | Soma defeats Deorem | Soma displaced | `deorem_defeated` |
| 30-39 | `cv.mechanism_awakened` | castlevania | Arm Cannon mechanism opens a breach | Creaking Skull | `samus_breach_ready` |
| 30-39 | `mzm.alien_soul_awakened` | metroid | Alien soul opens a breach | Deorem | `soma_breach_ready` |
| 40 | `shared.interzone_meeting` | shared | Full first meeting | both breaches | `pair_formed`, `interzone_open` |
| 50-69 | `shared.linked_save_tutorial` | shared | First explicit save-room link | pair formed | `linked_travel_enabled` |
| 60-119 | `mzm.early_route` | metroid | Early Zebes route and alien-soul growth | linked travel | `mzm_midroute_open` |
| 60-119 | `cv.early_route` | castlevania | Early castle route and Samus melee growth | linked travel | `cv_midroute_open` |
| 120-179 | `shared.armor_fragment_gate` | shared | First paired Chozo armor gate | both midroutes | `shared_armor_enabled` |
| 140-239 | `mzm.major_boss_route` | metroid | Kraid/Ridley-side progression | shared armor | `mzm_late_route_open` |
| 140-239 | `cv.major_boss_route` | castlevania | Castle central-boss progression | shared armor | `cv_late_route_open` |
| 180-239 | `cv.three_required_souls` | castlevania | Acquire the three true-route souls | castle major route | `aria_three_required_souls` |
| 240-299 | `mzm.mother_brain` | metroid | Mother Brain defeated | MZM late route | `mother_brain_defeated` |
| 240-299 | `cv.graham_branch` | castlevania | Graham true branch entered | CV late route and three souls | `graham_true_branch` |
| 300-359 | `mzm.chozodia_route` | metroid | Chozodia and Ruins Test | Mother Brain | `legendary_power_suit` |
| 300-359 | `cv.julius` | castlevania | Julius confrontation | Graham true branch | `julius_resolved` |
| 360-419 | `mzm.mecha_ridley` | metroid | Original Mecha Ridley defeated | Legendary Power Suit | `mzm_true_final_complete` |
| 360-419 | `cv.chaos` | castlevania | Chaotic Realm and Chaos defeated | Julius resolved | `cv_true_final_complete` |
| 420 | `shared.separation` | shared | Worlds deliberately separated | both true finals | `worlds_separated` |
| 430-469 | `mzm.samus_secret_final` | metroid | Samus fights Dracula manifestation | separation | `samus_epilogue_complete` |
| 430-469 | `cv.soma_secret_final` | castlevania | Soma fights copied Mecha Ridley | separation | `soma_epilogue_complete` |
| 500 | `shared.credits` | shared | Ending, credits, statistics | both epilogues | `campaign_complete` |

## Interleaving invariants

1. `shared.interzone_meeting` requires both prologue bosses, regardless of which
   prologue the player sees first.
2. Neither `mzm.mother_brain` nor `cv.graham_branch` can become reachable until
   both worlds have produced their midroute flag.
3. Separation requires the complete true-final route in both games.
4. The two secret finals occur after separation and may be played in either
   order, but credits require both.
5. Save-room links are authored edges with requirements and destinations. They
   are not inferred from spatial proximity and are not available everywhere.

## Event status vocabulary

- `planned`: required by the product brief but not implemented.
- `data`: represented in structured project data.
- `prototype`: executable with temporary systems or geometry.
- `verified`: original-game identity or behavior confirmed from source/ROM.
- `integrated`: final-engine implementation and validation complete.

The initial timeline is `data`, not a claim that its rooms, cutscenes, gates, or
boss logic are integrated. Boss-level dependencies will replace the coarse
route events as the verified matrix and progression graph mature.
