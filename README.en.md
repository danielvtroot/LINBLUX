<p align="center"><img src="midia/logo.png" width="112" alt="LINBLUX logo"/></p>
<h1 align="center">LINBLUX Studio</h1>
<p align="center">An experimental 3D creation engine for Linux.</p>
<p align="center"><strong>Alpha 0.1.0 — an experimental open-source project.</strong></p>
<p align="center"><a href="README.md">Português brasileiro</a> · <a href="docs/README.md">Documentation</a> · <a href="docs/MIDIA.md">Gallery</a> · <a href="#build">Build from source</a> · <a href="CONTRIBUTING.md">Contribute</a></p>

<p align="center"><a href="docs/prints/studio-construcao.png"><img src="docs/prints/studio-construcao.png" width="880" alt="LINBLUX Studio: 3D construction of a room, Explorer hierarchy and properties panel"/></a></p>

<p align="center">
<a href="docs/prints/explorador-servicos.png"><img src="docs/prints/explorador-servicos.png" width="404" alt="Explorer and services: ServerStorage with Items and a Sword model selected"/></a>
<a href="docs/prints/explorador-servicos-automaticos.png"><img src="docs/prints/explorador-servicos-automaticos.png" width="404" alt="Storage services organized automatically in the Explorer"/></a>
</p>
<p align="center"><sub><strong>LINBLUX Studio — 3D construction</strong> · <strong>Explorer and services</strong> · <strong>Organized storage services</strong></sub></p>

## Watch LINBLUX running

The demo is published on YouTube: **[Watch the LINBLUX demo](https://www.youtube.com/watch?v=jVck4UuYlWQ)**,
on the [Abismroot](https://www.youtube.com/@Abismroot) channel. GitHub does not play
videos stored in the repository inside a README, so the player opens in a new tab.

The original recording (`docs/videos/linblux.mp4`, 1920×1080, about 8 minutes and
126 MiB) stays in the development environment only and is **not part of Git**: it
exceeds the 20 MiB gallery limit and GitHub's 100 MiB per-file limit. Nothing was
transcoded or duplicated. Music, images and other third-party media used in the
video must be cleared for publication by the channel owner.

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
| Objects | Parts, Models, persistent IDs, Attachments and local storage services |
| Runtime | Separate Player; Play/Stop runs a copy of the authored scene |
| Lua 5.4 | Objects, events, physics, NPCs, inventory, audio and UI within the current API |
| Resources | PNG/JPEG, Decals, tiled textures, local sounds and music |
| Experiences | Local library, saving/reopening and `.lo` import/export |
| Prototypes | Models in ServerStorage/Itens remain outside rendering/physics and spawn through inventory |
| Characters | Six-body rig, compatible animations and NPCs |

Moving a face moves the whole Part without resizing it. Face scaling keeps the
opposite face fixed. The visual grid has been removed; transform Snap and
surface placement remain available. Existing object names are preserved.

## Demonstration

[RifleDemo](assets/examples/rifledemo/README.md) demonstrates Lua, items, NPCs and
Player interaction. Player validation results are in the
[Alpha notes](docs/ALPHA_0_1_0.md); public Studio captures are in the
[media gallery](docs/MIDIA.md).

The gallery is generated from the files in `docs/prints/` and `docs/videos/`:
run `python3 tools/update_readme_media.py`. The script reads metadata only, never
transcodes, and never adds files to Git. Large videos belong in Releases or a
video service so they do not increase clone size.

## Try it on Linux

**A public binary package is still blocked pending dependency redistribution
review.** No approved binary download is available, and there is no universal
Linux package or published binary tag/Release. See
[Alpha status](docs/ALPHA_0_1_0.md). Source builds work on Linux.

After building, open the Studio, create an experience and press **F5** to run it;
**Shift+F5** stops it. In the editor, right mouse button + WASD moves the camera,
Q/E moves down/up and F frames the selection. In the Player, WASD moves, space
jumps and Esc opens the menu while simulation continues.

### Build

<a id="build"></a>

The official development build directory is `build-linblux/`. See
[build requirements](docs/BUILD.md) before configuring. The validation environment
uses Linux x86_64, C++17, raylib 6.2.0, Bullet 3.24 and Lua 5.4; this does not
prove support for every distribution. RmlUi is fetched from a pinned revision, so
first configuration needs network access or a cached copy.

```sh
git clone https://github.com/danielvtroot/LINBLUX.git
cd LINBLUX
cmake -S . -B build-linblux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linblux -j2
./build-linblux/linblux
./build-linblux/linblux_player --project assets/examples/rifledemo/world.loblox.json
```

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

LINBLUX includes local maintenance tools accessible from the terminal.
**ATLAS** indexes repository structure and
finds files, symbols and possible bug causes, suggests evidence-based hypotheses
and reports impact. Its textual relationships do not prove runtime calls or
automatically fix bugs.

**TURBO** selects the checks relevant to a change and runs them with individual
timeouts, a global budget, process cleanup and logs. It can avoid unnecessary
repeated work; speed and coverage depend on the change and environment. Partial
coverage is never a full pass. Everything runs locally, with no external service.
**HDD-Safe** mode reduces writes and reindexing on mechanical disks; HDD-Critical
mode allows read-only queries.

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

Source code and documentation are published on `main`. The **public binary
distribution remains blocked** by the linked FFmpeg redistribution obligations and
by completing the notices for embedded components; no approved download and no
universal Linux package exist. No binary tag or Release has been published.

Tests using software rendering do not establish physical GPU performance.
A null audio backend does not establish audible playback. Run only trusted
experiences: Lua can access local engine capabilities. There is no automatic
cross-session inventory persistence or production multiplayer service.

## Next steps

Consolidate the Studio and the engine, improve stability, rendering and
performance, make 3D construction more intuitive, and expand characters, Lua
scripts, terrain and mesh import. Later, evolve Linux packaging and explore online
experiences — multiplayer, accounts, discovery and collaboration with an
authoritative server. These are future goals without promised dates, and none of
them is available in this Alpha. [Full roadmap](docs/ROADMAP.md).

## Documentation, contributions and license

Start at the [documentation index](docs/README.md). Bug reports should include
the commit, Linux environment, reproduction steps and a minimal scene without
private data. Read [CONTRIBUTING](CONTRIBUTING.md), [security guidance](SECURITY.md)
and the [changelog](docs/CHANGELOG.md). The interface and most manual pages are in
Brazilian Portuguese.

Original project code is [MIT licensed](LICENSE). Dependency copyrights and
notices are preserved. LINBLUX uses raylib, Bullet, Lua, RmlUi, Dear ImGui,
rlImGui, stb and nlohmann/json. Optional FFmpeg licensing depends on the linked
build. See [assets and licenses](docs/ASSETS_E_LICENCAS.md) and the
[launch report](docs/releases/ALPHA_0_1_0_LANCAMENTO.md) for distribution limits.
