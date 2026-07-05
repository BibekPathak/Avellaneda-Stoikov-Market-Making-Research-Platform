add_library(mm_core INTERFACE)
target_include_directories(mm_core INTERFACE include)

add_subdirectory(tests)

add_executable(mm_simulate tools/mm_simulate_main.cpp)
target_link_libraries(mm_simulate PRIVATE mm_core hft_core)
target_include_directories(mm_simulate PRIVATE include)
