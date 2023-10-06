cmake_minimum_required(VERSION 3.19.2)
project(project2_threading)
if(CMAKE_SYSTEM_NAME MATCHES Linux)
  execute_process(COMMAND dpkg -l git uuid-dev build-essential cmake libboost1.71-tools-dev
          RESULT_VARIABLE result
          OUTPUT_QUIET ERROR_QUIET)
  if(result)
    message(
            FATAL_ERROR
            "Missing dependencies. Fix by running this command:\nsudo apt -y install git uuid-dev build-essential cmake libboost1.71-tools-dev"
    )
  endif()
endif()

# Download googletest
configure_file(CMakeLists.txt.in ext/googletest/download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
                RESULT_VARIABLE result
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext/googletest/download)
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
                RESULT_VARIABLE result
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext/googletest/download)
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

if(CMAKE_SYSTEM_NAME MATCHES Darwin)
  set (BUILD_ARGS "architecture=combined cxxflags=\"-arch x86_64 -arch arm64\"")
endif()

configure_file(CMakeLists.txt.in ext/boost/download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext/boost/download)

execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext/boost/download)


set(CMAKE_BUILD_TYPE Debug)
add_subdirectory(${CMAKE_BINARY_DIR}/ext/googletest/src
                 ${CMAKE_BINARY_DIR}/ext/googletest/build EXCLUDE_FROM_ALL)
add_subdirectory(os_simulator)

set(CMAKE_CXX_STANDARD 11)
include_directories(. os_simulator/includes os_simulator/framework test answer ${CMAKE_BINARY_DIR}/ext/boostinstall/include)
set(SOURCE_FILES
    test/main.cpp
    test/test_helper.cpp
    test/test_helper.h
    answer/thread.cpp
    answer/thread_lock.h
    answer/test_config.h answer/lock.cpp)

link_directories(${CMAKE_BINARY_DIR}/ext/boostinstall/lib)
add_executable(project2_threading ${SOURCE_FILES})

if(CMAKE_SYSTEM_NAME MATCHES Linux)
target_link_libraries(project2_threading gtest os_simulator boost_atomic boost_regex boost_thread boost_chrono uuid )
endif()

if(CMAKE_SYSTEM_NAME MATCHES Darwin)
target_link_libraries(project2_threading gtest os_simulator boost_atomic boost_regex boost_thread boost_chrono )
endif()

