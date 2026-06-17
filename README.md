# Cathuluvania

A minimal raylib C 2D platformer with Aseprite layer-driven acts, parallax scrolling, and horizontal camera follow.

## Controls

- **A / ←** – Move left
- **D / →** – Move right
- **Space / W / ↑** – Jump (coyote time, jump buffer, double jump)
- **R** – Respawn at act spawn point
- **Top-left menu** – Switch act (Green Act / Dark Act)
- **Esc** – Quit

Wall slide activates automatically when falling against a wall.

## Level authoring

Acts live as Aseprite files:

```
resources/visual/green-act.aseprite
resources/visual/dark-act.aseprite
```

Layer names:

- **backdrop** — `background`, `parallax` / `parralax`
- **collision** — `collision`, `base`, or `earth-tileset`

Export with Aseprite (CLI or GUI script):

```bash
make assets
```

This produces per act under `resources/visual/layers/`:

- `<act>-background.png` — backdrop + parallax
- `<act>.png` — collision + render (opaque 16×16 cells = solid)
- `<act>.export.json` — auto-generated dimensions and paths

Game metadata (label, spawn) is hand-written in [`src/acts_metadata.h`](src/acts_metadata.h). Build merges export JSON + metadata into [`src/acts.gen.h`](src/acts.gen.h) via Lua — no runtime JSON parsing.

**Tip:** Keep canvas width and height divisible by 16 for clean tile collision.

## Display

- Viewport: **320×240** pixels (camera follows player; narrow rooms are centered in view)
- Default window: **960×720** (3× integer upscale); resizable with a **960×720** minimum
- Scales up in whole-number steps to fit the window; **4:3** aspect is preserved with letterboxing on wider screens
- **F11** — toggle fullscreen | **Alt+Enter** — toggle borderless windowed fullscreen
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
- `scripts/vendor_raylib.sh` — clone/pin raylib 6.0 into `external/`
