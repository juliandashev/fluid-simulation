# CONFIGURE_DEPENDS re-runs the glob at build time, so new .cc files are
# picked up without editing this file.
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

add_executable(fluid_simulation ${SOURCES})

target_include_directories(fluid_simulation PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(fluid_simulation PRIVATE
    glfw
    glad
    glm::glm
    OpenGL::GL
    imgui
)

target_compile_definitions(fluid_simulation PRIVATE
    SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders"
)

target_compile_options(fluid_simulation PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
     $<$<CONFIG:Release>:-march=native>
     $<$<CONFIG:Release>:-ffast-math>
)

set_property(TARGET fluid_simulation PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
