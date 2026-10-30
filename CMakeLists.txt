cmake_minimum_required(VERSION 3.15)
project(voxel_engine VERSION 1.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

execute_process(
    COMMAND python3 -m pybind11 --cmakedir
    OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(PYBIND11_CMAKE_DIR)
    set(pybind11_DIR ${PYBIND11_CMAKE_DIR})
endif()

find_package(pybind11 REQUIRED)

# ============================================================================
# Поиск GLFW
# ============================================================================
find_package(glfw3 REQUIRED)

# ============================================================================
# GLAD (загрузчик OpenGL)
# ============================================================================
add_library(glad STATIC glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)
set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(LIB_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/lib)
set(PYTHON_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/python)

file(MAKE_DIRECTORY ${LIB_OUTPUT_DIR})
file(MAKE_DIRECTORY ${PYTHON_OUTPUT_DIR})

# ============================================================================
# 1. БИБЛИОТЕКА ВЕКТОРОВ
# ============================================================================
add_library(vec SHARED 
    src/vec.cpp
)
target_include_directories(vec PUBLIC include)
set_target_properties(vec PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
    POSITION_INDEPENDENT_CODE ON
)

# ============================================================================
# 2. CORE БИБЛИОТЕКА
# ============================================================================
add_library(core SHARED 
    src/image.cpp
    src/texture.cpp
    src/mesh.cpp
    src/entity.cpp
    src/dummy.cpp
    src/block.cpp
    src/item.cpp
    src/view_cam.cpp
    src/chunk.cpp
    src/chunk_mesher.cpp
    src/light_engine.cpp
    src/world.cpp
    src/mesh_lighting.cpp
    src/raycast.cpp
    src/collision.cpp
    src/movement.cpp
    src/input.cpp
)
target_include_directories(core PUBLIC include)
# src/input.cpp использует GLFW (glfwGetKey и т.д.), поэтому core линкуется с glfw,
# иначе при импорте модулей возникает "undefined symbol: glfwGetKey"
target_link_libraries(core PUBLIC vec glfw)
set_target_properties(core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
    POSITION_INDEPENDENT_CODE ON
)

# ============================================================================
# 3. PYTHON МОДУЛИ
# ============================================================================
pybind11_add_module(vec_module bindings/vec_pybind.cpp)
target_link_libraries(vec_module PRIVATE vec)
set_target_properties(vec_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(entity_module bindings/entity_pybind.cpp)
target_link_libraries(entity_module PRIVATE core)
set_target_properties(entity_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(view_cam_module bindings/view_cam_pybind.cpp)
target_link_libraries(view_cam_module PRIVATE core)
set_target_properties(view_cam_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(item_module bindings/item_pybind.cpp)
target_link_libraries(item_module PRIVATE core)
set_target_properties(item_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(image_module bindings/image_pybind.cpp)
target_link_libraries(image_module PRIVATE core)
set_target_properties(image_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(texture_module bindings/texture_pybind.cpp)
target_link_libraries(texture_module PRIVATE core)
set_target_properties(texture_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(mesh_module bindings/mesh_pybind.cpp)
target_link_libraries(mesh_module PRIVATE core)
set_target_properties(mesh_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(mesh_lighting_module bindings/mesh_lighting_pybind.cpp)
target_link_libraries(mesh_lighting_module PRIVATE core)
set_target_properties(mesh_lighting_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(block_module bindings/block_pybind.cpp)
target_link_libraries(block_module PRIVATE core)
set_target_properties(block_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(chunk_module bindings/chunk_pybind.cpp)
target_link_libraries(chunk_module PRIVATE core)
set_target_properties(chunk_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(chunk_mesher_module bindings/chunk_mesher_pybind.cpp)
target_link_libraries(chunk_mesher_module PRIVATE core)
set_target_properties(chunk_mesher_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(world_module bindings/world_pybind.cpp)
target_link_libraries(world_module PRIVATE core)
set_target_properties(world_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(collision_module bindings/collision_pybind.cpp)
target_link_libraries(collision_module PRIVATE core)
set_target_properties(collision_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(movement_module bindings/movement_pybind.cpp)
target_link_libraries(movement_module PRIVATE core)
set_target_properties(movement_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(light_engine_module bindings/light_engine_pybind.cpp)
target_link_libraries(light_engine_module PRIVATE core)
set_target_properties(light_engine_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(input_module bindings/input_pybind.cpp)
target_link_libraries(input_module PRIVATE core)
set_target_properties(input_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(raycast_module bindings/raycast_pybind.cpp)
target_link_libraries(raycast_module PRIVATE core)
set_target_properties(raycast_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(dummy_module bindings/dummy_pybind.cpp)
target_link_libraries(dummy_module PRIVATE core)
set_target_properties(dummy_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# ============================================================================
# 4. RENDERER
# ============================================================================
add_library(renderer SHARED
    src/renderer.cpp
    src/camera.cpp
)
target_include_directories(renderer PUBLIC include glad/include)
target_link_libraries(renderer PUBLIC core glad glfw)
set_target_properties(renderer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
    POSITION_INDEPENDENT_CODE ON
)

pybind11_add_module(renderer_module bindings/renderer_pybind.cpp)
target_link_libraries(renderer_module PRIVATE renderer)
set_target_properties(renderer_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

pybind11_add_module(camera_module bindings/camera_pybind.cpp)
target_link_libraries(camera_module PRIVATE renderer)
set_target_properties(camera_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)


