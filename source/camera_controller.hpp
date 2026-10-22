#pragma once

struct GLFWwindow;

namespace scene {
struct CameraSettings;
}

namespace camera_controller {

struct State {
    bool captured = false;
    bool right_mouse_was_down = false;

    double previous_mouse_x = 0.0;
    double previous_mouse_y = 0.0;
};

void update(
    State& state,
    GLFWwindow* window,
    scene::CameraSettings& camera
);

void release(
    State& state,
    GLFWwindow* window
);

} // namespace camera_controller