cmake_minimum_required(VERSION 3.8)
project(os_simulator)

set(CMAKE_CXX_STANDARD 11)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext/boost/download)

execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext/boost/download)

file(GLOB SOURCE_FILES
          "framework/threading/*.cpp"
          "framework/io/*.cpp"
          "framework/structures/*.cpp"
          "framework/structures/StructureManager.h"
          "framework/structures/MapManager.h")

include_directories(framework includes ${CMAKE_BINARY_DIR}/ext/boostinstall/include)

link_directories(${CMAKE_BINARY_DIR}/ext/boostinstall/lib)
add_library(os_simulator ${SOURCE_FILES})


target_link_libraries(os_simulator boost_atomic boost_regex boost_thread boost_chrono)
