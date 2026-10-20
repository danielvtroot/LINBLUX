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
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

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
target_link_libraries(loblox_core PUBLIC ${BULLET_LIBRARIES} ${RAYLIB_LIBRARIES} Threads::Threads)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(loblox_core PRIVATE -Wall -Wextra)
endif()

add_executable(loblox
  src/main.cpp
  src/app/Application.cpp
  src/rendering/SceneRenderer.cpp
  src/editor/EditorUI.cpp
)
target_link_libraries(loblox PRIVATE loblox_core loblox_imgui ${RAYLIB_LIBRARIES} OpenGL::GL)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(loblox PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_executable(loblox_tests tests/test_main.cpp)
target_link_libraries(loblox_tests PRIVATE loblox_core)
add_test(NAME loblox_tests COMMAND loblox_tests)
add_test(NAME loblox_startup_no_display
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/startup_no_display.sh
          $<TARGET_FILE:loblox>)
add_test(NAME loblox_help COMMAND loblox --help)

# Regressao grafica dos gizmos: renderiza em RenderTexture e confere pixel e
# hit test. Sem contexto grafico o executavel retorna 77 (SKIP).
add_executable(loblox_gizmo_visual_tests
  tests/gizmo_visual_main.cpp
  src/rendering/SceneRenderer.cpp
)
target_link_libraries(loblox_gizmo_visual_tests PRIVATE loblox_core)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(loblox_gizmo_visual_tests PRIVATE -Wall -Wextra)
endif()
find_program(XVFB_RUN_EXECUTABLE xvfb-run)
if(XVFB_RUN_EXECUTABLE)
  add_test(NAME loblox_gizmo_visual
    COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24"
            $<TARGET_FILE:loblox_gizmo_visual_tests>)
  set_tests_properties(loblox_gizmo_visual PROPERTIES SKIP_RETURN_CODE 77)
  find_program(XDOTOOL_EXECUTABLE xdotool)
  find_package(X11 QUIET)
  if(XDOTOOL_EXECUTABLE AND X11_FOUND)
    add_executable(loblox_editor_input_tests tests/editor_input_main.cpp
      tests/x11_focus.cpp src/editor/EditorUI.cpp src/rendering/SceneRenderer.cpp)
    target_link_libraries(loblox_editor_input_tests PRIVATE loblox_core loblox_imgui OpenGL::GL X11::X11)
    add_test(NAME loblox_editor_input
      COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24"
              $<TARGET_FILE:loblox_editor_input_tests>)
    set_tests_properties(loblox_editor_input PROPERTIES TIMEOUT 240
      ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
      add_test(NAME loblox_editor_restart
        COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x800x24"
                ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/editor_restart.py
                $<TARGET_FILE:loblox>)
      set_tests_properties(loblox_editor_restart PROPERTIES TIMEOUT 90)
    endif()
  endif()
endif()
