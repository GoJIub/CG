#pragma once

#include "graphics_internal.hpp"
#include "scene.hpp"

namespace scene_renderer {

bool initialize();
void shutdown();
bool update_uniform_buffer(const scene::GlobalUniforms& uniforms);
void render(const graphics::internal::FrameData& fd);

} // namespace scene_renderer