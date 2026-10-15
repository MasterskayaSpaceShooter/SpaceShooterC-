# SpaceShooterC-

## FlatBuffers integration

Wire format is defined in `schemas/` and mirrored 1:1
by the DTO in `src/common/`.

### Conan

`flatbuffers` is a normal (host) dependency — one package provides both the
runtime library and the `flatc` compiler.

`CMakeDeps` generates two imported targets:

- `flatbuffers::flatbuffers` — link target
- `flatbuffers::flatc` — tool used by the codegen step

### CMake

```cmake
find_package(flatbuffers CONFIG REQUIRED)

include(${PROJECT_SOURCE_DIR}/cmake/flatbuffers_codegen.cmake)

add_flatbuffers_schema(entity_snapshot_fbs
        "${PROJECT_SOURCE_DIR}/schemas/entity_snapshot.fbs")

target_link_libraries(common
        PRIVATE entity_snapshot_fbs
)
```

### Pipeline

```
entity_snapshot.fbs
      │ flatc --cpp --scoped-enums   (build-time, via entity_snapshot_fbs_gen)
      ▼
entity_snapshot_generated.h  ──► snapshot/wire serder (.cpp in common)
                                          ▲
                                          │
                              entity_snapshot.h (DTO, public)
```
