cmake_minimum_required(VERSION 3.16)
project(LINBLUX VERSION 0.3.0 LANGUAGES CXX)

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
find_package(ZLIB REQUIRED)

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
add_library(linblux_rml_gl3 STATIC ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp)
target_include_directories(linblux_rml_gl3 PUBLIC ${rmlui_SOURCE_DIR}/Backends)
target_link_libraries(linblux_rml_gl3 PUBLIC RmlUi::Core OpenGL::GL ${CMAKE_DL_LIBS})
# The RmlUi GL3 backend draws the interface into its own layer framebuffer and
# resolves it to the postprocess target before touching raylib's backbuffer.
# The circular gameplay buttons and the health bar are border-radius geometry,
# so window MSAA flags cannot smooth them; the layer target itself needs MSAA.
# Four samples is the smallest count OpenGL 3.3 guarantees (MAX_SAMPLES >= 4),
# so a conforming context always accepts it, and the cost is limited to the
# interface layer (never the 3D scene). The layer target is genuinely larger:
# around 80 MiB at 1920x1080 (multisampled colour and depth/stencil plus the
# two resolve targets) and around 42 MiB at 1366x768, which still fits the 3 GB
# target machine. Lighter alternative: -DLINBLUX_RMLUI_MSAA_SAMPLES=2, at the
# price of visibly fewer coverage levels; 0 is the build-time fallback for a
# driver that cannot create a multisample target, and the fixtures then report
# the hard-edged result.
set(LINBLUX_RMLUI_MSAA_SAMPLES "4" CACHE STRING
    "MSAA samples for the RmlUi layer framebuffer (0 disables MSAA)")
target_compile_definitions(linblux_rml_gl3 PRIVATE
    RMLUI_NUM_MSAA_SAMPLES=${LINBLUX_RMLUI_MSAA_SAMPLES})

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
add_library(linblux_imgui STATIC
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp
  third_party/imgui/imgui_stdlib.cpp
  third_party/rlimgui/rlImGui.cpp
)
target_include_directories(linblux_imgui PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rlimgui
  ${RAYLIB_INCLUDE_DIRS}
)
target_compile_definitions(linblux_imgui PUBLIC NO_FONT_AWESOME)
target_link_libraries(linblux_imgui PUBLIC ${RAYLIB_LIBRARIES} OpenGL::GL)

# Camada sem interface: modelo de objetos, cena, fisica e persistencia.
add_library(linblux_engine STATIC
  src/experiences/ExperienceManager.cpp
  src/scripting/LuaRuntime.cpp
  src/game/Rig.cpp
  src/game/GameAssetsIO.cpp
  src/game/ImageAssets.cpp
  src/game/JpegImage.cpp
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
)
target_include_directories(linblux_engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${BULLET_INCLUDE_DIR}
  ${RAYLIB_INCLUDE_DIRS}
)
target_include_directories(linblux_engine SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)
target_link_libraries(linblux_engine PUBLIC ${BULLET_LIBRARIES} ${RAYLIB_LIBRARIES} OpenGL::GL Threads::Threads PkgConfig::LUA)
target_link_libraries(linblux_engine PUBLIC ZLIB::ZLIB)
target_compile_definitions(linblux_engine PRIVATE LINBLUX_FACTORY_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/templates")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(linblux_engine PRIVATE -Wall -Wextra)
endif()

# Editor support has no ownership over runtime scene/physics/Lua.
add_library(linblux_editing STATIC
  src/editor/EditHistory.cpp
  src/editor/GizmoMath.cpp
  src/editor/GizmoLayout.cpp
  src/editor/Gizmo.cpp
  src/editor/TransformMetrics.cpp
  src/editor/TransformSession.cpp
  src/rendering/EditorCamera.cpp
  src/editor/SceneRendererAdapter.cpp
)
target_link_libraries(linblux_editing PUBLIC linblux_engine)
add_library(linblux_core INTERFACE) # transitional test/embedding facade
 target_link_libraries(linblux_core INTERFACE linblux_engine linblux_editing)
add_library(linblux_renderer STATIC src/rendering/SceneRenderer.cpp)
target_link_libraries(linblux_renderer PUBLIC linblux_engine)
add_library(linblux_presentation STATIC src/ui/Localization.cpp)
target_link_libraries(linblux_presentation PUBLIC linblux_engine)

option(LINBLUX_ENABLE_VIDEO "Video de fundo com FFmpeg" ON)
if(LINBLUX_ENABLE_VIDEO)
  pkg_check_modules(FFMPEG IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
endif()
add_library(linblux_media STATIC src/media/VideoBackground.cpp)
target_include_directories(linblux_media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${RAYLIB_INCLUDE_DIRS})
target_link_libraries(linblux_media PUBLIC ${RAYLIB_LIBRARIES} Threads::Threads)
if(FFMPEG_FOUND AND LINBLUX_ENABLE_VIDEO)
  target_compile_definitions(linblux_media PRIVATE LINBLUX_VIDEO_FFMPEG)
  target_link_libraries(linblux_media PRIVATE PkgConfig::FFMPEG)
else()
  message(WARNING "FFmpeg indisponivel: tela inicial usara fundo estatico")
endif()

add_library(linblux_studio STATIC
  src/ui/ExperienceUI.cpp
  src/ui/NativeFileDialog.cpp
  src/ui/StudioFrontend.cpp
  src/ui/EditorActions.cpp
  src/ui/RmlFrontendSupport.cpp
  src/editor/EditorUI.cpp
  src/editor/GamePanels.cpp
  src/editor/StudioPanels.cpp
)
target_link_libraries(linblux_studio PUBLIC linblux_core linblux_renderer linblux_presentation linblux_media linblux_imgui linblux_rml_gl3 OpenGL::GL)
target_compile_definitions(linblux_studio PRIVATE LINBLUX_UI_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/ui")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(linblux_studio PRIVATE -Wall -Wextra)
endif()
add_library(linblux_editor ALIAS linblux_studio)

add_executable(linblux_player src/player/main.cpp)
target_link_libraries(linblux_player PRIVATE linblux_engine linblux_renderer linblux_presentation)
install(TARGETS linblux_player RUNTIME DESTINATION bin COMPONENT Linblux)

add_executable(linblux src/main.cpp src/app/Application.cpp)
target_link_libraries(linblux PRIVATE linblux_editor)
add_custom_target(linblux_ui_assets ALL COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/assets/ui ${CMAKE_CURRENT_BINARY_DIR}/share/linblux/ui)
add_dependencies(linblux linblux_ui_assets)
add_custom_target(linblux_template_assets ALL COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/assets/templates ${CMAKE_CURRENT_BINARY_DIR}/share/linblux/templates)
add_dependencies(linblux linblux_template_assets)
add_custom_target(linblux_default_assets ALL COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/assets/defaults ${CMAKE_CURRENT_BINARY_DIR}/share/linblux/defaults)
add_dependencies(linblux linblux_default_assets)
install(DIRECTORY assets/defaults/ DESTINATION share/linblux/defaults COMPONENT Linblux)
add_custom_target(linblux_builtin_assets ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/share/linblux/builtin
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/midia/logo.png ${CMAKE_CURRENT_SOURCE_DIR}/assets/builtin/character-face.png ${CMAKE_CURRENT_BINARY_DIR}/share/linblux/builtin)
add_dependencies(linblux linblux_builtin_assets)
add_custom_target(linblux_media_assets ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/share/linblux/midia
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/midia/logo.png ${CMAKE_CURRENT_SOURCE_DIR}/midia/careditfullhd.mp4 ${CMAKE_CURRENT_BINARY_DIR}/share/linblux/midia)
add_dependencies(linblux linblux_media_assets)
install(FILES midia/logo.png midia/careditfullhd.mp4 DESTINATION share/linblux/midia COMPONENT Linblux)
install(TARGETS linblux RUNTIME DESTINATION bin COMPONENT Linblux)
install(DIRECTORY assets/ui/ DESTINATION share/linblux/ui COMPONENT Linblux)
install(DIRECTORY assets/templates/ DESTINATION share/linblux/templates COMPONENT Linblux)
install(FILES midia/logo.png assets/builtin/character-face.png DESTINATION share/linblux/builtin COMPONENT Linblux)
install(DIRECTORY docs/ DESTINATION share/linblux/docs COMPONENT Linblux FILES_MATCHING PATTERN "*.md")
install(FILES third_party/stb/LICENSE DESTINATION share/linblux/licenses COMPONENT Linblux RENAME stb_image.txt)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(linblux PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME linblux_player_help COMMAND linblux_player --help)
add_test(NAME linblux_player_validate COMMAND linblux_player --validate ${CMAKE_CURRENT_SOURCE_DIR}/assets/defaults/factory/world.loblox.json)
add_executable(linblux_experience_tests tests/experiences_main.cpp)
target_link_libraries(linblux_experience_tests PRIVATE linblux_core)
add_test(NAME linblux_experiences COMMAND linblux_experience_tests)
add_executable(linblux_studio_tests tests/studio_main.cpp)
target_link_libraries(linblux_studio_tests PRIVATE linblux_core)
add_test(NAME linblux_studio COMMAND linblux_studio_tests)
add_executable(linblux_game_tests tests/game_main.cpp)
target_link_libraries(linblux_game_tests PRIVATE linblux_core)
add_test(NAME linblux_game COMMAND linblux_game_tests)
add_executable(linblux_tests tests/test_main.cpp)
target_link_libraries(linblux_tests PRIVATE linblux_core)
add_test(NAME linblux_tests COMMAND linblux_tests)
add_executable(linblux_manipulation_tests tests/manipulation_main.cpp)
target_link_libraries(linblux_manipulation_tests PRIVATE linblux_core)
add_test(NAME linblux_manipulation COMMAND linblux_manipulation_tests)
add_executable(linblux_spawn_tests tests/spawn_main.cpp)
target_link_libraries(linblux_spawn_tests PRIVATE linblux_core)
add_test(NAME linblux_spawn COMMAND linblux_spawn_tests)
add_test(NAME linblux_startup_no_display
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/startup_no_display.sh
          $<TARGET_FILE:linblux>)
add_test(NAME linblux_help COMMAND linblux --help)

# Regressao grafica dos gizmos: renderiza em RenderTexture e confere pixel e
# hit test. Sem contexto grafico o executavel retorna 77 (SKIP).
add_executable(linblux_gizmo_visual_tests tests/gizmo_visual_main.cpp)
target_link_libraries(linblux_gizmo_visual_tests PRIVATE linblux_editor)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(linblux_gizmo_visual_tests PRIVATE -Wall -Wextra)
endif()
find_program(XVFB_RUN_EXECUTABLE xvfb-run)
if(XVFB_RUN_EXECUTABLE)
  add_executable(linblux_decal_render_tests tests/decal_render_main.cpp)
  target_link_libraries(linblux_decal_render_tests PRIVATE linblux_editor)
  add_test(NAME linblux_decal_render COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 256x256x24" $<TARGET_FILE:linblux_decal_render_tests>)
  set_tests_properties(linblux_decal_render PROPERTIES TIMEOUT 45 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
  add_executable(linblux_rig_image_tests tests/rig_image_main.cpp)
  target_link_libraries(linblux_rig_image_tests PRIVATE linblux_editor)
  add_test(NAME linblux_rig_image COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 512x512x24" $<TARGET_FILE:linblux_rig_image_tests>)
  set_tests_properties(linblux_rig_image PROPERTIES TIMEOUT 60 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
  add_test(NAME linblux_gizmo_visual
    COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24 -noreset"
            $<TARGET_FILE:linblux_gizmo_visual_tests>)
  set_tests_properties(linblux_gizmo_visual PROPERTIES SKIP_RETURN_CODE 77)
  find_program(XDOTOOL_EXECUTABLE xdotool)
  find_package(X11 QUIET)
  if(XDOTOOL_EXECUTABLE AND X11_FOUND)
    add_executable(linblux_experience_ui_tests tests/experience_ui_main.cpp)
    target_link_libraries(linblux_experience_ui_tests PRIVATE linblux_editor)
    add_dependencies(linblux_experience_ui_tests linblux_ui_assets linblux_template_assets)
    add_test(NAME linblux_experience_ui COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset" $<TARGET_FILE:linblux_experience_ui_tests>)
    set_tests_properties(linblux_experience_ui PROPERTIES TIMEOUT 300 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_executable(linblux_rml_ui_tests tests/rml_ui_main.cpp tests/x11_focus.cpp)
    target_link_libraries(linblux_rml_ui_tests PRIVATE linblux_editor X11::X11)
    add_dependencies(linblux_rml_ui_tests linblux_ui_assets)
    add_test(NAME linblux_rml_ui COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset" $<TARGET_FILE:linblux_rml_ui_tests>)
    # The integration suite renders the interface at four resolutions on the
    # software rasteriser. Four-sample UI MSAA makes that pass slower, so keep
    # a timeout with headroom for slower CI machines.
    set_tests_properties(linblux_rml_ui PROPERTIES TIMEOUT 480 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_test(NAME linblux_rig_decals_ui COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset" $<TARGET_FILE:linblux_rml_ui_tests>)
    set_tests_properties(linblux_rig_decals_ui PROPERTIES TIMEOUT 120 ENVIRONMENT "LOBLOX_RIG_DECALS_ONLY=1;LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_test(NAME linblux_mouse_lock COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset" $<TARGET_FILE:linblux_rml_ui_tests>)
    set_tests_properties(linblux_mouse_lock PROPERTIES TIMEOUT 180 ENVIRONMENT "LOBLOX_MOUSE_LOCK_ONLY=1;LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_test(NAME linblux_manipulation_ui COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset" $<TARGET_FILE:linblux_rml_ui_tests>)
    set_tests_properties(linblux_manipulation_ui PROPERTIES TIMEOUT 240 ENVIRONMENT "LOBLOX_MANIPULATION_ONLY=1;LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_executable(linblux_rig_input_tests tests/rig_input_main.cpp)
    target_link_libraries(linblux_rig_input_tests PRIVATE linblux_editor)
    add_test(NAME linblux_rig_input COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1366x768x24 -noreset" $<TARGET_FILE:linblux_rig_input_tests>)
    set_tests_properties(linblux_rig_input PROPERTIES TIMEOUT 180 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_executable(linblux_visual_studio_tests tests/visual_studio_main.cpp)
    target_link_libraries(linblux_visual_studio_tests PRIVATE linblux_editor)
    add_test(NAME linblux_visual_studio COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset" $<TARGET_FILE:linblux_visual_studio_tests>)
    set_tests_properties(linblux_visual_studio PROPERTIES TIMEOUT 180 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")

    add_executable(linblux_sky_capture_tests tests/sky_capture_main.cpp)
    target_link_libraries(linblux_sky_capture_tests PRIVATE linblux_editor)
    add_test(NAME linblux_sky_capture COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset" $<TARGET_FILE:linblux_sky_capture_tests>)
    set_tests_properties(linblux_sky_capture PROPERTIES TIMEOUT 180 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")

    # Graphics quality fixture: logo, icons, text, transparency, gradient,
    # rounded corners, the gameplay HUD circles and health bar, decal, R6 face
    # and a repeating surface texture.
    add_executable(linblux_graphics_quality_tests tests/graphics_quality_main.cpp)
    target_link_libraries(linblux_graphics_quality_tests PRIVATE linblux_editor)
    add_test(NAME linblux_graphics_quality COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1366x768x24 -noreset" $<TARGET_FILE:linblux_graphics_quality_tests>)
    set_tests_properties(linblux_graphics_quality PROPERTIES TIMEOUT 240 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    # The same HUD checks at 1920x1080: the circular edge quality must be a
    # property of the interface framebuffer, not of one resolution.
    add_test(NAME linblux_graphics_quality_1080
      COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset"
              $<TARGET_FILE:linblux_graphics_quality_tests>
              ${CMAKE_CURRENT_BINARY_DIR}/graphics-quality-1080 1920x1080)
    set_tests_properties(linblux_graphics_quality_1080 PROPERTIES TIMEOUT 240 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")

    add_executable(linblux_studio_ui_tests tests/studio_ui_main.cpp tests/x11_focus.cpp)
    target_link_libraries(linblux_studio_ui_tests PRIVATE linblux_editor X11::X11)
    add_test(NAME linblux_studio_ui COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24 -noreset" $<TARGET_FILE:linblux_studio_ui_tests>)
    set_tests_properties(linblux_studio_ui PROPERTIES TIMEOUT 180 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_executable(linblux_game_ui_tests tests/game_ui_main.cpp tests/x11_focus.cpp)
    target_link_libraries(linblux_game_ui_tests PRIVATE linblux_editor X11::X11)
    add_test(NAME linblux_game_ui
      COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24 -noreset"
              $<TARGET_FILE:linblux_game_ui_tests>)
    set_tests_properties(linblux_game_ui PROPERTIES TIMEOUT 240 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    add_executable(linblux_editor_input_tests tests/editor_input_main.cpp tests/x11_focus.cpp)
    target_link_libraries(linblux_editor_input_tests PRIVATE linblux_editor X11::X11)
    add_test(NAME linblux_editor_input
      COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24 -noreset"
              $<TARGET_FILE:linblux_editor_input_tests>)
    set_tests_properties(linblux_editor_input PROPERTIES TIMEOUT 240
      ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
      add_test(NAME linblux_experience_startup
        COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24 -noreset"
                ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/experience_startup.py
                $<TARGET_FILE:linblux> $<TARGET_FILE:linblux_experience_tests>)
      set_tests_properties(linblux_experience_startup PROPERTIES TIMEOUT 90 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
      add_test(NAME linblux_editor_restart
        COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24 -noreset"
                ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/editor_restart.py
                $<TARGET_FILE:linblux> $<TARGET_FILE:linblux_game_tests>)
      set_tests_properties(linblux_editor_restart PROPERTIES TIMEOUT 90)
    endif()
  endif()
endif()

if(FFMPEG_FOUND AND LINBLUX_ENABLE_VIDEO)
  add_executable(linblux_video_tests tests/video_main.cpp)
  target_link_libraries(linblux_video_tests PRIVATE linblux_media)
  add_test(NAME linblux_video COMMAND linblux_video_tests ${CMAKE_CURRENT_SOURCE_DIR}/midia/careditfullhd.mp4)
  set_tests_properties(linblux_video PROPERTIES TIMEOUT 40)
endif()
if(XVFB_RUN_EXECUTABLE AND FFMPEG_FOUND AND LINBLUX_ENABLE_VIDEO)
  add_executable(linblux_home_video_tests tests/home_video_main.cpp)
  target_link_libraries(linblux_home_video_tests PRIVATE linblux_editor)
  add_dependencies(linblux_home_video_tests linblux_ui_assets linblux_template_assets linblux_media_assets)
  add_test(NAME linblux_home_video COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24" $<TARGET_FILE:linblux_home_video_tests>)
  set_tests_properties(linblux_home_video PROPERTIES TIMEOUT 120 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
endif()
add_executable(linblux_defaults_tests tests/defaults_main.cpp)
target_link_libraries(linblux_defaults_tests PRIVATE linblux_core)
add_test(NAME linblux_defaults COMMAND linblux_defaults_tests)
add_executable(linblux_engine_default_tests tests/engine_default_main.cpp)
target_link_libraries(linblux_engine_default_tests PRIVATE linblux_engine)
add_test(NAME linblux_engine_default_lifecycle COMMAND linblux_engine_default_tests)

# Preserve notices for every static third-party component shipped in the application.
install(FILES LICENSE DESTINATION share/linblux/licenses COMPONENT Linblux RENAME LINBLUX-MIT.txt)
install(FILES third_party/imgui/LICENSE.txt DESTINATION share/linblux/licenses COMPONENT Linblux RENAME Dear-ImGui-MIT.txt)
install(FILES third_party/rlimgui/LICENSE DESTINATION share/linblux/licenses COMPONENT Linblux RENAME rlImGui-zlib.txt)
install(FILES third_party/nlohmann/LICENSE.MIT DESTINATION share/linblux/licenses COMPONENT Linblux RENAME nlohmann-json-MIT.txt)
install(FILES ${rmlui_SOURCE_DIR}/LICENSE.txt DESTINATION share/linblux/licenses COMPONENT Linblux RENAME RmlUi-MIT.txt)
set_tests_properties(linblux_player_validate PROPERTIES ENVIRONMENT
  "XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/player;XDG_STATE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-state/player")
if(Python3_Interpreter_FOUND AND XVFB_RUN_EXECUTABLE AND XDOTOOL_EXECUTABLE)
  execute_process(COMMAND ${Python3_EXECUTABLE} -c "import PIL" RESULT_VARIABLE LINBLUX_PIL_RESULT OUTPUT_QUIET ERROR_QUIET)
  if(LINBLUX_PIL_RESULT EQUAL 0)
    add_test(NAME linblux_engine_default_restart COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1920x1080x24"
      ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/engine_default_restart.py
      $<TARGET_FILE:linblux> $<TARGET_FILE:linblux_engine_default_tests>)
    set_tests_properties(linblux_engine_default_restart PROPERTIES TIMEOUT 150 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
  else()
    message(WARNING "Teste de reinicializacao do Mundo Padrao requer python3-pil")
  endif()
endif()
install(FILES packaging/linblux.desktop packaging/linblux-player.desktop DESTINATION share/applications COMPONENT Linblux)
install(FILES packaging/linblux-mime.xml DESTINATION share/mime/packages COMPONENT Linblux)
install(FILES midia/logo.png DESTINATION share/pixmaps COMPONENT Linblux RENAME linblux.png)

# ATLAS: testes locais da inteligência estrutural (Python stdlib, sem rede).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME linblux_atlas
    COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_CURRENT_SOURCE_DIR}/tests/atlas -p "test_*.py" -v)
  set_tests_properties(linblux_atlas PROPERTIES
    TIMEOUT 180
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# TURBO: labels, timeouts and display resource locks.  The classification is
# based on the real command/environment and measured CTestCostData durations.
# Keep this block at the end so conditional tests only receive properties when
# they are actually registered in the current build.
function(linblux_turbo_headless test labels)
  if(TEST ${test})
    set_tests_properties(${test} PROPERTIES LABELS "${labels}")
    get_test_property(${test} TIMEOUT _linblux_timeout)
    if(_linblux_timeout STREQUAL "NOTFOUND")
      set_tests_properties(${test} PROPERTIES TIMEOUT 300)
    endif()
  endif()
endfunction()

function(linblux_turbo_graphics test labels)
  if(TEST ${test})
    set_tests_properties(${test} PROPERTIES
      LABELS "${labels}"
      RESOURCE_LOCK display
      PROCESSORS 2)
    get_test_property(${test} TIMEOUT _linblux_timeout)
    if(_linblux_timeout STREQUAL "NOTFOUND")
      set_tests_properties(${test} PROPERTIES TIMEOUT 300)
    endif()
  endif()
endfunction()

linblux_turbo_headless(linblux_player_help "smoke;headless;player")
linblux_turbo_headless(linblux_player_validate "smoke;headless;player;serialization")
linblux_turbo_headless(linblux_experiences "unit;headless;experiences;serialization")
linblux_turbo_headless(linblux_studio "unit;headless;editor;game")
linblux_turbo_headless(linblux_game "unit;headless;game;scene;physics")
linblux_turbo_headless(linblux_tests "unit;headless;core;scene;physics;scripting;serialization")
linblux_turbo_headless(linblux_manipulation "unit;headless;editor;scene;physics")
linblux_turbo_headless(linblux_spawn "unit;headless;game;scene;objects")
linblux_turbo_headless(linblux_startup_no_display "smoke;headless;app;core")
linblux_turbo_headless(linblux_help "smoke;headless;app")
linblux_turbo_headless(linblux_defaults "unit;headless;core;experiences;serialization")
linblux_turbo_headless(linblux_engine_default_lifecycle "unit;headless;core;experiences;serialization")
linblux_turbo_headless(linblux_atlas "unit;headless;tooling;atlas")

linblux_turbo_graphics(linblux_decal_render "graphics;integration;rendering;game")
linblux_turbo_graphics(linblux_rig_image "graphics;integration;editor;rendering;rig")
linblux_turbo_graphics(linblux_gizmo_visual "graphics;integration;editor;scene;rendering")
linblux_turbo_graphics(linblux_experience_ui "graphics;integration;experiences;ui;app;slow")
linblux_turbo_graphics(linblux_rml_ui "graphics;integration;ui;editor;slow")
linblux_turbo_graphics(linblux_rig_decals_ui "graphics;integration;editor;rendering;rig;slow")
linblux_turbo_graphics(linblux_mouse_lock "graphics;integration;editor;ui;game;slow")
linblux_turbo_graphics(linblux_manipulation_ui "graphics;integration;editor;scene;ui;slow")
linblux_turbo_graphics(linblux_rig_input "graphics;integration;editor;rig;ui;slow")
linblux_turbo_graphics(linblux_visual_studio "graphics;integration;editor;rendering;ui;slow")
linblux_turbo_graphics(linblux_sky_capture "graphics;integration;rendering;scene")
linblux_turbo_graphics(linblux_graphics_quality "graphics;integration;rendering;ui;game;slow")
linblux_turbo_graphics(linblux_graphics_quality_1080 "graphics;integration;rendering;ui;game;slow")
linblux_turbo_graphics(linblux_studio_ui "graphics;integration;editor;ui;slow")
linblux_turbo_graphics(linblux_game_ui "graphics;integration;game;ui;slow")
linblux_turbo_graphics(linblux_editor_input "graphics;integration;editor;ui;slow")
linblux_turbo_graphics(linblux_experience_startup "graphics;integration;experiences;app;ui;slow")
linblux_turbo_graphics(linblux_editor_restart "graphics;integration;editor;app;serialization;slow")
linblux_turbo_graphics(linblux_home_video "graphics;integration;media;ui;app;slow")
linblux_turbo_graphics(linblux_engine_default_restart "graphics;integration;core;experiences;serialization;app;slow")
linblux_turbo_headless(linblux_video "integration;video;headless;media;slow")

# TURBO's own tests: stdlib only and offline.  They do not replace the engine
# suite; they validate selection, cache invalidation and process supervision.
if(Python3_Interpreter_FOUND)
  add_test(NAME linblux_turbo
    COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_CURRENT_SOURCE_DIR}/tests/turbo -p "test_*.py" -v)
  set_tests_properties(linblux_turbo PROPERTIES
    TIMEOUT 180
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
linblux_turbo_headless(linblux_turbo "unit;headless;tooling;turbo")
