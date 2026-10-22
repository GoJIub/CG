#include "scene.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace scene {

constexpr float minimum_rotation_axis_length = 0.0001f;

SceneState::SceneState() {
	const glm::vec3 forward = glm::normalize(-camera.position);

	camera.rotation_degrees.x =
		glm::degrees(glm::asin(forward.y));

	camera.rotation_degrees.y =
		glm::degrees(glm::atan(-forward.x, -forward.z));

    constexpr float half_spacing = 1.8f;
    objects[0].settings.position.x = -half_spacing;
    objects[1].settings.position.x =  half_spacing;
	objects[1].animation.trajectory_phase = glm::pi<float>();

	point_lights.emplace_back();
	spot_lights.emplace_back();
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

glm::mat4 compute_camera_world_matrix(
    const CameraSettings& camera
) {
    const glm::vec3 angles =
        glm::radians(camera.rotation_degrees);

    glm::mat4 world = glm::translate(
        glm::mat4(1.0f),
        camera.position
    );

    world = glm::rotate(
        world,
        angles.y,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    world = glm::rotate(
        world,
        angles.x,
        glm::vec3(1.0f, 0.0f, 0.0f)
    );

    world = glm::rotate(
        world,
        angles.z,
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    return world;
}

SceneUniformData compute_scene_uniforms(
    const SceneState& state,
    float aspect_ratio
) {
	const auto& camera = state.camera;
    SceneUniformData uniforms{};

    const glm::mat4 view = glm::inverse(
		compute_camera_world_matrix(camera)
	);

    glm::mat4 projection{1.0f};

    if (camera.use_perspective_projection) {
		projection = glm::perspective(
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

		projection = glm::ortho(
			-half_width,
			half_width,
			-half_height,
			half_height,
			camera.near_plane,
			camera.far_plane
		);
	}

    projection[1][1] *= -1.0f;

    uniforms.view_projection = projection * view;
    uniforms.camera_position = glm::vec4(state.camera.position, 1.0f);

	const auto& light = state.directional_light;

	constexpr float minimum_light_direction_length = 0.0001f;

	glm::vec3 direction = light.direction;

	if (glm::length(direction) < minimum_light_direction_length) {
		direction = glm::vec3(0.0f, -1.0f, 0.0f);
	}

	uniforms.directional_light_direction =
		glm::vec4(glm::normalize(direction), 0.0f);

	uniforms.directional_light_color =
		glm::vec4(light.color * light.intensity, 0.0f);

	uniforms.ambient_light =
		glm::vec4(state.ambient_light, 0.0f);

	uniforms.light_counts.x =
		static_cast<std::uint32_t>(state.point_lights.size());

	uniforms.light_counts.y =
		static_cast<std::uint32_t>(state.spot_lights.size());

    return uniforms;
}

ObjectUniformData compute_object_uniforms(
    ObjectSettings& object,
    const AnimationState& animation,
    TrajectoryCenterMode center_mode
) {
    ObjectUniformData uniforms{};

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

    uniforms.albedo =
		glm::vec4(object.material.albedo, 1.0f);

	uniforms.specular_shininess = glm::vec4(
		object.material.specular_color,
		object.material.shininess
	);

    return uniforms;
}

PointLightData compute_point_light_data(
    const PointLightSettings& light
) {
    PointLightData data{};

    data.position = glm::vec4(light.position, 1.0f);
    data.color = glm::vec4(
        light.color * light.intensity,
        0.0f
    );

    return data;
}

SpotLightData compute_spot_light_data(
    const SpotLightSettings& light
) {
    SpotLightData data{};

    glm::vec3 direction = light.direction;

    constexpr float minimum_direction_length = 0.0001f;

    if (glm::length(direction) < minimum_direction_length) {
        direction = glm::vec3(0.0f, -1.0f, 0.0f);
    }

    const float inner_angle = glm::clamp(
        light.inner_angle_degrees,
        0.0f,
        89.0f
    );

    const float outer_angle = glm::clamp(
        light.outer_angle_degrees,
        inner_angle,
        89.0f
    );

    data.position = glm::vec4(light.position, 1.0f);
    data.direction = glm::vec4(glm::normalize(direction), 0.0f);
    data.color = glm::vec4(light.color * light.intensity, 0.0f);

    data.cone_cosines = glm::vec4(
        glm::cos(glm::radians(inner_angle)),
        glm::cos(glm::radians(outer_angle)),
        0.0f,
        0.0f
    );

    return data;
}

glm::mat4 compute_spot_light_cone_model(
    const SpotLightData& light,
    float cos_half_angle,
    float cone_length
) {
    const glm::vec3 forward = glm::vec3(light.direction);

    constexpr float parallel_threshold = 0.99f;

    const glm::vec3 helper =
        glm::abs(forward.y) < parallel_threshold
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);

    const glm::vec3 right =
        glm::normalize(glm::cross(helper, forward));

    const glm::vec3 up =
        glm::cross(forward, right);

    glm::mat4 rotation(1.0f);
    rotation[0] = glm::vec4(right, 0.0f);
    rotation[1] = glm::vec4(up, 0.0f);
    rotation[2] = glm::vec4(forward, 0.0f);

    const float half_angle = glm::acos(
        glm::clamp(cos_half_angle, -1.0f, 1.0f)
    );

    const float radius =
        cone_length * glm::tan(half_angle);

    const glm::mat4 translation = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(light.position)
    );

    const glm::mat4 scaling = glm::scale(
        glm::mat4(1.0f),
        glm::vec3(radius, radius, cone_length)
    );

    return translation * rotation * scaling;
}

} // namespace scene
