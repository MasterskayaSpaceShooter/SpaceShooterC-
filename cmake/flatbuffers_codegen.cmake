# Requires: find_package(flatbuffers CONFIG REQUIRED) already done by caller
# Function is intended to be called once per schema in one directory

function(add_flatbuffers_schema TARGET_NAME SCHEMA_FILE)
    if (TARGET ${TARGET_NAME})
        return()
    endif ()

    get_filename_component(SCHEMA_NAME "${SCHEMA_FILE}" NAME_WE)
    set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/${SCHEMA_NAME}")
    set(GENERATED_HEADER "${GENERATED_DIR}/${SCHEMA_NAME}_generated.h")

    file(MAKE_DIRECTORY "${GENERATED_DIR}")

    add_custom_command(
            OUTPUT "${GENERATED_HEADER}"
            COMMAND $<TARGET_FILE:flatbuffers::flatc>
            --cpp --scoped-enums
            -o "${GENERATED_DIR}"
            "${SCHEMA_FILE}"
            DEPENDS "${SCHEMA_FILE}" flatbuffers::flatc
            COMMENT "flatc: ${SCHEMA_FILE} -> ${GENERATED_DIR}"
            VERBATIM
    )

    add_custom_target(${TARGET_NAME}_gen DEPENDS "${GENERATED_HEADER}")

    add_library(${TARGET_NAME} INTERFACE)
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_gen)
    target_include_directories(${TARGET_NAME} INTERFACE "${GENERATED_DIR}")
    target_link_libraries(${TARGET_NAME} INTERFACE flatbuffers::flatbuffers)
endfunction()
