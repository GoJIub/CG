#include "scene.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace scene {

constexpr float minimum_rotation_axis_length = 0.0001f;

void update_animation_state(
    const SceneSettings& settings,
    AnimationState& animation,
    double time
) {
    if (!animation.started) {
		animation.previous_time = time;
		animation.started = true;
	}

	const float delta_time =
		static_cast<float>(time - animation.previous_time);

	animation.previous_time = time;

	if (animation.is_playing) {
		animation.trajectory_phase +=
			delta_time *
			settings.trajectory_speed;

		animation.rotation_angle +=
			delta_time *
			glm::radians(settings.rotation_speed_degrees);
	}
}

GlobalUniforms compute_global_uniforms(
    SceneSettings& settings,
    const AnimationState& animation,
    float aspect_ratio
) {
    scene::GlobalUniforms uniforms{};

	if (glm::length(settings.rotation_axis) <
		minimum_rotation_axis_length) {
		settings.rotation_axis = glm::vec3(0.0f, 1.0f, 0.0f);
	}

	const glm::vec3 rotation_axis =
		glm::normalize(settings.rotation_axis);

	const float trajectory_phase =
		animation.trajectory_phase;

	const glm::vec3 trajectory_offset{
		settings.trajectory_radius *
			glm::sin(trajectory_phase),

		settings.trajectory_height *
			glm::sin(2.0f * trajectory_phase),

		settings.trajectory_radius *
			glm::cos(trajectory_phase)
	};

	const glm::vec3 object_position =
		settings.position +
		trajectory_offset;

	glm::mat4 model(1.0f);

	model = glm::translate(model, object_position);
	model = glm::rotate(model, animation.rotation_angle, rotation_axis);
	model = glm::scale(model, settings.scale);

	uniforms.model = model;

	uniforms.view = glm::lookAt(
		settings.camera_position,
		settings.camera_target,
		settings.camera_up
	);

	if (settings.use_perspective_projection) {
		uniforms.projection = glm::perspective(
			glm::radians(settings.field_of_view_degrees),
			aspect_ratio,
			settings.near_plane,
			settings.far_plane
		);
	} else {
		const float half_height =
			settings.orthographic_half_height;

		const float half_width =
			aspect_ratio * half_height;

		uniforms.projection = glm::ortho(
			-half_width,
			half_width,
			-half_height,
			half_height,
			settings.near_plane,
			settings.far_plane
		);
	}

	// У GLM и Vulkan различается направление оси Y экрана.
	uniforms.projection[1][1] *= -1.0f;

	uniforms.object_color = settings.object_color;
	return uniforms;
}

} // namespace scene