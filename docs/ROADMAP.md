# Roadmap

## P0 - Complete structural inventories

- decoded: Aria room records, transitions, graphics references and entity lists;
- verified: all eleven Aria campaign bosses have stable room/entity identities;
- resolve MZM Imago room variants;
- add access gates and connections to both savepoint inventories.

Exit criterion: boss and savepoint records for both games have stable native
identities and no invented technical fields.

## P1 - Native room reconstruction

- complete Zero Mission collision, entities, animated graphics and effects;
- build the equivalent Aria room renderer and local override format;
- display and edit doors, objects, enemies, music and scripts in GTK4;
- validate reconstructed rooms against reference captures.

Exit criterion: one representative room from each game renders and collides
natively from imported data with verified entities and transitions.

## P2 - Two native gameplay kernels

- define separate MZM and Aria engine adapters;
- implement native player movement, collision, damage and room lifecycle;
- preserve each world's timing and mechanical rules;
- implement native Samus in MZM and native Soma in Aria first.

Exit criterion: each native character can complete a source-authentic test path
in its own engine without emulation.

## P3 - Crossover prologues

- implement Samus against Creaking Skull in Aria rules;
- implement Soma against mandatory Deorem in MZM rules;
- add both portal rewards and the authored Interzone scene;
- persist order-independent prologue completion.

## P4 - Duo campaign

- instantiate both protagonists in one active world engine;
- add character switching, companion AI and softlock recovery;
- implement cross-equipment and alien soul mappings;
- connect verified save rooms through authored world links;
- implement the full concurrent timeline and both secret epilogues.

## P5 - Production

- complete content verification and balancing;
- version persistent saves and migrations;
- add accessibility, input configuration, credits and combined statistics;
- ship only source/tools and require local extraction of proprietary assets.
