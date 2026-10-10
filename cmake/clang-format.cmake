
# ---------------------------------------------------------------------------
# clang-format integration
#
# Provides two aggregate targets:
#   check-format   -> dry-run check (fails on any formatting violation)
#   fix-format     -> in-place format
#
# Both are composed of per-target sub-targets (e.g. MyLib),
# which can also be invoked individually.
# ---------------------------------------------------------------------------

find_program(CLANG_FORMAT_EXE
        NAMES clang-format
        DOC "Path to clang-format executable"
)

if (NOT CLANG_FORMAT_EXE)
    message(WARNING "clang-format not found! Formatting targets are disabled.")
    return()
endif ()

message(STATUS "Using clang-format: ${CLANG_FORMAT_EXE}")

# --style=file silently falls back to LLVM style if .clang-format is missing
if (NOT EXISTS "${CMAKE_SOURCE_DIR}/.clang-format")
    message(WARNING
            "No .clang-format file found in ${CMAKE_SOURCE_DIR}. "
            "clang-format will fall back to the default LLVM style.")
endif ()

set(CLANG_FORMAT_FILE_REGEX "\\.(cpp|cc|cxx|h|hh|hpp|hxx)$")

function(add_clang_format_target TARGET_NAME)
    # silently ignore imported / alias targets (Boost::boost, etc.)
    # and anything that isn't a real buildsystem target.
    if (NOT TARGET ${TARGET_NAME})
        return()
    endif ()
    if (TARGET_NAME MATCHES "::")
        return()
    endif ()

    get_target_property(TARGET_SOURCES ${TARGET_NAME} SOURCES)
    if (NOT TARGET_SOURCES)
        return()
    endif ()

    # SOURCES entries are relative to the directory in which the
    # target was defined, not to CMAKE_CURRENT_SOURCE_DIR. Resolve against
    # the target's SOURCE_DIR to be safe.
    get_target_property(TARGET_SOURCE_DIR ${TARGET_NAME} SOURCE_DIR)
    if (NOT TARGET_SOURCE_DIR)
        set(TARGET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif ()

    set(MATCHING_SOURCES "")
    foreach (SOURCE_FILE ${TARGET_SOURCES})
        # Skip generator expressions like $<TARGET_OBJECTS:...> and $<...>
        if (SOURCE_FILE MATCHES "^\\$<")
            continue()
        endif ()
        if (SOURCE_FILE MATCHES "${CLANG_FORMAT_FILE_REGEX}")
            get_filename_component(ABS_FILE "${SOURCE_FILE}" ABSOLUTE
                    BASE_DIR "${TARGET_SOURCE_DIR}")
            list(APPEND MATCHING_SOURCES "${ABS_FILE}")
        endif ()
    endforeach ()

    if (NOT MATCHING_SOURCES)
        return()
    endif ()

    # --- Aggregate: check ---
    if (NOT TARGET check-format)
        add_custom_target(check-format)
    endif ()

    set(SUB_TARGET_CHECK "${TARGET_NAME}-check-format")
    add_custom_target(${SUB_TARGET_CHECK}
            COMMAND ${CLANG_FORMAT_EXE}
            --dry-run --Werror --style=file
            ${MATCHING_SOURCES}
            COMMENT "Checking format for target: ${TARGET_NAME}"
            VERBATIM
    )
    add_dependencies(check-format ${SUB_TARGET_CHECK})

    # --- Aggregate: fix ---
    if (NOT TARGET fix-format)
        add_custom_target(fix-format)
    endif ()

    set(SUB_TARGET_FIX "${TARGET_NAME}-fix-format")
    add_custom_target(${SUB_TARGET_FIX}
            COMMAND ${CLANG_FORMAT_EXE}
            -i --style=file
            ${MATCHING_SOURCES}
            COMMENT "Fixing format for target: ${TARGET_NAME}"
            VERBATIM
    )
    add_dependencies(fix-format ${SUB_TARGET_FIX})
endfunction()

# Auto-register every buildsystem target defined so far in the current
# directory scope. Must be called AFTER all add_subdirectory() calls.
function(auto_clang_format_all)
    get_property(all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach (tgt ${all_targets})
        if (tgt MATCHES "::")
            continue()
        endif ()
        get_target_property(type ${tgt} TYPE)
        if (type MATCHES "STATIC_LIBRARY|SHARED_LIBRARY|EXECUTABLE|OBJECT_LIBRARY|MODULE_LIBRARY")
            add_clang_format_target(${tgt})
        endif ()
    endforeach ()
endfunction()
