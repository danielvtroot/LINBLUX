cmake_minimum_required(VERSION 3.16)
project(loblox_open_source VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(RAYLIB REQUIRED raylib)
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua5.4)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# RmlUi 6.3, immutable source + digest; no samples, browser, GLFW backend or Lua VM.
include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(rmlui
  URL https://codeload.github.com/mikke89/RmlUi/tar.gz/ba95ffe8bfb6370efb2cdcca927eaad4710c5413
  URL_HASH SHA256=1541ef5577115e9368f8ed389b29f0925ef6572f326a33d378ea16c3cfa2cde8)
FetchContent_MakeAvailable(rmlui)
add_library(loblox_rml_gl3 STATIC ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp)
target_include_directories(loblox_rml_gl3 PUBLIC ${rmlui_SOURCE_DIR}/Backends)
target_link_libraries(loblox_rml_gl3 PUBLIC RmlUi::Core OpenGL::GL ${CMAKE_DL_LIBS})
# The RmlUi GL3 backend draws the interface into its own layer framebuffer and
# resolves it to the postprocess target before touching raylib's backbuffer.
# The circular gameplay buttons and the health bar are border-radius geometry,
# so window MSAA flags cannot smooth them; the layer target itself needs MSAA.
# Four samples is the smallest count OpenGL 3.3 guarantees (MAX_SAMPLES >= 4),
# so a conforming context always accepts it, and the cost is limited to the
# interface layer (never the 3D scene). The layer target is genuinely larger:
# around 80 MiB at 1920x1080 (multisampled colour and depth/stencil plus the
# two resolve targets) and around 42 MiB at 1366x768, which still fits the 3 GB
# target machine. Lighter alternative: -DLOBLOX_RMLUI_MSAA_SAMPLES=2, at the
# price of visibly fewer coverage levels; 0 is the build-time fallback for a
# driver that cannot create a multisample target, and the fixtures then report
# the hard-edged result.
set(LOBLOX_RMLUI_MSAA_SAMPLES "4" CACHE STRING
    "MSAA samples for the RmlUi layer framebuffer (0 disables MSAA)")
target_compile_definitions(loblox_rml_gl3 PRIVATE
    RMLUI_NUM_MSAA_SAMPLES=${LOBLOX_RMLUI_MSAA_SAMPLES})

# Bullet Physics: Debian/Ubuntu install the headers under /usr/include/bullet.
find_path(BULLET_INCLUDE_DIR NAMES btBulletDynamicsCommon.h
  HINTS /usr/include/bullet /usr/local/include/bullet)
find_library(BULLET_DYNAMICS_LIBRARY NAMES BulletDynamics)
find_library(BULLET_COLLISION_LIBRARY NAMES BulletCollision)
find_library(BULLET_MATH_LIBRARY NAMES LinearMath)
if(NOT BULLET_INCLUDE_DIR OR NOT BULLET_DYNAMICS_LIBRARY OR NOT BULLET_COLLISION_LIBRARY OR NOT BULLET_MATH_LIBRARY)
  message(FATAL_ERROR "Bullet Physics nao encontrado. Instale 'libbullet-dev' (apt).")
endif()
set(BULLET_LIBRARIES ${BULLET_DYNAMICS_LIBRARY} ${BULLET_COLLISION_LIBRARY} ${BULLET_MATH_LIBRARY})

# Dear ImGui + rlImGui backend (vendored, versao fixa, funciona offline).
add_library(loblox_imgui STATIC
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp
  third_party/imgui/imgui_stdlib.cpp
  third_party/rlimgui/rlImGui.cpp
)
target_include_directories(loblox_imgui PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rlimgui
  ${RAYLIB_INCLUDE_DIRS}
)
target_compile_definitions(loblox_imgui PUBLIC NO_FONT_AWESOME)
target_link_libraries(loblox_imgui PUBLIC ${RAYLIB_LIBRARIES} OpenGL::GL)

# Camada sem interface: modelo de objetos, cena, fisica e persistencia.
add_library(loblox_core STATIC
  src/scripting/LuaRuntime.cpp
  src/game/Rig.cpp
  src/game/GameAssetsIO.cpp
  src/game/ImageAssets.cpp
  src/game/Runtime.cpp
  src/game/Spawn.cpp
  src/core/Diagnostics.cpp
  src/objects/Instance.cpp
  src/objects/Part.cpp
  src/objects/Workspace.cpp
  src/objects/Lighting.cpp
  src/objects/Baseplate.cpp
  src/objects/SpawnLocation.cpp
  src/scene/Scene.cpp
  src/physics/PhysicsWorld.cpp
  src/serialization/ProjectIO.cpp
  src/serialization/SceneLua.cpp
  src/editor/GizmoMath.cpp
  src/editor/GizmoLayout.cpp
  src/editor/Gizmo.cpp
  src/editor/TransformSession.cpp
  src/rendering/EditorCamera.cpp
)
target_include_directories(loblox_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${BULLET_INCLUDE_DIR}
  ${RAYLIB_INCLUDE_DIRS}
)
target_link_libraries(loblox_core PUBLIC ${BULLET_LIBRARIES} ${RAYLIB_LIBRARIES} OpenGL::GL Threads::Threads PkgConfig::LUA)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(loblox_core PRIVATE -Wall -Wextra)
endif()

add_library(loblox_editor STATIC
  src/ui/StudioFrontend.cpp
  src/ui/EditorActions.cpp
  src/ui/RmlFrontendSupport.cpp
  src/rendering/SceneRenderer.cpp
  src/editor/EditorUI.cpp
  src/editor/GamePanels.cpp
  src/editor/StudioPanels.cpp
)
target_link_libraries(loblox_editor PUBLIC loblox_core loblox_imgui loblox_rml_gl3 OpenGL::GL)
target_compile_definitions(loblox_editor PRIVATE LOBLOX_UI_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/ui")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(loblox_editor PRIVATE -Wall -Wextra)
endif()
add_executable(loblox src/main.cpp src/app/Application.cpp)
target_link_libraries(loblox PRIVATE loblox_editor)
add_custom_target(loblox_ui_assets ALL COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/assets/ui ${CMAKE_CURRENT_BINARY_DIR}/share/loblox/ui)
add_dependencies(loblox loblox_ui_assets)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(loblox PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_executable(loblox_studio_tests tests/studio_main.cpp)
target_link_libraries(loblox_studio_tests PRIVATE loblox_core)
add_test(NAME loblox_studio COMMAND loblox_studio_tests)
add_executable(loblox_game_tests tests/game_main.cpp)
target_link_libraries(loblox_game_tests PRIVATE loblox_core)
add_test(NAME loblox_game COMMAND loblox_game_tests)
add_executable(loblox_tests tests/test_main.cpp)
target_link_libraries(loblox_tests PRIVATE loblox_core)
add_test(NAME loblox_tests COMMAND loblox_tests)
add_executable(loblox_spawn_tests tests/spawn_main.cpp)
target_link_libraries(loblox_spawn_tests PRIVATE loblox_core)
add_test(NAME loblox_spawn COMMAND loblox_spawn_tests)
add_test(NAME loblox_startup_no_display
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/startup_no_display.sh
          $<TARGET_FILE:loblox>)
add_test(NAME loblox_help COMMAND loblox --help)

# Regressao grafica dos gizmos: renderiza em RenderTexture e confere pixel e
# hit test. Sem contexto grafico o executavel retorna 77 (SKIP).
add_executable(loblox_gizmo_visual_tests tests/gizmo_visual_main.cpp)
target_link_libraries(loblox_gizmo_visual_tests PRIVATE loblox_editor)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(loblox_gizmo_visual_tests PRIVATE -Wall -Wextra)
endif()
find_program(XVFB_RUN_EXECUTABLE xvfb-run)
if(XVFB_RUN_EXECUTABLE)
  add_executable(loblox_decal_render_tests tests/decal_render_main.cpp)
  target_link_libraries(loblox_decal_render_tests PRIVATE loblox_editor)
  add_test(NAME loblox_decal_render COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 256x256x24" $<TARGET_FILE:loblox_decal_render_tests>)
  set_tests_properties(loblox_decal_render PROPERTIES TIMEOUT 45 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
  add_executable(loblox_rig_image_tests tests/rig_image_main.cpp)
  target_link_libraries(loblox_rig_image_tests PRIVATE loblox_editor)
  add_test(NAME loblox_rig_image COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 512x512x24" $<TARGET_FILE:loblox_rig_image_tests>)
  set_tests_properties(loblox_rig_image PROPERTIES TIMEOUT 60 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
  add_test(NAME loblox_gizmo_visual
    COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24 -noreset"
            $<TARGET_FILE:loblox_gizmo_visual_tests>)
  set_tests_properties(loblox_gizmo_visual PROPERTIES SKIP_RETURN_CODE 77)
  find_program(XDOTOOL_EXECUTABLE xdotool)
  find_package(X11 QUIET)
  if(XDOTOOL_EXECUTABLE AND X11_FOUND)
    add_executable(loblox_rml_ui_tests tests/rml_ui_main.cpp)
    target_link_libraries(loblox_rml_ui_tests PRIVATE loblox_editor)
    add_dependencies(loblox_rml_ui_tests loblox_ui_assets)
    add_test(NAME loblox_rml_ui COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset" $<TARGET_FILE:loblox_rml_ui_tests>)
    # The integration suite renders the interface at four resolutions on the
    # software rasteriser. Four-sample UI MSAA makes that pass slower, so keep
    # a timeout with headroom for slower CI machines.
    set_tests_properties(loblox_rml_ui PROPERTIES TIMEOUT 480 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_executable(loblox_rig_input_tests tests/rig_input_main.cpp)
    target_link_libraries(loblox_rig_input_tests PRIVATE loblox_editor)
    add_test(NAME loblox_rig_input COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1366x768x24 -noreset" $<TARGET_FILE:loblox_rig_input_tests>)
    set_tests_properties(loblox_rig_input PROPERTIES TIMEOUT 180 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_executable(loblox_visual_studio_tests tests/visual_studio_main.cpp)
    target_link_libraries(loblox_visual_studio_tests PRIVATE loblox_editor)
    add_test(NAME loblox_visual_studio COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset" $<TARGET_FILE:loblox_visual_studio_tests>)
    set_tests_properties(loblox_visual_studio PROPERTIES TIMEOUT 180 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")

    add_executable(loblox_sky_capture_tests tests/sky_capture_main.cpp)
    target_link_libraries(loblox_sky_capture_tests PRIVATE loblox_editor)
    add_test(NAME loblox_sky_capture COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset" $<TARGET_FILE:loblox_sky_capture_tests>)
    set_tests_properties(loblox_sky_capture PROPERTIES TIMEOUT 180 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")

    # Graphics quality fixture: logo, icons, text, transparency, gradient,
    # rounded corners, the gameplay HUD circles and health bar, decal, R6 face
    # and a repeating surface texture.
    add_executable(loblox_graphics_quality_tests tests/graphics_quality_main.cpp)
    target_link_libraries(loblox_graphics_quality_tests PRIVATE loblox_editor)
    add_test(NAME loblox_graphics_quality COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1366x768x24 -noreset" $<TARGET_FILE:loblox_graphics_quality_tests>)
    set_tests_properties(loblox_graphics_quality PROPERTIES TIMEOUT 240 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    # The same HUD checks at 1920x1080: the circular edge quality must be a
    # property of the interface framebuffer, not of one resolution.
    add_test(NAME loblox_graphics_quality_1080
      COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset"
              $<TARGET_FILE:loblox_graphics_quality_tests>
              ${CMAKE_CURRENT_BINARY_DIR}/graphics-quality-1080 1920x1080)
    set_tests_properties(loblox_graphics_quality_1080 PROPERTIES TIMEOUT 240 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")

    add_executable(loblox_studio_ui_tests tests/studio_ui_main.cpp tests/x11_focus.cpp)
    target_link_libraries(loblox_studio_ui_tests PRIVATE loblox_editor X11::X11)
    add_test(NAME loblox_studio_ui COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24 -noreset" $<TARGET_FILE:loblox_studio_ui_tests>)
    set_tests_properties(loblox_studio_ui PROPERTIES TIMEOUT 180 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_executable(loblox_game_ui_tests tests/game_ui_main.cpp tests/x11_focus.cpp)
    target_link_libraries(loblox_game_ui_tests PRIVATE loblox_editor X11::X11)
    add_test(NAME loblox_game_ui
      COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24 -noreset"
              $<TARGET_FILE:loblox_game_ui_tests>)
    set_tests_properties(loblox_game_ui PROPERTIES TIMEOUT 240 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_executable(loblox_editor_input_tests tests/editor_input_main.cpp tests/x11_focus.cpp)
    target_link_libraries(loblox_editor_input_tests PRIVATE loblox_editor X11::X11)
    add_test(NAME loblox_editor_input
      COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24 -noreset"
              $<TARGET_FILE:loblox_editor_input_tests>)
    set_tests_properties(loblox_editor_input PROPERTIES TIMEOUT 240
      ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
      add_test(NAME loblox_editor_restart
        COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24 -noreset"
                ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/editor_restart.py
                $<TARGET_FILE:loblox> $<TARGET_FILE:loblox_game_tests>)
      set_tests_properties(loblox_editor_restart PROPERTIES TIMEOUT 90)
    endif()
  endif()
endif()
