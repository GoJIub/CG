#include "scene.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace scene {

constexpr float minimum_rotation_axis_length = 0.0001f;

SceneState::SceneState() {
    constexpr float half_spacing = 1.8f;
    objects[0].settings.position.x = -half_spacing;
    objects[1].settings.position.x =  half_spacing;
	objects[1].animation.trajectory_phase = glm::pi<float>();
}

void update_animation_state(
    const ObjectSettings& object,
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
			object.trajectory_speed;

		animation.rotation_angle +=
			delta_time *
			glm::radians(object.rotation_speed_degrees);
	}
}

GlobalUniforms compute_global_uniforms(
    ObjectSettings& object,
    const CameraSettings& camera,
    const AnimationState& animation,
    TrajectoryCenterMode center_mode,
    float aspect_ratio
) {
    scene::GlobalUniforms uniforms{};

	if (glm::length(object.rotation_axis) <
		minimum_rotation_axis_length) {
		object.rotation_axis = glm::vec3(0.0f, 1.0f, 0.0f);
	}

	const glm::vec3 rotation_axis =
		glm::normalize(object.rotation_axis);

	const float trajectory_phase =
		animation.trajectory_phase;

	const glm::vec3 trajectory_offset{
		object.trajectory_radius *
			glm::sin(trajectory_phase),

		object.trajectory_height *
			glm::sin(2.0f * trajectory_phase),

		object.trajectory_radius *
			glm::cos(trajectory_phase)
	};

	const glm::vec3 center =
		center_mode == TrajectoryCenterMode::WorldOrigin
			? glm::vec3(0.0f)
			: object.position;

	const glm::vec3 object_position = center + trajectory_offset;

	glm::mat4 model(1.0f);

	model = glm::translate(model, object_position);
	model = glm::rotate(model, animation.rotation_angle, rotation_axis);
	model = glm::scale(model, object.scale);

	uniforms.model = model;

	uniforms.view = glm::lookAt(
		camera.position,
		camera.target,
		camera.up
	);

	if (camera.use_perspective_projection) {
		uniforms.projection = glm::perspective(
			glm::radians(camera.field_of_view_degrees),
			aspect_ratio,
			camera.near_plane,
			camera.far_plane
		);
	} else {
		const float half_height =
			camera.orthographic_half_height;

		const float half_width =
			aspect_ratio * half_height;

		uniforms.projection = glm::ortho(
			-half_width,
			half_width,
			-half_height,
			half_height,
			camera.near_plane,
			camera.far_plane
		);
	}

	// У GLM и Vulkan различается направление оси Y экрана.
	uniforms.projection[1][1] *= -1.0f;

	uniforms.object_color = object.object_color;
	return uniforms;
}

} // namespace scene