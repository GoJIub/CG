#include "camera_controller.hpp"
#include "scene.hpp"

#include <algorithm>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

namespace {

constexpr float movement_speed = 3.0f;
constexpr float mouse_sensitivity = 0.12f;
constexpr float maximum_delta_time = 0.1f;
constexpr float maximum_pitch = 89.0f;

} // namespace


namespace camera_controller {

void release(
    State& state,
    GLFWwindow* window
) {
    if (!state.captured) {
        return;
    }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

    state.captured = false;
}

void update(
    State& state,
    GLFWwindow* window,
    scene::CameraSettings& camera
) {
    auto& io = ImGui::GetIO();

    const bool right_mouse_down =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)
            == GLFW_PRESS;

    const bool right_mouse_pressed =
        right_mouse_down && !state.right_mouse_was_down;

    state.right_mouse_was_down = right_mouse_down;

    const bool focused =
        glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;

    const bool escape_down =
        glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;

    if (!focused || escape_down || !right_mouse_down) {
        release(state, window);
        return;
    }

    if (!state.captured) {
        if (!right_mouse_pressed ||
            io.WantCaptureMouse ||
            io.WantCaptureKeyboard) {
            return;
        }

        glfwSetInputMode(
            window,
            GLFW_CURSOR,
            GLFW_CURSOR_DISABLED
        );

        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;

        glfwGetCursorPos(
            window,
            &state.previous_mouse_x,
            &state.previous_mouse_y
        );

        state.captured = true;
        return;
    }

    double mouse_x = 0.0;
    double mouse_y = 0.0;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);

    const float delta_x =
        static_cast<float>(mouse_x - state.previous_mouse_x);

    const float delta_y =
        static_cast<float>(mouse_y - state.previous_mouse_y);

    state.previous_mouse_x = mouse_x;
    state.previous_mouse_y = mouse_y;

    camera.rotation_degrees.y -= delta_x * mouse_sensitivity;
    camera.rotation_degrees.x -= delta_y * mouse_sensitivity;

    camera.rotation_degrees.x = std::clamp(
        camera.rotation_degrees.x,
        -maximum_pitch,
        maximum_pitch
    );

    camera.rotation_degrees.y = glm::mod(
        camera.rotation_degrees.y + 180.0f,
        360.0f
    ) - 180.0f;

    if (io.WantCaptureKeyboard) {
        return;
    }

    const glm::mat4 camera_world =
        scene::compute_camera_world_matrix(camera);

    const glm::vec3 right = glm::vec3(camera_world[0]);
    const glm::vec3 forward = -glm::vec3(camera_world[2]);

    glm::vec3 movement{0.0f};

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        movement += forward;
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        movement -= forward;
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        movement += right;
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        movement -= right;
    }

    if (glm::dot(movement, movement) > 0.0f) {
        const float delta_time = std::clamp(
            io.DeltaTime,
            0.0f,
            maximum_delta_time
        );

        camera.position +=
            glm::normalize(movement) *
            movement_speed *
            delta_time;
    }
}

} // namespace camera_controller