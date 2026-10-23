if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/librw/CMakeLists.txt")
    message(FATAL_ERROR "Initialize the SDK dependency with git submodule update --init --recursive")
endif()

add_subdirectory(librw "${CMAKE_BINARY_DIR}/librw")
get_target_property(librw_sources librw SOURCES)
set(librw_override_dir "${CMAKE_CURRENT_BINARY_DIR}/librw-overrides")
file(MAKE_DIRECTORY "${librw_override_dir}")

function(librw_override filename original replacement)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/librw/src/${filename}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    file(READ "${source}" content)
    string(FIND "${content}" "${replacement}" fixed_position)
    if(fixed_position EQUAL -1)
        string(FIND "${content}" "${original}" original_position)
        if(original_position EQUAL -1)
            message(FATAL_ERROR "Unexpected librw source in ${filename}")
        endif()
        string(REPLACE "${original}" "${replacement}" content "${content}")
    endif()
    set(override "${librw_override_dir}/${filename}")
    file(WRITE "${override}" "${content}")
    list(REMOVE_ITEM librw_sources "${filename}")
    list(APPEND librw_sources "${override}")
    set(librw_sources "${librw_sources}" PARENT_SCOPE)
endfunction()

librw_override(hanim.cpp
    [=[		anim->applyCB(&animMat, anim->getInterpFrame(i));]=]
    [=[		anim->applyCB(&animMat, anim->getInterpFrame(i));
		if((this->flags & UPDATEMODELLINGMATRICES) && node->frame){
			node->frame->matrix = animMat;
			node->frame->updateObjects();
		}]=])

librw_override(userdata.cpp
    [=[	dsta = srcext->arrays;]=]
    [=[	dsta = dstext->arrays;]=])

set_target_properties(librw PROPERTIES SOURCES "${librw_sources}")
target_include_directories(librw PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/librw/src")
