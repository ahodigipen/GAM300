# No include() needed — all handled by find_package()
project(BoomEngine LANGUAGES CXX)
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libraries/debug")

add_library(BoomEngine SHARED
    src/BoomEngine.cpp
    includes/BoomEngine.h
)

target_include_directories(BoomEngine PUBLIC includes)

# This is to install any package u need.
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(imgui REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(BoomEngine
    PUBLIC
    glfw
    GLEW::GLEW
    glm::glm
    imgui::imgui
    OpenGL::GL
)
