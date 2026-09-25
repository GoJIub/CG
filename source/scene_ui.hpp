#pragma once

#include "scene.hpp"

namespace scene_ui {

void configure();
void apply_pending_scale();
void draw_scene_controls(
    scene::SceneSettings& settings,
    scene::AnimationState& animation
);

} // namespace scene_ui