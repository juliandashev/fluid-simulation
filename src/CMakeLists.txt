add_executable(fluid_simulation main.cc simulation.cc renderer.cc shader.cc spacial_grid.cc debug_gui.cc)

find_package(TBB REQUIRED)

target_link_libraries(fluid_simulation PRIVATE
    glfw
    glad
    glm::glm
    OpenGL::GL
    TBB::tbb
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
