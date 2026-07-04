include(GoogleTest)

add_executable(test_inventory test_inventory.cpp)
target_link_libraries(test_inventory PRIVATE mm_core hft_core gtest_main)
gtest_discover_tests(test_inventory)

add_executable(test_volatility test_volatility.cpp)
target_link_libraries(test_volatility PRIVATE mm_core hft_core gtest_main)
gtest_discover_tests(test_volatility)
