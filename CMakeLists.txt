cmake_minimum_required(VERSION 3.24)
project(spectre CXX)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(../kitui kitui)
add_executable(spectre main.cpp)
target_link_libraries(spectre PRIVATE kitui_app)
if(UNIX)  # release binary only needs glibc + SDL3 on the user's system
  target_link_options(spectre PRIVATE -static-libstdc++ -static-libgcc)
endif()

add_executable(queue_test EXCLUDE_FROM_ALL tests/queue_test.cpp)  # needs network + yt-dlp: cmake --build build --target queue_test
target_link_libraries(queue_test PRIVATE kitui_app)

install(TARGETS spectre)
if(UNIX)
  install(FILES spectre.desktop DESTINATION share/applications)
endif()
