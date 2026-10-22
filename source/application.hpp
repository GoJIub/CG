#pragma once

#include "graphics_internal.hpp"

struct GLFWwindow;

namespace application {

void configure_imgui();
void apply_pending_imgui_scale();

bool initialize();
void shutdown(GLFWwindow* window);

bool update(double time, GLFWwindow* window);
void render(const graphics::internal::FrameData& fd);

} // namespace application