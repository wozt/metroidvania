# romdev — Native GBA Inspection and Debugging

romdev (`romdevtools` npm package, pinned in `tools/romdev/package.json`) is an
MCP/HTTP server that bundles the mGBA core plus disassembly, memory, video and
save-state tools. It gives agents direct, scriptable access to the original GBA
execution for native-parity work on Aria of Sorrow and Metroid: Zero Mission.

It complements `build/aos_mgba_lab`: keep using the lab for deterministic
checkpoints, validated frame captures and project regression tests. Use romdev
for interactive CPU debugging, instruction traces, breakpoints/watchpoints,
memory searches, OAM/palette/BG inspection and agent-driven investigation.

Save states are NOT assumed compatible between romdev's bundled mGBA and the
lab's mGBA build. If a lab checkpoint does not load, reproduce the state with
romdev inputs instead of mixing the two.

## Install (one time)

```sh
cd tools/romdev && npm ci
```

Requires Node.js 24+. `package.json` pins `romdevtools` (currently 0.156.3);
`package-lock.json` is committed. `node_modules/` is gitignored.

## Start / stop / health

```sh
scripts/romdev_start.sh    # idempotent; binds http://127.0.0.1:7331 only
scripts/romdev_health.sh   # checks /documentation + MCP initialize
scripts/romdev_stop.sh     # stops the server on port 7331
```

Endpoints: MCP `http://127.0.0.1:7331/mcp`, docs `/documentation`,
livestream `/livestream`, OpenAPI `/openapi.json`.

## Connect Devin

`.devin/mcp_config.json` (committed, project scope) registers `romdev` as a
Streamable HTTP MCP server at `http://127.0.0.1:7331/mcp`. Start a new Devin
session after `scripts/romdev_start.sh` so the `mcp__romdev__*` tools are
discovered. The existing `godot` server in the user-level config is unrelated
and untouched.

## One-shot calls without MCP registration

`scripts/romdev_call.sh <tool> '<json-args>'` performs the MCP handshake
(initialize → notifications/initialized → tools/call) in a single shot. The
emulator host and loaded media persist across calls, so multi-step sessions
work:

```sh
scripts/romdev_call.sh loadMedia '{"platform":"gba","path":"roms/Castlevania - Aria of Sorrow (USA).gba"}'
scripts/romdev_call.sh frame    '{"op":"step","frames":60}'
scripts/romdev_call.sh memory   '{"op":"read","region":"gba_io_regs","address":"0","length":4}'
scripts/romdev_call.sh sprites  '{"op":"inspect"}'
scripts/romdev_call.sh state    '{"op":"save","path":"/tmp/aos_room05.state"}'
```

`memory` `address` is an OFFSET within the named region (e.g.
`system_ram` offset `0x1261C`), not a GBA absolute address. Region sizes are
listed by `memory({op:"regions"})`.

## Verified on GBA (romdevtools 0.156.3, mGBA core)

- `loadMedia` Aria ROM: loads, `core:"mgba"`, 240x160 framebuffer.
- `frame` step/screenshot/stepAndShot; title screen reached after ~720 frames.
- `memory` regions: `system_ram` (EWRAM), `gba_iwram`, `gba_io_regs`
  (DISPCNT etc.), `gba_palette`, `gba_oam`, `video_ram`, `save_ram`.
- `sprites({op:"inspect"})`: live OAM entries (x, y, tile, palette,
  priority, size, affine, visibility).
- `palette({op:..., source:"live", inline:true})`: live BG/OBJ palette colors.
- `background({op/view:"renderState"})`: DISPCNT, BG enables, per-layer
  priority/charBase/mapBase/size/colorMode.
- `cpu({op:"read"})`: ARM/Thumb register file and flags.
- `breakpoint({on:"pc", address})`: PC breakpoints with hit report and
  main-thread PC histogram.
- `frame({op:"stepInstructions"})` / `stepInstruction`: instruction stepping
  with PC trace.
- `state` save/load/list, in-memory slots and disk paths (disk states survive
  server restarts; reload with `op:"load", path` after loading the same ROM).

## Not yet verified / limitations

- ARM/Thumb instruction *tracing* (`traceInstructions`), memory write
  watchpoints, `disasm`/`decompile`/`symbols`/`romPatch` on the GBA core:
  advertised by the tool schemas but not exercised yet in this project.
- Whether romdev disk states load in `aos_mgba_lab` (different mGBA builds) —
  assume incompatible until proven otherwise.
- `romdev` must be started manually before an agent session; the MCP client
  does not auto-start it. If tools are missing, run `scripts/romdev_start.sh`
  and restart the Devin session.

## ROM privacy

Never commit ROMs, saves or captured proprietary frames. `roms/` stays
local-only per `.gitignore`. romdev keeps everything on localhost; do not
expose port 7331 beyond the machine.

## Example: inspect native room state

```sh
scripts/romdev_start.sh
scripts/romdev_call.sh loadMedia '{"platform":"gba","path":"roms/Castlevania - Aria of Sorrow (USA).gba"}'
scripts/romdev_call.sh frame '{"op":"step","frames":720}'   # reach title
# drive inputs / load a state, then inspect:
scripts/romdev_call.sh background '{"view":"renderState"}'
scripts/romdev_call.sh memory '{"op":"read","region":"gba_palette","address":"0xA0","length":32}'
```
