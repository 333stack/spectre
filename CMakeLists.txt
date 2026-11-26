cmake_minimum_required(VERSION 3.24)
project(spectre CXX)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(../kitui kitui)
add_executable(spectre main.cpp)
target_link_libraries(spectre PRIVATE kitui_app)

add_executable(queue_test EXCLUDE_FROM_ALL tests/queue_test.cpp)  # needs network + yt-dlp: cmake --build build --target queue_test
target_link_libraries(queue_test PRIVATE kitui_app)
