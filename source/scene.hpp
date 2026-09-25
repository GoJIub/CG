#pragma once

#include <glm/glm.hpp>

namespace scene {

struct GlobalUniforms {
    glm::mat4 model{ 1.0f };
    glm::mat4 view{ 1.0f };
    glm::mat4 projection{ 1.0f };
	glm::vec4 object_color{ 1.0f };
};

static_assert(
    sizeof(scene::GlobalUniforms) ==
    sizeof(glm::mat4) * 3 +
    sizeof(glm::vec4)
);

struct SceneSettings {
	float rotation_speed_degrees = 35.0f;
	glm::vec3 rotation_axis{ 0.0f, 1.0f, 0.0f };

	glm::vec3 camera_position{ 2.7f, 2.0f, 3.2f };
	glm::vec3 camera_target{ 0.0f, 0.0f, 0.0f };
	glm::vec3 camera_up{ 0.0f, 1.0f, 0.0f };

	float field_of_view_degrees = 45.0f;
	float near_plane = 0.1f;
	float far_plane = 100.0f;

	bool use_perspective_projection = true;
	float orthographic_half_height = 2.2f;

	glm::vec3 position{ 0.0f, 0.0f, 0.0f };
	glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

	float trajectory_radius = 0.8f;
	float trajectory_height = 0.35f;
	float trajectory_speed = 1.0f;

	glm::vec4 object_color{ 1.0f, 1.0f, 1.0f, 1.0f };
};

struct AnimationState {
	double previous_time = 0.0;
	float rotation_angle = 0.0f;
	bool started = false;

	float trajectory_phase = 0.0f;
	bool is_playing = true;
};

void update_animation_state(
    const SceneSettings& settings,
    AnimationState& animation,
    double time
);

GlobalUniforms compute_global_uniforms(
    SceneSettings& settings,
    const AnimationState& animation,
    float aspect_ratio
);

} // namespace scene