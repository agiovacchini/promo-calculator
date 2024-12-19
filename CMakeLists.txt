cmake_minimum_required(VERSION 3.22.1)

# Project name
project(promo-calculator)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Directory paths
set(SRCDIR src)
set(INCLUDEDIR include)
set(BINDIR bin)
set(OBJDIR obj)

# Source files
file(GLOB SRCFILES "${SRCDIR}/*.cpp")
list(FILTER SRCFILES EXCLUDE REGEX "src/stdafx.cpp")

# Include directories
include_directories(${INCLUDEDIR})

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -w -Wall -pthread -DBOOST_BIND_GLOBAL_PLACEHOLDERS")

# Linker flags
set(BOOST_LIBS
boost_atomic
boost_system
boost_filesystem
boost_iostreams
boost_log
boost_date_time
boost_regex
boost_log_setup
boost_chrono
boost_thread
)

# Output binary
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/${BINDIR})
add_executable(promo-calculator ${SRCFILES})

# Link libraries
target_link_libraries(promo-calculator ${BOOST_LIBS})
