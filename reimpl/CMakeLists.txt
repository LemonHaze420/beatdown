cmake_minimum_required(VERSION 3.8)
project(RWFightTribe LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REIMPL_INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,--stack,33554432")
endif()

set(GAME_RUNTIME_SOURCES
    game/game.cpp
    game/ai.cpp
    game/actor_state.cpp
    game/battle/battle.cpp
    game/battle/end.cpp
    game/battle/result.cpp
    game/battle/finisher.cpp
    game/battle/resources.cpp
    game/battle/resources_runtime.cpp
    game/battle/sequence.cpp
    game/battle/selection.cpp
    game/battle/flow.cpp
    game/battle/controller.cpp
    game/battle/presentation.cpp
    game/battle/dispatch.cpp
    game/battle/dialogue.cpp
    game/battle/position.cpp
    game/battle/subsequences.cpp
    game/battle/choice.cpp
    game/battle/tables.cpp
    game/battle/tables_query.cpp
    game/combat/combat.cpp
    game/combat/animation.cpp
    game/combat/damage.cpp
    game/combat/received_damage.cpp
    game/combat/surface.cpp
    game/combat/gauge.cpp
    game/combat/input.cpp
    game/combat/context.cpp
    game/combat/links.cpp
    game/combat/getup.cpp
    game/cpu_input.cpp
    game/combat/chain.cpp
    game/combat/weapons.cpp
    game/combat/weapon_runtime.cpp
    game/combat/owned_weapons.cpp
    stage/weapon_grants.cpp
    game/combat/projectiles.cpp
    game/combat/projectile_runtime.cpp
    game/combat/actions.cpp
    game/combat/items.cpp
    stage/item_assets.cpp
    stage/assets.cpp
    game/combat/motion.cpp
    game/combat/recovery.cpp
    game/combat/phases.cpp
    game/combat/state.cpp
    game/events.cpp
    game/frontend.cpp
    game/inventory.cpp
    game/movement.cpp
    game/controls.cpp
    game/locomotion.cpp
    game/combat/initial.cpp
    game/combat/guard.cpp
    game/options.cpp
    game/persistence.cpp
    game/rewards.cpp
    game/stage.cpp
    game/story.cpp)

if(WIN32)
    set(LIBRW_PLATFORM "D3D9" CACHE STRING "" FORCE)
else()
    set(LIBRW_PLATFORM "NULL" CACHE STRING "" FORCE)
endif()
set(LIBRW_TOOLS OFF CACHE BOOL "" FORCE)
set(LIBRW_INSTALL OFF CACHE BOOL "" FORCE)
set(LIBRW_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../libs" "${CMAKE_BINARY_DIR}/sdk-libs")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../formats"
                 "${CMAKE_BINARY_DIR}/formats")

add_library(EventScript STATIC ../formats/SCRIPT_BIN/script_bin.cpp)
target_include_directories(EventScript PUBLIC ../formats/SCRIPT_BIN)

if(WIN32)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../libs/librw/skeleton"
                     "${CMAKE_BINARY_DIR}/librw_skeleton")
    add_executable(RWFightTribe WIN32
        app/app.cpp
        app/assets.cpp
        app/audio.cpp
        app/automation.cpp
        app/camera.cpp
        app/effects.cpp
        app/input.cpp
        app/scene.cpp
        app/stage.cpp
        app/ui.cpp
        audio/audio.cpp
        character/character.cpp
        character/render.cpp
        effect.cpp
        event_vm.cpp
        ${GAME_RUNTIME_SOURCES}
        game/save.cpp
        game/data.cpp
        game/random.cpp
        camera/camera.cpp
        camera/camdat.cpp
        audio/spatial.cpp
        title_attract.cpp
        sprite_text.cpp
        ui/menu/font.cpp
        ui/menu/system.cpp
        localized_graph.cpp
        ui/menu/pause.cpp
        ui/menu/inventory.cpp
        scene/overlay.cpp
        scene/view.cpp
        scene/task.cpp
        scene/control.cpp
        scene/caption_view.cpp
        scene/mask.cpp
        frame_capture.cpp
        character/shadow.cpp
        movie.cpp
        rwworld.cpp
        scene/runtime.cpp
        scene/captions.cpp
        scene/props.cpp
        ui/hud/battle.cpp
        scene/sequence.cpp
        ui/hud/prompt.cpp
        ui/hud/advice.cpp
        ui/hud/advice_sequence.cpp
        ui/hud/radar.cpp
        credits.cpp
        scene/sequence_tables.cpp
        stage/stage.cpp
        stage/lighting.cpp
        stage/st4.cpp
        stage/ai.cpp
        stage/interactions.cpp
        story_state.cpp
        task.cpp)
    target_include_directories(RWFightTribe PRIVATE ${REIMPL_INCLUDE_DIRECTORIES})
    target_link_libraries(RWFightTribe PRIVATE EventScript librw::skeleton
                          librw::librw winmm mfplat mfuuid ole32 Formats)
else()
    add_library(RWFightTribeGame STATIC
        effect.cpp
        event_vm.cpp
        ${GAME_RUNTIME_SOURCES}
        game/save.cpp
        game/data.cpp
        game/random.cpp
        camera/camera.cpp
        camera/camdat.cpp
        audio/spatial.cpp
        title_attract.cpp
        sprite_text.cpp
        scene/runtime.cpp
        scene/sequence.cpp
        scene/sequence_tables.cpp
        scene/task.cpp
        scene/control.cpp
        stage/ai.cpp
        stage/interactions.cpp
        story_state.cpp
        task.cpp)
    target_include_directories(RWFightTribeGame PUBLIC ${REIMPL_INCLUDE_DIRECTORIES})
    target_link_libraries(RWFightTribeGame PUBLIC EventScript librw::librw Formats)
endif()
