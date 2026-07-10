#pragma once

#include "defs.hpp"

struct GLFWwindow;

// Thin RAII wrapper around Dear ImGui. Owns the ImGui context and the GLFW/GL3
// backends, so setup happens in the constructor and teardown in the destructor
// (while the GL context is still alive). Keeps every ImGui symbol confined to
// debug_gui.cc -- the rest of the app never includes <imgui.h>.
class DebugGui {
public:
    explicit DebugGui(GLFWwindow* window);
    ~DebugGui();

    DebugGui(const DebugGui&) = delete;             // owns a global context
    DebugGui& operator=(const DebugGui&) = delete;

    void begin_frame();                        // start a new ImGui frame
    void draw_params(SimParams& params);
    void end_frame();                          // render the UI on top of the scene
    bool wants_mouse() const;                  // true when the cursor is over the panel
};
