#pragma once

#include "graphics_internal.hpp"

namespace application {

void configure_imgui();
void apply_pending_imgui_scale();

bool initialize();
void shutdown();

void update(double time);
void render(const graphics::internal::FrameData& fd);

} // namespace application