<<<<<<< HEAD
add_library(common INTERFACE)

target_sources(common PUBLIC
        Asteroid.h
        GameSnapshot.h
        Spaceship.h
)
=======
add_library(common STATIC)
target_include_directories(common PUBLIC include)
target_sources(common PRIVATE src/protocol.cpp)
target_link_libraries(common PUBLIC Boost::boost)

# common/CMakeLists.txt

add_library(common_lib STATIC)

target_include_directories(common_lib PUBLIC include)

target_sources(common_lib PRIVATE
    src/common_lib.cpp
)

target_link_libraries(common_lib PUBLIC Boost::boost)
>>>>>>> f316033 (add build configuration)
