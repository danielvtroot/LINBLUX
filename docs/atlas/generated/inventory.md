# Inventário gerado pelo ATLAS

> Arquivo regenerável. Não edite manualmente; execute `python3 tools/atlas/atlas.py docs`.

## Contagens

- files: 235
- symbols: 2505
- relations: 895
- cmake_targets: 34
- tests: 34
- external_deps: 14

## Módulos

| Módulo | Arquivos | Linhas |
| --- | ---: | ---: |
| docs | 68 | 6235 |
| tests | 30 | 8823 |
| editor | 21 | 7217 |
| ui | 14 | 2125 |
| atlas | 12 | 4675 |
| objects | 12 | 848 |
| game | 11 | 2102 |
| ui-assets | 9 | 1228 |
| agents | 8 | 280 |
| core | 7 | 536 |
| root | 6 | 434 |
| rendering | 5 | 1700 |
| assets-templates | 4 | 4833 |
| scripts | 4 | 109 |
| serialization | 4 | 1450 |
| assets-defaults | 3 | 2498 |
| packaging | 3 | 31 |
| app | 2 | 256 |
| experiences | 2 | 793 |
| media | 2 | 160 |
| physics | 2 | 607 |
| scene | 2 | 738 |
| scripting | 2 | 543 |
| build | 1 | 387 |
| player | 1 | 61 |

## Símbolos por tipo

- function: 1079
- method: 734
- heading: 327
- ui-element: 165
- struct: 92
- constructor: 38
- class: 30
- enum-class: 25
- using: 11
- enum: 3
- interface: 1

## Targets CMake

- `linblux` (executable)
- `linblux_core` (interface)
- `linblux_decal_render_tests` (executable)
- `linblux_defaults_tests` (executable)
- `linblux_editing` (static)
- `linblux_editor` (library)
- `linblux_editor_input_tests` (executable)
- `linblux_engine` (static)
- `linblux_engine_default_tests` (executable)
- `linblux_experience_tests` (executable)
- `linblux_experience_ui_tests` (executable)
- `linblux_game_tests` (executable)
- `linblux_game_ui_tests` (executable)
- `linblux_gizmo_visual_tests` (executable)
- `linblux_graphics_quality_tests` (executable)
- `linblux_home_video_tests` (executable)
- `linblux_imgui` (static)
- `linblux_manipulation_tests` (executable)
- `linblux_media` (static)
- `linblux_player` (executable)
- `linblux_presentation` (static)
- `linblux_renderer` (static)
- `linblux_rig_image_tests` (executable)
- `linblux_rig_input_tests` (executable)
- `linblux_rml_gl3` (static)
- `linblux_rml_ui_tests` (executable)
- `linblux_sky_capture_tests` (executable)
- `linblux_spawn_tests` (executable)
- `linblux_studio` (static)
- `linblux_studio_tests` (executable)
- `linblux_studio_ui_tests` (executable)
- `linblux_tests` (executable)
- `linblux_video_tests` (executable)
- `linblux_visual_studio_tests` (executable)

## Testes declarados no CMake

- `linblux_atlas` (condicional)
- `linblux_decal_render` (condicional) — `tests/decal_render_main.cpp`
- `linblux_defaults` — `tests/defaults_main.cpp`
- `linblux_editor_input` (condicional) — `tests/x11_focus.cpp`
- `linblux_editor_restart` (condicional) — `src/app/Application.cpp`
- `linblux_engine_default_lifecycle` — `tests/engine_default_main.cpp`
- `linblux_engine_default_restart` (condicional) — `src/app/Application.cpp`
- `linblux_experience_startup` (condicional) — `src/app/Application.cpp`
- `linblux_experience_ui` (condicional) — `tests/experience_ui_main.cpp`
- `linblux_experiences` — `tests/experiences_main.cpp`
- `linblux_game` — `tests/game_main.cpp`
- `linblux_game_ui` (condicional) — `tests/x11_focus.cpp`
- `linblux_gizmo_visual` (condicional) — `tests/gizmo_visual_main.cpp`
- `linblux_graphics_quality` (condicional) — `tests/graphics_quality_main.cpp`
- `linblux_graphics_quality_1080` (condicional) — `tests/graphics_quality_main.cpp`
- `linblux_help` — `src/app/Application.cpp`
- `linblux_home_video` (condicional) — `tests/home_video_main.cpp`
- `linblux_manipulation` — `tests/manipulation_main.cpp`
- `linblux_manipulation_ui` (condicional) — `tests/x11_focus.cpp`
- `linblux_mouse_lock` (condicional) — `tests/x11_focus.cpp`
- `linblux_player_help` — `src/player/main.cpp`
- `linblux_player_validate` — `src/player/main.cpp`
- `linblux_rig_decals_ui` (condicional) — `tests/x11_focus.cpp`
- `linblux_rig_image` (condicional) — `tests/rig_image_main.cpp`
- `linblux_rig_input` (condicional) — `tests/rig_input_main.cpp`
- `linblux_rml_ui` (condicional) — `tests/x11_focus.cpp`
- `linblux_sky_capture` (condicional) — `tests/sky_capture_main.cpp`
- `linblux_spawn` — `tests/spawn_main.cpp`
- `linblux_startup_no_display` — `src/app/Application.cpp`
- `linblux_studio` — `tests/studio_main.cpp`
- `linblux_studio_ui` (condicional) — `tests/x11_focus.cpp`
- `linblux_tests` — `tests/test_main.cpp`
- `linblux_video` (condicional) — `tests/video_main.cpp`
- `linblux_visual_studio` (condicional) — `tests/visual_studio_main.cpp`

## Dependências externas inventariadas

- FFMPEG (cmake-pkg_check_modules)
- LUA (cmake-pkg_check_modules)
- OpenGL (cmake-find_package)
- PkgConfig (cmake-find_package)
- Python3 (cmake-find_package)
- RAYLIB (cmake-pkg_check_modules)
- Threads (cmake-find_package)
- X11 (cmake-find_package)
- ZLIB (cmake-find_package)
- imgui (vendored)
- nlohmann (vendored)
- rlimgui (vendored)
- rmlui (fetchcontent)
- stb (vendored)

## Contratos manuais esperados

- [x] `app`
- [x] `core`
- [x] `editor`
- [x] `experiences`
- [x] `game`
- [x] `media`
- [x] `objects`
- [x] `physics`
- [x] `player`
- [x] `rendering`
- [x] `scene`
- [x] `scripting`
- [x] `serialization`
- [x] `ui`
