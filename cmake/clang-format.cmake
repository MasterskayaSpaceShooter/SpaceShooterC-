# ---------------------------------------------------------------------------
# clang-format integration
#
# Provides two aggregate targets:
#   check-format   -> dry-run check (fails on any formatting violation)
#   fix-format     -> in-place format
#
# Both are composed of per-target sub-targets (e.g. MyLib-check-format,
# MyLib-fix-format), which can also be invoked individually.
#
# Design note: a source file that is listed in several targets is formatted
# once per owning target. This is intentional — the per-target sub-targets
# remain useful on their own, and the aggregate is a simple fan-out. If this
# ever becomes a build-time bottleneck, replace the fan-out with a global
# de-duplicating driver that owns the clang-format invocation.
#
# Known limitation: ${MATCHING_SOURCES} is expanded in a single command, so a
# target with several thousand files can exceed the OS command-line limit
# (notably ~8k chars on Windows). Switch to a clang-format response file
# (@file) if this ever becomes a problem.
# ---------------------------------------------------------------------------

find_program(CLANG_FORMAT_EXE
        NAMES clang-format
        DOC "Path to clang-format executable"
)

if (NOT CLANG_FORMAT_EXE)
    message(WARNING "clang-format not found! clang-format targets are disabled.")
    return()
endif ()

message(STATUS "Using clang-format: ${CLANG_FORMAT_EXE}")

# Warn if the installed version differs from the project's recommended one.
execute_process(
        COMMAND ${CLANG_FORMAT_EXE} --version
        OUTPUT_VARIABLE _clang_format_version_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)
string(REGEX MATCH "clang-format version ([0-9]+\\.[0-9]+\\.[0-9]+)"
        _ "${_clang_format_version_output}")
set(CLANG_FORMAT_VERSION "${CMAKE_MATCH_1}")

if (CLANG_FORMAT_VERSION AND NOT CLANG_FORMAT_VERSION VERSION_EQUAL "18.1.8")
    message(WARNING
            "clang-format ${CLANG_FORMAT_VERSION} found, but 18.1.8 is recommended")
endif ()

# --style=file silently falls back to LLVM style if .clang-format is missing.
if (NOT EXISTS "${CMAKE_SOURCE_DIR}/.clang-format")
    message(WARNING
            "No .clang-format file found in ${CMAKE_SOURCE_DIR}. "
            "clang-format will fall back to the default LLVM style.")
endif ()

# Regex applied to the lower-cased file extension (including the leading dot).
set(CLANG_FORMAT_FILE_REGEX "^\\.(cpp|cc|cxx|h|hh|hpp|hxx)$")

#[[
@brief Registers clang-format check/fix sub-targets for one build target.

Reads the target's SOURCES (or INTERFACE_SOURCES for INTERFACE_LIBRARY),
filters to C/C++ files, and wires up "<target>-check-format" and
"<target>-fix-format" custom targets that depend on the aggregate
"check-format" / "fix-format" targets.

Silently ignores:
  * names that do not refer to a real target,
  * imported targets (including namespaced aliases like Boost::boost),
  * targets that contribute no C/C++ sources,
  * targets that have already been registered.

@param TARGET_NAME Name of the build target to register.

@note A source file shared between multiple targets will be formatted once
      per owning target. This is a deliberate trade-off: per-target
      sub-targets remain independently invokable, at the cost of some
      duplicated work on shared headers and sources.
]]
function(add_clang_format_target TARGET_NAME)
    if (NOT TARGET ${TARGET_NAME})
        return()
    endif ()

    get_target_property(_is_imported ${TARGET_NAME} IMPORTED)
    if (_is_imported)
        return()
    endif ()

    # Aliases are required by CMake to contain "::".
    if (TARGET_NAME MATCHES "::")
        return()
    endif ()

    # Idempotence: skip if this target has already been registered.
    if (TARGET ${TARGET_NAME}-check-format)
        return()
    endif ()

    # INTERFACE_LIBRARY (header-only) exposes its sources via
    # INTERFACE_SOURCES, not SOURCES.
    get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
    if (TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
        get_target_property(TARGET_SOURCES ${TARGET_NAME} INTERFACE_SOURCES)
    else ()
        get_target_property(TARGET_SOURCES ${TARGET_NAME} SOURCES)
    endif ()

    if (NOT TARGET_SOURCES)
        return()
    endif ()

    # SOURCES / INTERFACE_SOURCES entries are relative to the directory in
    # which the target was defined, not to CMAKE_CURRENT_SOURCE_DIR. Resolve
    # against the target's SOURCE_DIR to be safe.
    get_target_property(TARGET_SOURCE_DIR ${TARGET_NAME} SOURCE_DIR)
    if (NOT TARGET_SOURCE_DIR)
        set(TARGET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif ()

    set(MATCHING_SOURCES "")
    foreach (SOURCE_FILE ${TARGET_SOURCES})
        # Skip any entry that contains a generator expression, not just ones that
        # begin with it: a $<CONFIG> token embedded in a path would otherwise pass
        # the extension check and be resolved as a literal path.
        if (SOURCE_FILE MATCHES "\\$<")
            continue()
        endif ()

        get_filename_component(SOURCE_EXT "${SOURCE_FILE}" EXT)
        string(TOLOWER "${SOURCE_EXT}" SOURCE_EXT_LOWER)

        if (SOURCE_EXT_LOWER MATCHES "${CLANG_FORMAT_FILE_REGEX}")
            get_filename_component(ABS_FILE "${SOURCE_FILE}" ABSOLUTE
                    BASE_DIR "${TARGET_SOURCE_DIR}")
            list(APPEND MATCHING_SOURCES "${ABS_FILE}")
        endif ()
    endforeach ()

    # De-duplicate only within this target. Cross-target duplicates are
    # intentionally kept; see the design note at the top of the file.
    if (MATCHING_SOURCES)
        list(REMOVE_DUPLICATES MATCHING_SOURCES)
    else ()
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

#[[
@brief Registers clang-format sub-targets for every build target in the
       project (recursively).

Uses the global TARGETS property (CMake >= 3.7), which is populated as
targets are created. Call once from the top-level CMakeLists.txt after
all add_subdirectory() calls. Imported and alias targets are skipped by
add_clang_format_target().

@note The aggregate "check-format" / "fix-format" targets are simple
      fan-outs over the per-target sub-targets. They do not de-duplicate
      files shared between targets; see the design note at the top of this
      file for the rationale.
]]
function(auto_clang_format_all)
    get_property(all_targets GLOBAL PROPERTY TARGETS)
    foreach (tgt ${all_targets})
        if (tgt MATCHES "::")
            continue()
        endif ()
        get_target_property(type ${tgt} TYPE)
        if (type MATCHES "STATIC_LIBRARY|SHARED_LIBRARY|EXECUTABLE|OBJECT_LIBRARY|MODULE_LIBRARY|INTERFACE_LIBRARY")
            add_clang_format_target(${tgt})
        endif ()
    endforeach ()
endfunction()
