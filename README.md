# Cathuluvania

A minimal raylib C 2D platformer with Aseprite layer-driven acts, parallax scrolling, and horizontal camera follow.

## Controls

- **A / ←** – Move left
- **D / →** – Move right
- **Space / W / ↑** – Jump (coyote time, jump buffer, double jump)
- **R** – Respawn at act spawn point
- **Top-left menu** – Switch act (Green Act / Dark Act)
- **Tab** – Toggle gameplay editor (see below)
- **Esc** – Quit (exit editor when editor is active)

Wall slide activates automatically when falling against a wall. Climbable tiles slow wall-slide speed; slippery tiles reduce ground friction.

## Gameplay editor

Press **Tab** to enter editor mode (player physics paused, free camera pan).

**Collision geometry** comes from exported `base` (tile art) plus `<act>-primitives.png` (editor-painted solids). The in-game **B** / **E** tools edit only the **primitives overlay**; they never overwrite `base` or `template.aseprite`.

**Tags** (slippery, climbable, etc.) are stored in `<act>.gameplay.json` and apply on top of existing solid tiles from Aseprite.

| Key | Action |
|-----|--------|
| **Tab** / **Esc** | Exit editor |
| **S** | Select solid tiles (Shift+click multi-select) |
| **Tag panel** | Click flags on selected tiles |
| **B** | Paint collision on **primitives** layer |
| **E** | Erase **primitives** layer cell |
| **WASD / arrows** | Pan camera |
| **Ctrl+S** | Save tags + collision → `<act>-primitives.png` + `.gameplay.json` |
| **Ctrl+N** | Create new empty act |

On **Ctrl+S**, collision edits are written to `resources/visual/layers/<act>-primitives.png`. The game loads that PNG on restart. **`template.aseprite` is not modified** — rewriting it from the game was corrupting indexed/tilemap layers on `base`. Edit tile art in Aseprite; use the in-game editor only for quick collision layout.

To import editor collision into Aseprite manually: open the act, import `layers/<act>-primitives.png` as a reference on the **primitives** layer (or run `make assets` after hand-copying the PNG content into the file in Aseprite).

### Creating a new act

**Template file:** [`resources/visual/template.aseprite`](resources/visual/template.aseprite) — copy of your standard act (layers, tileset, slices). Included in `make assets`.

**In-game:** **Tab** → **Ctrl+N** copies `template.aseprite` → `new-act-NNN.aseprite`, exports PNGs, updates the manifest, and loads the new act.

**Manual:**

```bash
cp resources/visual/template.aseprite resources/visual/my-act.aseprite
# edit in Aseprite, then:
make assets   # or export one file:
aseprite -b resources/visual/my-act.aseprite -script scripts/aesprite/export-act-level.lua
```

Add `{ "id": "my-act", "label": "My Act" }` to [`resources/acts.manifest.json`](resources/acts.manifest.json) and (for `make`) [`src/acts_metadata.h`](src/acts_metadata.h) + `ACTS` in the Makefile.

## Level authoring

Acts live as Aseprite files:

```
resources/visual/green-act.aseprite
resources/visual/dark-act.aseprite
```

Layer names:

- **backdrop** — `background`, `parallax` / `parralax`
- **collision** — `primitives`, `collision`, `base`, or `earth-tileset`

Export with Aseprite (CLI or GUI script):

```bash
make assets
```

This produces per act under `resources/visual/layers/`:

- `<act>-background.png` — backdrop + parallax
- `<act>.png` — base tile art (opaque 16×16 cells = solid)
- `<act>-primitives.png` — editor-painted collision overlay
- `<act>.export.json` — auto-generated dimensions and paths
- `<act>.gameplay.json` — tag overrides only (slippery, climb, …); editable in-game

Game metadata (label, spawn) is hand-written in [`src/acts_metadata.h`](src/acts_metadata.h). Build merges export JSON + metadata into [`src/acts.gen.h`](src/acts.gen.h) via Lua. Gameplay JSON is loaded at runtime (and saved from the editor).

**Tip:** Keep canvas width and height divisible by 16 for clean tile collision.

## Display

- Viewport: **320×240** pixels (camera follows player; narrow rooms are centered in view)
- Default window: 4× integer upscale
- Parallax background scrolls slower than the camera

## Rooms and tunnels

Connected rooms use `r-<n>` slices; isolated rooms use `ir-<n>`. Connectors between rooms are **tunnels** (`door-<id>` or `tunnel-<id>` slices in Aseprite — both names are accepted). Crossing a tunnel instantly switches the active room and pans the camera toward the tunnel/destination (no fade). Teleports (`teleport-<id>`) still use a fade transition for isolated-room warps.

Use `level_get_active_zone()` for the current combat room and `level_zone_at()` to query whether a position is in a room or tunnel (for future enemy spawning).

## Build

### Prerequisites

- C compiler (clang/gcc)
- [Aseprite](https://www.aseprite.org/) – for `make assets` (export + registry codegen)
- **Network:** The first build clones [raylib 6.0](https://github.com/raysan5/raylib) into `external/raylib-master/` (ignored by git). Later builds reuse it.

### Commands

```bash
make vendor-raylib   # optional; `make` runs this automatically
make assets          # Export PNGs + regenerate src/acts.gen.h (requires aseprite on PATH)
make                 # Build → bin/Debug/Cathuluvania
make run             # Build and run
make clean           # Remove build artifacts
make app-bundle      # macOS .app for distribution
```

### Windows (Zig)

```batch
build-zig.bat
```

### Web (Emscripten)

Requires [Emscripten](https://emscripten.org/) (`emcc` on `PATH`). Raylib is vendored into `external/raylib-master` on first build.

```bash
make -f Makefile.web all
make -f Makefile.web package
python3 -m http.server 8000 --directory bin/wasm
```

## CI/CD

Cross-platform builds are in `.github/workflows/cathuluvania-cicd.yml` (web, macOS, Windows). Publishing to itch.io/S3 is disabled by default.

## Structure

- `external/raylib-master/` — raylib 6.0 sources (`make vendor-raylib` or any build target)
- `external/cjson/` — cJSON for gameplay editor save/load
- `resources/gameplay/tile-catalog.json` — tile primitive definitions (types + tags)
