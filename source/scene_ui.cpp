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
	// На случай, если старое значение уже было установлено.
	ImGui::GetIO().FontGlobalScale = 1.0f;

	ImGui::GetStyle() = unscaled_imgui_style;
	ImGui::GetStyle().FontScaleMain = scale;
	ImGui::GetStyle().ScaleAllSizes(scale);
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
    scene::SceneSettings& settings,
    scene::AnimationState& animation
) {
	ImGui::Begin("Scene controls");

	ImGui::SliderFloat(
		"Rotation speed",
		&settings.rotation_speed_degrees,
		-180.0f,
		180.0f,
		"%.1f deg/s"
	);

	ImGui::SliderFloat3(
		"Rotation axis",
		&settings.rotation_axis.x,
		-1.0f,
		1.0f
	);

	ImGui::Checkbox(
		"Perspective projection",
		&settings.use_perspective_projection
	);

	if (settings.use_perspective_projection) {
		ImGui::SliderFloat(
			"Field of view",
			&settings.field_of_view_degrees,
			20.0f,
			120.0f,
			"%.1f deg"
		);
	} else {
		ImGui::SliderFloat(
			"Orthographic half height",
			&settings.orthographic_half_height,
			0.5f,
			10.0f,
			"%.2f"
		);
	}

	ImGui::DragFloat3(
		"Position",
		&settings.position.x,
		0.01f,
		-3.0f,
		3.0f,
		"%.2f"
	);

	ImGui::DragFloat3(
		"Scale",
		&settings.scale.x,
		0.01f,
		0.1f,
		5.0f,
		"%.2f"
	);

	ImGui::ColorEdit3(
		"Object color",
		&settings.object_color.x
	);

	ImGui::SliderFloat(
		"Trajectory radius",
		&settings.trajectory_radius,
		0.0f,
		2.0f,
		"%.2f"
	);

	ImGui::SliderFloat(
		"Trajectory height",
		&settings.trajectory_height,
		0.0f,
		2.0f,
		"%.2f"
	);

	ImGui::SliderFloat(
		"Trajectory speed",
		&settings.trajectory_speed,
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
		settings = scene::SceneSettings{};
		animation = scene::AnimationState{};
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