cmake_minimum_required(VERSION 3.10)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_MakeAvailable(raylib)

project(MyProject C)
# add_compile_options(-g -O2)

add_executable(my_program 
	src/main.c
    src/world.c
    src/render.c
    src/hash_table.c
    src/noise_init.c
    src/fifo.c
    src/files_api.c
    src/destroy_stack.c
)

target_include_directories(my_program PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(my_program PRIVATE raylib)