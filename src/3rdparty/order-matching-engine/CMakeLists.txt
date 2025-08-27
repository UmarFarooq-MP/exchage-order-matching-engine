cmake_minimum_required(VERSION 3.10)  # 4.0 doesn't exist, latest is 3.x
project(order_matching_engine)

# Prefer Homebrew LLVM on macOS
if(APPLE)
    set(HOMEBREW_LLVM "/opt/homebrew/opt/llvm")

    if(EXISTS "${HOMEBREW_LLVM}/bin/clang++")
        message(STATUS "Using Homebrew LLVM toolchain from ${HOMEBREW_LLVM}")

        set(CMAKE_C_COMPILER "${HOMEBREW_LLVM}/bin/clang" CACHE STRING "" FORCE)
        set(CMAKE_CXX_COMPILER "${HOMEBREW_LLVM}/bin/clang++" CACHE STRING "" FORCE)

        # Ensure headers and libs are found
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${HOMEBREW_LLVM}/include" CACHE STRING "" FORCE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${HOMEBREW_LLVM}/lib" CACHE STRING "" FORCE)
    else()
        message(STATUS "Homebrew LLVM not found, falling back to system compiler")
    endif()
endif()


if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 14)

# Boost (only headers needed, no linking usually)
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# --- Library: liquibook ---
file(GLOB LIQUIBOOK_SRC
        src/*.cpp
        src/book/*.cpp
        src/simple/*.cpp
)

add_library(liquibook ${LIQUIBOOK_SRC})
target_include_directories(liquibook PUBLIC include src)


# --- Unit Tests (optional) ---
file(GLOB UNIT_TESTS test/unit/*.cpp test/latency/*.cpp test/perf/*.cpp)
add_executable(liquibook_tests ${UNIT_TESTS})
target_link_libraries(liquibook_tests liquibook ${Boost_LIBRARIES})

# Add QuickFAST include dir
include_directories(${CMAKE_SOURCE_DIR}/../quickfast/src)

