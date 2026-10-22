#pragma once

#include "scene/scene.hpp"

namespace scene_ui {

void configure();
void apply_pending_scale();
void draw_scene_controls(
    scene::SceneState& state
);

} // namespace scene_ui