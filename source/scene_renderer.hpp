#pragma once

#include "graphics_internal.hpp"
#include "scene.hpp"

namespace scene_renderer {

bool initialize();
void shutdown();
bool update_uniform_buffers(const scene::FrameUniformData& uniforms);
void render(
    const graphics::internal::FrameData& fd,
    const scene::FrameUniformData& uniforms
);

} // namespace scene_renderer