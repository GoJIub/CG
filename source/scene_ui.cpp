#include "scene_ui.hpp"

#include <imgui.h>

namespace {

constexpr float default_imgui_scale = 1.5f;

struct UiSettings {
	float scale = default_imgui_scale;
};

UiSettings ui_settings;
bool imgui_scale_pending = false;
ImGuiStyle unscaled_imgui_style{};

void apply_scale(float scale) {
	ImGui::GetIO().FontGlobalScale = 1.0f;

	ImGui::GetStyle() = unscaled_imgui_style;
	ImGui::GetStyle().FontScaleMain = scale;
	ImGui::GetStyle().ScaleAllSizes(scale);
}

void draw_lighting_controls(scene::SceneState& state) {
    if (!ImGui::CollapsingHeader(
        "Lighting",
        ImGuiTreeNodeFlags_DefaultOpen
    )) {
        return;
    }

    auto& light = state.directional_light;

    ImGui::SliderFloat3(
        "Light direction",
        &light.direction.x,
        -1.0f,
        1.0f,
        "%.2f"
    );

    ImGui::ColorEdit3(
        "Light color",
        &light.color.x
    );

    ImGui::SliderFloat(
        "Light intensity",
        &light.intensity,
        0.0f,
        3.0f,
        "%.2f"
    );

    ImGui::ColorEdit3(
        "Ambient light",
        &state.ambient_light.x
    );
}

void draw_camera_controls(scene::CameraSettings& camera) {
	if (!ImGui::CollapsingHeader(
        "Camera",
        ImGuiTreeNodeFlags_DefaultOpen
    )) {
        return;
    }

	ImGui::DragFloat3(
		"Camera position",
		&camera.position.x,
		0.05f
	);

	ImGui::SliderFloat(
		"Pitch",
		&camera.rotation_degrees.x,
		-89.0f,
		89.0f,
		"%.1f deg"
	);

	ImGui::SliderFloat(
		"Yaw",
		&camera.rotation_degrees.y,
		-180.0f,
		180.0f,
		"%.1f deg"
	);

	ImGui::SliderFloat(
		"Roll",
		&camera.rotation_degrees.z,
		-180.0f,
		180.0f,
		"%.1f deg"
	);

	ImGui::Checkbox(
		"Perspective projection",
		&camera.use_perspective_projection
	);

	if (camera.use_perspective_projection) {
		ImGui::SliderFloat(
			"Field of view",
			&camera.field_of_view_degrees,
			20.0f,
			120.0f,
			"%.1f deg"
		);
	} else {
		ImGui::SliderFloat(
			"Orthographic half height",
			&camera.orthographic_half_height,
			0.5f,
			10.0f,
			"%.2f"
		);
	}
}

} // namespace


namespace scene_ui {

void configure() {
	unscaled_imgui_style = ImGui::GetStyle();
	apply_scale(ui_settings.scale);
}

void apply_pending_scale() {
	if (!imgui_scale_pending) {
		return;
	}

	apply_scale(ui_settings.scale);
	imgui_scale_pending = false;
}

void draw_scene_controls(
    scene::SceneState& state
) {
	static int selected_object_number = 1;

	ImGui::Begin("Scene controls");

	bool orbiting_world_origin =
		state.center_mode == scene::TrajectoryCenterMode::WorldOrigin;

	if (ImGui::Checkbox(
		"Orbit around world origin",
		&orbiting_world_origin
	)) {
		state.center_mode = orbiting_world_origin
			? scene::TrajectoryCenterMode::WorldOrigin
			: scene::TrajectoryCenterMode::ObjectPosition;
	}

	draw_camera_controls(state.camera);
	draw_lighting_controls(state);

	ImGui::Separator();

	ImGui::SliderInt(
		"Object",
		&selected_object_number,
		1,
		static_cast<int>(scene::object_count)
	);

	auto& selected = state.objects[
		static_cast<std::size_t>(selected_object_number - 1)
	];
	auto& object = selected.settings;
	auto& animation = selected.animation;

	ImGui::SliderFloat(
		"Rotation speed",
		&object.rotation_speed_degrees,
		-180.0f,
		180.0f,
		"%.1f deg/s"
	);

	ImGui::SliderFloat3(
		"Rotation axis",
		&object.rotation_axis.x,
		-1.0f,
		1.0f
	);

	if (state.center_mode == scene::TrajectoryCenterMode::ObjectPosition) {
		ImGui::DragFloat3(
			"Own orbit center position",
			&object.position.x,
			0.01f,
			-3.0f,
			3.0f,
			"%.2f"
		);
	}

	ImGui::DragFloat3(
		"Scale",
		&object.scale.x,
		0.01f,
		0.1f,
		5.0f,
		"%.2f"
	);

	ImGui::ColorEdit3(
		"Albedo",
		&object.material.albedo.x
	);

	ImGui::ColorEdit3(
		"Specular color",
		&object.material.specular_color.x
	);

	ImGui::SliderFloat(
		"Shininess",
		&object.material.shininess,
		1.0f,
		256.0f,
		"%.0f"
	);

	ImGui::SliderFloat(
		"Trajectory radius",
		&object.trajectory_radius,
		0.0f,
		3.0f,
		"%.2f"
	);

	ImGui::SliderFloat(
		"Trajectory height",
		&object.trajectory_height,
		0.0f,
		2.0f,
		"%.2f"
	);

	ImGui::SliderFloat(
		"Trajectory speed",
		&object.trajectory_speed,
		0.0f,
		4.0f,
		"%.2f rad/s"
	);

	if (ImGui::Button(
		animation.is_playing
			? "Pause animation"
			: "Resume animation"
	)) {
		animation.is_playing = !animation.is_playing;
	}

	if (ImGui::Button("Reset scene")) {
		state = scene::SceneState{};
	}

	if (ImGui::SliderFloat(
		"Interface scale",
		&ui_settings.scale,
		1.0f,
		2.5f,
		"%.2fx"
	)) {
		imgui_scale_pending = true;
	}

	ImGui::End();
}

} // namespace scene_ui
