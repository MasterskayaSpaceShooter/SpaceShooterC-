add_executable(unit_tests_common
        test_frame_codec.cpp
        test_math_elastic_impulse.cpp
        test_math_normalizeAngle.cpp
        test_vector2d.cpp
        test_math_clamp_to_world_bounds.cpp
        test_circle_collision.cpp
	test_math_ship_physics.cpp
        test_entity_snapshot_serder.cpp
        test_game_snapshot.cpp
)

target_link_libraries(unit_tests_common PRIVATE common GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests_common)
