cmake_minimum_required(VERSION 3.14)
project(eelib VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources for the eelib library
set(EELIB_SOURCES
		order.cpp
		#matcher.cpp
		#notifier.cpp
		#abm.cpp
		agent.cpp
		#agent_manager.cpp
		matcher.cpp
		notifier.cpp
		limits_bin.cpp
)

add_library(eelib STATIC ${EELIB_SOURCES})
target_include_directories(eelib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# target_compile_options(eelib PRIVATE -Wall -Wextra -Wpedantic)

add_executable(eelib_app main.cpp)
target_link_libraries(eelib_app PRIVATE eelib)

enable_testing()

include(FetchContent)
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG v1.17.0
	UPDATE_DISCONNECTED ON
)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(test_eelib ${TEST_SOURCES})
target_include_directories(test_eelib BEFORE PRIVATE
	${googletest_SOURCE_DIR}/googletest/include
	${googletest_SOURCE_DIR}/googletest
)
target_link_libraries(test_eelib PRIVATE eelib gtest_main)

# TODO remove CTest
add_test(NAME eelib_tests COMMAND test_eelib)