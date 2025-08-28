cmake_minimum_required(VERSION 3.16)
project(ome)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# ---- RocksDB (fetched at configure time, lightweight build) ----
set(ROCKSDB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ROCKSDB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(WITH_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_BENCHMARK_TOOLS OFF CACHE BOOL "" FORCE)
set(WITH_TOOLS OFF CACHE BOOL "" FORCE)
set(WITH_CORE_TOOLS OFF CACHE BOOL "" FORCE)
set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
set(PORTABLE ON CACHE BOOL "" FORCE)

FetchContent_Declare(
        rocksdb
        GIT_REPOSITORY https://github.com/facebook/rocksdb.git
        GIT_TAG v9.0.0
)
FetchContent_MakeAvailable(rocksdb)
set_target_properties(rocksdb PROPERTIES COMPILE_FLAGS "-Wno-error")

# ---- nlohmann/json ----
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# ---- Liquibook ----
add_subdirectory(src/3rdParty/order-matching-engine)

# ---- Your executables ----
add_executable(ome
        src/main.cpp
        src/engine/matching_engine.cpp
        src/wal/wal_manager.cpp
        src/broadcast/broadcaster.h
)

add_executable(benchmark src/benchmark.cpp)

# ---- Link with libs ----
target_link_libraries(ome PRIVATE liquibook rocksdb nlohmann_json::nlohmann_json)
target_link_libraries(benchmark PRIVATE liquibook rocksdb)

# ---- Include dirs (Project + RocksDB) ----
target_include_directories(ome PRIVATE
        ${rocksdb_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src           # so wal/wal.h and engine/… are found
)
target_include_directories(benchmark PRIVATE
        ${rocksdb_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
