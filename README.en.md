<p align="center"><img src="midia/logo.png" width="112" alt="LINBLUX logo"/></p>
<h1 align="center">LINBLUX Studio</h1>
<p align="center">An experimental 3D creation environment for Linux.</p>
<p align="center"><strong>Alpha 0.1.0 · local experiences · Brazilian Portuguese interface</strong></p>
<p align="center"><a href="README.md">Português brasileiro</a> · <a href="docs/ALPHA_0_1_0.md">Release status</a> · <a href="docs/README.md">Documentation</a> · <a href="docs/MIDIA.md">Gallery</a> · <a href="CONTRIBUTING.md">Contribute</a></p>

<p align="center"><img src="docs/prints/studio-construcao.png" width="960" alt="LINBLUX Studio with a 3D scene, hierarchy and properties"/></p>

## What is LINBLUX?

LINBLUX combines a Studio for editing worlds with a separate Player for running
them. Build scenes from Parts and Models, organize their hierarchy, change
properties and write behavior in Lua. The local engine includes physics, images,
sounds and a basic character system.

This is an experimental engine, developed through a short prototyping cycle on
limited hardware. Its purpose is to establish a concrete local foundation that
can be understood, changed and expanded into a broader creation platform.
The Alpha is intended for small experiments and contributions. Keep copies of
important projects; editing, distribution and compatibility still have limitations.

## Available in the Alpha codebase

| Area | Current scope |
| --- | --- |
| Studio | 3D scenes, searchable hierarchy, selection, properties and script editing |
| Construction | Face-based Move/Scale, Rotate, incremental Snap and surface placement |
| Objects | Parts, Models, grouping, persistent IDs and Attachments |
| Runtime | Separate Player; Play/Stop runs a copy of the authored scene |
| Lua 5.4 | Objects, events, physics, NPCs, inventory, audio and UI within the current API |
| Resources | PNG/JPEG, Decals, tiled textures, local sounds and music |
| Experiences | Local library, saving/reopening and `.lo` import/export |
| Characters | Six-body rig, compatible animations and NPCs |

Moving a face moves the whole Part without resizing it. Face scaling keeps the
opposite face fixed. The visual grid has been removed; transform Snap and
surface placement remain available. Existing object names are preserved.

## See it running

<p align="center"><img src="docs/screenshots/alpha-rifledemo-player.png" width="800" alt="RifleDemo running in the standalone Player"/></p>

[RifleDemo](assets/examples/rifledemo/README.md) demonstrates Lua, items, NPCs
and Player interaction. This screenshot comes from an earlier candidate; consult
the [Alpha notes](docs/ALPHA_0_1_0.md) for current validation results.
The [media gallery](docs/MIDIA.md) contains actual captures. Add reviewed public
media to `docs/prints/` or `docs/videos/`, then run
`python3 tools/update_readme_media.py`. Large videos should be hosted separately.

## Try it on Linux

**A public binary package is still blocked pending dependency redistribution
review.** No approved binary download is advertised here. Source builds are
available; see [Alpha status](docs/ALPHA_0_1_0.md).

The official development build directory is `build-linblux/`. See
[build requirements](docs/BUILD.md) before configuring. The validation environment
uses Linux x86_64, C++17, raylib 6.2.0, Bullet 3.24 and Lua 5.4. RmlUi is fetched
from a pinned revision; first configuration needs network access or a cached copy.

```sh
git clone https://github.com/danielvtroot/LINBLUX.git
cd LINBLUX
cmake -S . -B build-linblux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linblux -j2
./build-linblux/linblux
./build-linblux/linblux_player --project assets/examples/rifledemo/world.loblox.json
```

In the Studio, right mouse button + WASD moves the camera; Q/E moves down/up,
F frames the selection, F5 starts Play and Shift+F5 stops it. In the Player,
WASD moves, space jumps and Esc opens the menu while simulation continues.
The interface and most manuals are currently in Brazilian Portuguese.

## Architecture

C++17 and CMake organize the engine. raylib/OpenGL renders scenes, Bullet
simulates physics, and Lua 5.4 runs scripts. Studio uses RmlUi for its main
panels and ImGui for specialized editors. Player does not link the Studio UI.

`Scene` owns objects and persistent IDs. `ProjectIO` validates scenes and saves
transactionally. `RuntimeSession` owns an execution copy, keeping authoring data
separate from Play. Experience management handles local manifests and resources.
See [Architecture](docs/ARCHITECTURE.md) for targets and remaining coupling.

## ATLAS + TURBO

LINBLUX includes local maintenance tools for developers and terminal-capable
agents, independent of any external service provider. **ATLAS** indexes repository structure,
finds code and symbols, suggests evidence-based hypotheses and reports impact.
Its textual relationships do not prove runtime calls or automatically fix bugs.

**ATLAS TURBO** selects checks and supervises tests with individual timeouts,
a global budget, process cleanup and logs. It can avoid unnecessary repeated
work; speed and coverage depend on the change and environment. Partial coverage
is never a full pass. HDD-Safe and HDD-Critical modes reduce index I/O.

```sh
ATLAS_IO_MODE=hdd-safe python3 tools/atlas/atlas.py diagnose "problem" --brief
python3 tools/atlas/atlas.py tests-for src/editor/EditorUI.cpp
python3 tools/turbo/turbo.py test --mode fast --changed --plan
```

[ATLAS](docs/atlas/README.md) · [TURBO](docs/turbo/README.md) · [Testing](docs/TESTING.md)

## Status and direction

| Status | Scope |
| --- | --- |
| Implemented | Local authoring, Player, persistence, physics, Lua and resources listed above |
| Experimental | Surface construction, characters/NPCs, materials, Lua UI and tool integration |
| Limited | Six-body rigs, box surface snapping, no 3D audio mixing/attenuation; Linux portability still needs wider validation |
| Future | Arbitrary mesh import, terrain, multiplayer, accounts and online publishing |

Tests using software rendering do not establish physical GPU performance.
A null audio backend does not establish audible playback. Run only trusted
experiences: Lua can access local engine capabilities. There is no automatic
cross-session inventory persistence or production multiplayer service.

The [roadmap](docs/ROADMAP.md) starts with engine/Studio stability, rendering,
more intuitive tools, characters, 3D import, terrain, Lua and Linux packaging.
Later goals include authoritative servers, identity, world replication,
experience discovery, collaboration and extensions. No delivery dates are promised.

## Documentation, contributions and license

Start at the [documentation index](docs/README.md). Bug reports should include
the commit, Linux environment, reproduction steps and a minimal scene without
private data. Read [CONTRIBUTING](CONTRIBUTING.md), [security guidance](SECURITY.md)
and the [changelog](docs/CHANGELOG.md).

Original project code is [MIT licensed](LICENSE). Dependency copyrights and
notices are preserved. LINBLUX uses raylib, Bullet, Lua, RmlUi, Dear ImGui,
rlImGui, stb and nlohmann/json. Optional FFmpeg licensing depends on the linked
build. See [assets and licenses](docs/ASSETS_E_LICENCAS.md) and the
[launch report](docs/releases/ALPHA_0_1_0_LANCAMENTO.md) for distribution limits.
