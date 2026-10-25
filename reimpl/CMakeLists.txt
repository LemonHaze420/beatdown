cmake_minimum_required(VERSION 3.8)
project(RWFightTribe LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,--stack,33554432")
endif()

set(GAME_RUNTIME_SOURCES
    game.cpp
    game_ai.cpp
    game_actor_state.cpp
    game_battle.cpp
    game_battle_end.cpp
    game_battle_result.cpp
    game_battle_finisher.cpp
    game_battle_resources.cpp
    game_battle_resources_runtime.cpp
    game_battle_sequence.cpp
    game_battle_selection.cpp
    game_battle_flow.cpp
    game_battle_controller.cpp
    game_battle_presentation.cpp
    game_battle_dispatch.cpp
    game_battle_dialogue.cpp
    game_battle_position.cpp
    game_battle_subsequences.cpp
    game_battle_choice.cpp
    game_battle_tables.cpp
    game_battle_tables_query.cpp
    game_combat.cpp
    game_combat_animation.cpp
    game_combat_damage.cpp
    game_combat_received_damage.cpp
    game_combat_surface.cpp
    game_combat_gauge.cpp
    game_combat_input.cpp
    game_combat_context.cpp
    game_combat_links.cpp
    game_combat_getup.cpp
    game_cpu_input.cpp
    game_combat_chain.cpp
    game_combat_weapons.cpp
    game_combat_weapon_runtime.cpp
    game_combat_owned_weapons.cpp
    stage_weapon_grants.cpp
    game_combat_projectiles.cpp
    game_combat_projectile_runtime.cpp
    game_combat_actions.cpp
    game_combat_items.cpp
    item_world.cpp
    stage_assets.cpp
    game_combat_motion.cpp
    game_combat_recovery.cpp
    game_combat_phases.cpp
    game_combat_state.cpp
    game_events.cpp
    game_frontend.cpp
    game_inventory.cpp
    game_movement.cpp
    game_controls.cpp
    game_locomotion.cpp
    game_combat_initial.cpp
    game_combat_guard.cpp
    game_options.cpp
    game_persistence.cpp
    game_rewards.cpp
    game_stage.cpp
    game_story.cpp)

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
        app.cpp
        app_assets.cpp
        app_audio.cpp
        app_automation.cpp
        app_camera.cpp
        app_effects.cpp
        app_input.cpp
        app_scene.cpp
        app_stage.cpp
        app_ui.cpp
        audio.cpp
        character.cpp
        character_render.cpp
        effect.cpp
        event_vm.cpp
        ${GAME_RUNTIME_SOURCES}
        game_save.cpp
        game_data.cpp
        game_random.cpp
        camera.cpp
        camdat.cpp
        audio_spatial.cpp
        title_attract.cpp
        sprite_text.cpp
        menu_font.cpp
        menu_system.cpp
        localized_graph.cpp
        menu_pause.cpp
        menu_inventory.cpp
        scene_overlay.cpp
        scene_view.cpp
        scene_task.cpp
        scene_control.cpp
        scene_caption_view.cpp
        scene_mask.cpp
        frame_capture.cpp
        character_shadow.cpp
        movie.cpp
        rwworld.cpp
        scene_runtime.cpp
        scene_captions.cpp
        scene_props.cpp
        battle_hud.cpp
        scene_sequence.cpp
        hud_prompt.cpp
        hud_advice.cpp
        hud_advice_sequence.cpp
        hud_radar.cpp
        credits.cpp
        scene_sequences.cpp
        stage.cpp
        stage_lighting.cpp
        st4.cpp
        stage_ai.cpp
        stage_interactions.cpp
        story_state.cpp
        task.cpp)
    target_link_libraries(RWFightTribe PRIVATE EventScript librw::skeleton
                          librw::librw winmm mfplat mfuuid ole32 Formats)
else()
    add_library(RWFightTribeGame STATIC
        effect.cpp
        event_vm.cpp
        ${GAME_RUNTIME_SOURCES}
        game_save.cpp
        game_data.cpp
        game_random.cpp
        camera.cpp
        camdat.cpp
        audio_spatial.cpp
        title_attract.cpp
        sprite_text.cpp
        scene_runtime.cpp
        scene_sequence.cpp
        scene_sequences.cpp
        scene_task.cpp
        scene_control.cpp
        stage_ai.cpp
        stage_interactions.cpp
        story_state.cpp
        task.cpp)
    target_link_libraries(RWFightTribeGame PRIVATE Formats)
    target_link_libraries(RWFightTribeGame PUBLIC EventScript librw::librw)
endif()
