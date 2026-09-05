cmake_minimum_required(VERSION 3.16)

set(PACKAGE GameplayFootball)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 3)
set(VERSION_PATCH 1)
if(${VERSION_PATCH})
   set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
else(${VERSION_PATCH})
   set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR})
endif(${VERSION_PATCH})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules")

project(GameplayFootball)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX)
   set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wall")
   # Temporarily suppress warnings during debug
   set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-unused-variable -Wno-sign-compare -Wno-unused-function -Wno-strict-aliasing")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3")
endif(UNIX)

# Find required libraries. Only two differences are real across platforms: Windows
# builds use vcpkg (manifest mode), Linux/macOS use system packages (apt/brew).
# - Boost: system builds (Ubuntu libboost-all-dev, Homebrew) are done with b2, which
#   installs only the main BoostConfig.cmake and not the modular per-component
#   configs, so find_package(Boost COMPONENTS ...) fails against them and FindBoost's
#   manual search is forced; vcpkg's Boost is consumed in CONFIG mode. Boost.System
#   is header-only since 1.69, so it is not requested as a component.
# - SQLite: vcpkg ships its config under the unofficial name (there is no
#   SQLite3Config.cmake), so it is requested explicitly on Windows.
# Everything else resolves the same on every platform: SDL3/SDL3_image/SDL3_ttf have
# no Find modules, so find_package falls back to the CMake config files where they
# exist (vcpkg, system SDL3), and CMake's FindOpenAL/FindSQLite3 locate the system or
# vcpkg libraries through the toolchain prefix path.
find_package(OpenGL REQUIRED)
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(OpenAL REQUIRED)
if(WIN32)
  find_package(Boost REQUIRED COMPONENTS thread filesystem CONFIG)
  find_package(unofficial-sqlite3 CONFIG REQUIRED)
else()
  set(Boost_NO_BOOST_CMAKE ON)
  find_package(Boost REQUIRED COMPONENTS thread filesystem)
  find_package(SQLite3 REQUIRED)
endif()

if(WIN32)
  set(SQLite3_LINK_TARGET unofficial::sqlite3::sqlite3)
else()
  set(SQLite3_LINK_TARGET SQLite::SQLite3)
endif()
# CONFIG mode does not set the *_INCLUDE_DIR cache variables that the Find modules
# provide, but the game libraries compile against these includes directly (they do
# not link blunted2), so derive the missing values on Windows from the imported
# targets. OpenAL is found by the FindOpenAL module on every platform and defines
# OpenAL::OpenAL, so it needs no special-casing.
if(NOT Boost_INCLUDE_DIR)
  get_target_property(Boost_INCLUDE_DIR Boost::headers INTERFACE_INCLUDE_DIRECTORIES)
endif()
if(NOT SQLITE3_INCLUDE_DIRS)
  get_target_property(SQLITE3_INCLUDE_DIRS ${SQLite3_LINK_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
endif()

# Blunted2 engine core: a single static library built from the former
# object libraries (base, systems, loaders, types, framework, scene,
# managers, utils, gui2). Note: physics is intentionally NOT compiled,
# as it is not used by gameplayfootball.
add_library(blunted2 STATIC
        src/base/log.cpp
        src/base/sdl_surface.cpp
        src/base/utils.cpp
        src/base/properties.cpp
        src/base/exception.cpp
        src/base/geometry/triangle.cpp
        src/base/geometry/line.cpp
        src/base/geometry/trianglemeshutils.cpp
        src/base/geometry/aabb.cpp
        src/base/geometry/plane.cpp
        src/base/math/vector3.cpp
        src/base/math/matrix3.cpp
        src/base/math/bluntmath.cpp
        src/base/math/quaternion.cpp
        src/base/math/matrix4.cpp
        src/systems/isystemtask.cpp
        src/systems/graphics/graphics_object.cpp
        src/systems/graphics/graphics_task.cpp
        src/systems/graphics/objects/graphics_geometry.cpp
        src/systems/graphics/objects/graphics_camera.cpp
        src/systems/graphics/objects/graphics_light.cpp
        src/systems/graphics/objects/graphics_overlay2d.cpp
        src/systems/graphics/graphics_scene.cpp
        src/systems/graphics/resources/vertexbuffer.cpp
        src/systems/graphics/resources/texture.cpp
        src/systems/graphics/rendering/r3d_messages.cpp
        src/systems/graphics/rendering/opengl_renderer3d.cpp
        src/systems/graphics/graphics_system.cpp
        src/systems/audio/audio_system.cpp
        src/systems/audio/audio_scene.cpp
        src/systems/audio/objects/audio_audioreceiver.cpp
        src/systems/audio/objects/audio_sound.cpp
        src/systems/audio/resources/audio_soundbuffer.cpp
        src/systems/audio/rendering/openal_renderer.cpp
        src/systems/audio/rendering/audio_messages.cpp
        src/systems/audio/audio_object.cpp
        src/loaders/imageloader.cpp
        src/loaders/aseloader.cpp
        src/loaders/wavloader.cpp
        src/types/iusertask.cpp
        src/types/spatial.cpp
        src/types/refcounted.cpp
        src/types/trianglemesh.cpp
        src/types/observer.cpp
        src/types/command.cpp
        src/framework/workerthread.cpp
        src/framework/tasksequence.cpp
        src/framework/scheduler.cpp
        src/scene/objectfactory.cpp
        src/scene/scene2d/scene2d.cpp
        src/scene/scene.cpp
        src/scene/objects/image2d.cpp
        src/scene/objects/sound.cpp
        src/scene/objects/light.cpp
        src/scene/objects/audioreceiver.cpp
        src/scene/objects/geometry.cpp
        src/scene/objects/skybox.cpp
        src/scene/objects/joint.cpp
        src/scene/objects/camera.cpp
        src/scene/scene3d/scene3d.cpp
        src/scene/scene3d/node.cpp
        src/scene/object.cpp
        src/scene/resources/surface.cpp
        src/scene/resources/soundbuffer.cpp
        src/scene/resources/geometrydata.cpp
        src/managers/taskmanager.cpp
        src/managers/scenemanager.cpp
        src/managers/resourcemanagerpool.cpp
        src/managers/usereventmanager.cpp
        src/managers/environmentmanager.cpp
        src/managers/systemmanager.cpp
        src/utils/database.cpp
        src/utils/text2d.cpp
        src/utils/orbitcamera.cpp
        src/utils/animation.cpp
        src/utils/splitgeometry.cpp
        src/utils/objectloader.cpp
        src/utils/directoryparser.cpp
        src/utils/threadhud.cpp
        src/utils/xmlloader.cpp
        src/utils/animationextensions/footballanimationextension.cpp
        src/utils/console.cpp
        src/utils/capturestate.cpp
        src/utils/envstate.cpp
        src/utils/gui2/style.cpp
        src/utils/gui2/widgets/caption.cpp
        src/utils/gui2/widgets/menu.cpp
        src/utils/gui2/widgets/editline.cpp
        src/utils/gui2/widgets/button.cpp
        src/utils/gui2/widgets/image.cpp
        src/utils/gui2/widgets/grid.cpp
        src/utils/gui2/widgets/root.cpp
        src/utils/gui2/widgets/iconselector.cpp
        src/utils/gui2/widgets/frame.cpp
        src/utils/gui2/widgets/filebrowser.cpp
        src/utils/gui2/widgets/scrollbar.cpp
        src/utils/gui2/widgets/capturekey.cpp
        src/utils/gui2/widgets/text.cpp
        src/utils/gui2/widgets/slider.cpp
        src/utils/gui2/widgets/dialog.cpp
        src/utils/gui2/widgets/pulldown.cpp
        src/utils/gui2/events.cpp
        src/utils/gui2/view.cpp
        src/utils/gui2/windowmanager.cpp
        src/utils/gui2/guitask.cpp
        src/utils/gui2/page.cpp
        src/blunted.cpp)
target_include_directories(blunted2 PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${OPENGL_INCLUDE_DIR}
        ${Boost_INCLUDE_DIR}
        ${OPENAL_INCLUDE_DIR}
        ${SQLite3_INCLUDE_DIRS})
target_link_libraries(blunted2 PUBLIC
        Boost::filesystem Boost::thread ${SQLite3_LINK_TARGET}
        SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf
        OpenAL::OpenAL ${OPENGL_LIBRARIES})
if(UNIX)
   target_link_libraries(blunted2 PUBLIC dl m)
endif(UNIX)

# Game-side libraries. Each compiles headers that pull in engine/core
# includes, so give every target the same include path set.
set(ALL_INCLUDE_DIRS
        ${PROJECT_SOURCE_DIR}/src
        ${OPENGL_INCLUDE_DIR}
        ${Boost_INCLUDE_DIR}
        ${OPENAL_INCLUDE_DIR}
        ${SQLite3_INCLUDE_DIRS}
        ${SDL3_INCLUDE_DIRS})

function(apply_common_includes target)
  target_include_directories(${target} PRIVATE ${ALL_INCLUDE_DIRS})
endfunction()

add_library(leaguelib STATIC src/league/leaguecode.cpp)
apply_common_includes(leaguelib)
add_library(gamelib STATIC
        src/onthepitch/officials.cpp
        src/onthepitch/player/humanoid/humanoid_utils.cpp
        src/onthepitch/player/humanoid/animcollection.cpp
        src/onthepitch/player/humanoid/humanoidbase.cpp
        src/onthepitch/player/humanoid/humanoid.cpp
        src/onthepitch/player/playerofficial.cpp
        src/onthepitch/player/player.cpp
        src/onthepitch/player/playerbase.cpp
        src/onthepitch/player/controller/playercontroller.cpp
        src/onthepitch/player/controller/humancontroller.cpp
        src/onthepitch/player/controller/icontroller.cpp
        src/onthepitch/player/controller/refereecontroller.cpp
        src/onthepitch/player/controller/elizacontroller.cpp
        src/onthepitch/player/controller/strategies/special/celebration.cpp
        src/onthepitch/player/controller/strategies/strategy.cpp
        src/onthepitch/player/controller/strategies/offtheball/default_mid.cpp
        src/onthepitch/player/controller/strategies/offtheball/default_off.cpp
        src/onthepitch/player/controller/strategies/offtheball/default_def.cpp
        src/onthepitch/player/controller/strategies/offtheball/goalie_default.cpp
        src/onthepitch/humangamer.cpp
        src/onthepitch/ball.cpp
        src/onthepitch/match.cpp
        src/onthepitch/referee.cpp
        src/onthepitch/AIsupport/mentalimage.cpp
        src/onthepitch/AIsupport/AIfunctions.cpp
        src/onthepitch/proceduralpitch.cpp
        src/onthepitch/team.cpp
        src/onthepitch/teamAIcontroller.cpp)
apply_common_includes(gamelib)
add_library(hidlib STATIC src/hid/gamepad.cpp src/hid/keyboard.cpp)
apply_common_includes(hidlib)
add_library(menulib STATIC
        src/menu/credits.cpp
        src/menu/league/league_system.cpp
        src/menu/league/league_team.cpp
        src/menu/league/league_management.cpp
        src/menu/league/league.cpp
        src/menu/league/league_calendar.cpp
        src/menu/league/league_inbox.cpp
        src/menu/league/league_forward.cpp
        src/menu/league/league_standings.cpp
        src/menu/gameplan.cpp
        src/menu/widgets/planmap.cpp
        src/menu/widgets/gameplansubmenu.cpp
        src/menu/startmatch/teamselect.cpp
        src/menu/startmatch/loadingmatch.cpp
        src/menu/startmatch/matchoptions.cpp
        src/menu/visualoptions.cpp
        src/menu/pagefactory.cpp
        src/menu/menutask.cpp
        src/menu/cameramenu.cpp
        src/menu/menuscene.cpp
        src/menu/controllerselect.cpp
        src/menu/mainmenu.cpp
        src/menu/ingame/radar.cpp
        src/menu/ingame/tacticsdebug.cpp
        src/menu/ingame/gamepage.cpp
        src/menu/ingame/replaymenu.cpp
        src/menu/ingame/gameover.cpp
        src/menu/ingame/phasemenu.cpp
        src/menu/ingame/ingame.cpp
        src/menu/ingame/scoreboard.cpp
        src/menu/ingame/playerhud.cpp
        src/menu/settings.cpp)
apply_common_includes(menulib)
add_library(datalib STATIC
        src/data/matchdata.cpp
        src/data/playerdata.cpp
        src/data/teamdata.cpp)
apply_common_includes(datalib)
add_library(gamecontextlib STATIC src/gamecontext.cpp)
apply_common_includes(gamecontextlib)

set(LIBRARIES gamelib hidlib menulib datalib leaguelib gamecontextlib blunted2)

# Build the game as a GUI application (WIN32 subsystem) so no console window
# appears on Windows. Turn it off to keep a console for stdout/stderr debugging.
option(GAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM "Build gameplayfootball as a Windows GUI application (WIN32 subsystem)" ON)

# The game resolves all asset/database paths relative to the working directory,
# so copy data/ next to the binary automatically (no manual xcopy/cp needed).
# Player photos (databases/default/faces) are not used yet and are excluded.
function(copy_data_post_build target)
  add_custom_command(TARGET ${target} POST_BUILD
          COMMAND ${CMAKE_COMMAND}
                  "-DSRC=${PROJECT_SOURCE_DIR}/data"
                  "-DDST=$<TARGET_FILE_DIR:${target}>"
                  -P "${PROJECT_SOURCE_DIR}/tools/copy_data.cmake"
          COMMENT "Copying game data (excluding unused player photos)")
endfunction()

add_executable(gameplayfootball
        src/misc/perlin.cpp
        src/misc/hungarian.c
        src/gametask.cpp
        src/utils.cpp
        src/main.cpp
        src/dbquery.cpp
        src/gamedefines.cpp)
apply_common_includes(gameplayfootball)
if(WIN32 AND GAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM)
  set_target_properties(gameplayfootball PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
copy_data_post_build(gameplayfootball)
# GNU ld resolves static library references left-to-right; the game libraries
# have circular references (e.g. hidlib <-> gamecontextlib), so group them on
# Linux. MSVC/ld64 tolerate the flat order.
if(UNIX AND NOT APPLE)
  target_link_libraries(gameplayfootball PRIVATE -Wl,--start-group ${LIBRARIES} -Wl,--end-group)
else()
  target_link_libraries(gameplayfootball PRIVATE ${LIBRARIES})
endif()

# determinism_runner needs the core sources (utils, gamedefines, gametask, dbquery, misc)
# but NOT main.cpp (which would conflict with its own main()).
add_executable(determinism_runner
        tools/determinism/main.cpp
        src/misc/perlin.cpp
        src/misc/hungarian.c
        src/gametask.cpp
        src/utils.cpp
        src/dbquery.cpp
        src/gamedefines.cpp)
apply_common_includes(determinism_runner)
if(UNIX AND NOT APPLE)
  target_link_libraries(determinism_runner PRIVATE -Wl,--start-group ${LIBRARIES} -Wl,--end-group)
else()
  target_link_libraries(determinism_runner PRIVATE ${LIBRARIES})
endif()
copy_data_post_build(determinism_runner)
