#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace scene {

inline constexpr std::size_t object_count = 2;

struct CameraSettings {
	glm::vec3 position{ 2.7f, 2.0f, 3.2f };
	glm::vec3 rotation_degrees{0.0f};

	float field_of_view_degrees = 45.0f;
	float near_plane = 0.1f;
	float far_plane = 100.0f;

	bool use_perspective_projection = true;
	float orthographic_half_height = 2.2f;
};

struct MaterialSettings {
    glm::vec3 albedo{1.0f};
    glm::vec3 specular_color{0.5f};
    float shininess = 32.0f;
};

struct ObjectSettings {
	float rotation_speed_degrees = 35.0f;
	glm::vec3 rotation_axis{ 0.0f, 1.0f, 0.0f };

	glm::vec3 position{ 0.0f, 0.0f, 0.0f };
	glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

	float trajectory_radius = 1.2f;
	float trajectory_height = 0.35f;
	float trajectory_speed = 1.0f;

	MaterialSettings material;
};

struct AnimationState {
	double previous_time = 0.0;
	float rotation_angle = 0.0f;
	bool started = false;

	float trajectory_phase = 0.0f;
	bool is_playing = true;
};

struct ObjectState {
    ObjectSettings settings;
    AnimationState animation;
};

enum class TrajectoryCenterMode {
    ObjectPosition,
    WorldOrigin
};

struct DirectionalLightSettings {
    glm::vec3 direction{-1.0f, -1.0f, -1.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

struct PointLightSettings {
    glm::vec3 position{2.0f, 2.0f, 2.0f};
    glm::vec3 color{1.0f};
    float intensity = 3.0f;
};

struct SceneState {
    CameraSettings camera;
    std::array<ObjectState, object_count> objects;
	TrajectoryCenterMode center_mode = TrajectoryCenterMode::WorldOrigin;

	DirectionalLightSettings directional_light;
	glm::vec3 ambient_light{0.1f};

    std::vector<PointLightSettings> point_lights;

	SceneState();
};

void update_animation_state(
    const ObjectSettings& object,
    AnimationState& animation,
    double time
);

struct SceneUniformData {
    glm::mat4 view_projection{1.0f};
    glm::vec4 camera_position{0.0f, 0.0f, 0.0f, 1.0f};

    glm::vec4 directional_light_direction{0.0f};
    glm::vec4 directional_light_color{0.0f};
    glm::vec4 ambient_light{0.0f};

    glm::uvec4 light_counts{0u};
};

struct ObjectUniformData {
    glm::mat4 model{1.0f};
    glm::vec4 albedo{1.0f};
    glm::vec4 specular_shininess{0.0f};
};

struct PointLightData {
    glm::vec4 position{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 color{0.0f};
};

struct FrameUniformData {
    SceneUniformData scene;
    std::array<ObjectUniformData, object_count> objects;
    std::vector<PointLightData> point_lights;
};

static_assert(
    sizeof(PointLightData) == sizeof(glm::vec4) * 2
);

static_assert(
    offsetof(PointLightData, color) == sizeof(glm::vec4)
);

static_assert(
    sizeof(SceneUniformData) ==
    sizeof(glm::mat4) +
    sizeof(glm::vec4) * 4 +
    sizeof(glm::uvec4)
);

static_assert(
    sizeof(ObjectUniformData) ==
    sizeof(glm::mat4) + sizeof(glm::vec4) * 2
);

SceneUniformData compute_scene_uniforms(
    const SceneState& state,
    float aspect_ratio
);

ObjectUniformData compute_object_uniforms(
    ObjectSettings& object,
    const AnimationState& animation,
    TrajectoryCenterMode center_mode
);

PointLightData compute_point_light_data(
    const PointLightSettings& light
);

} // namespace scene