project(BoomEngine LANGUAGES CXX)

add_library(BoomEngine SHARED
    src/BoomEngine.cpp
    includes/BoomEngine.h
)

# 2a) Include dirs
target_include_directories(BoomEngine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/includes
  ${CMAKE_CURRENT_SOURCE_DIR}/includes/common
)


# 2b) Your custom preprocessor defs
target_compile_definitions(BoomEngine
  PUBLIC
    $<$<CONFIG:Debug>:MY_DEBUG_DEF>
    $<$<CONFIG:Release>:NDEBUG>
)

# 2c) Your custom compiler flags
target_compile_options(BoomEngine
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>                 # warning level 4 on MSVC
    $<$<CONFIG:Release>:/O2>                       # optimize for speed
)

# 2d) Find the Conan-installed libs
find_package(glfw3    REQUIRED)  # from CMakeDeps :contentReference[oaicite:0]{index=0}&#8203;:contentReference[oaicite:1]{index=1}
find_package(GLEW     REQUIRED)
find_package(glm      REQUIRED)
find_package(imgui    REQUIRED)
find_package(OpenGL   REQUIRED)

# 2e) Link libraries + any extra .lib you manually had
target_link_libraries(BoomEngine
  PUBLIC
    glfw
    GLEW::GLEW
    glm::glm
    imgui::imgui
    OpenGL::GL
    $<$<CONFIG:Debug>:MyExtraDebugOnly.lib>
)

# 2f) Linker flags (if you set any)
set_target_properties(BoomEngine PROPERTIES
  LINK_FLAGS $<$<CONFIG:Release>:"/SUBSYSTEM:WINDOWS">
)
