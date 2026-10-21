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
        "%.2f",
        ImGuiSliderFlags_AlwaysClamp
    );

    ImGui::ColorEdit3(
        "Light color",
        &light.color.x
    );

    ImGui::SliderFloat(
        "Directional intensity",
        &light.intensity,
        0.0f,
        3.0f,
        "%.2f",
        ImGuiSliderFlags_AlwaysClamp
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
		"%.1f deg",
		ImGuiSliderFlags_AlwaysClamp
	);

	ImGui::SliderFloat(
		"Yaw",
		&camera.rotation_degrees.y,
		-180.0f,
		180.0f,
		"%.1f deg",
		ImGuiSliderFlags_AlwaysClamp
	);

	ImGui::SliderFloat(
		"Roll",
		&camera.rotation_degrees.z,
		-180.0f,
		180.0f,
		"%.1f deg",
		ImGuiSliderFlags_AlwaysClamp
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
			"%.1f deg",
			ImGuiSliderFlags_AlwaysClamp
		);
	} else {
		ImGui::SliderFloat(
			"Orthographic half height",
			&camera.orthographic_half_height,
			0.5f,
			10.0f,
			"%.2f",
			ImGuiSliderFlags_AlwaysClamp
		);
	}
}

void draw_object_controls(scene::SceneState& state) {
	if (!ImGui::CollapsingHeader(
        "Objects",
        ImGuiTreeNodeFlags_DefaultOpen
    )) {
        return;
    }

    ImGui::PushID("Objects");

	static int selected_object_number = 1;

	ImGui::SliderInt(
		"Object",
		&selected_object_number,
		1,
		static_cast<int>(scene::object_count),
		"%d",
		ImGuiSliderFlags_AlwaysClamp
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
		"%.1f deg/s",
		ImGuiSliderFlags_AlwaysClamp
	);

	ImGui::SliderFloat3(
		"Rotation axis",
		&object.rotation_axis.x,
		-1.0f,
		1.0f,
		"%.3f",
		ImGuiSliderFlags_AlwaysClamp
	);

	if (state.center_mode == scene::TrajectoryCenterMode::ObjectPosition) {
		ImGui::DragFloat3(
			"Own orbit center position",
			&object.position.x,
			0.01f,
			-3.0f,
			3.0f,
			"%.2f",
			ImGuiSliderFlags_AlwaysClamp
		);
	}

	ImGui::DragFloat3(
		"Scale",
		&object.scale.x,
		0.01f,
		0.1f,
		5.0f,
		"%.2f",
		ImGuiSliderFlags_AlwaysClamp
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
		"%.0f",
		ImGuiSliderFlags_AlwaysClamp
	);

	ImGui::SliderFloat(
		"Trajectory radius",
		&object.trajectory_radius,
		0.0f,
		3.0f,
		"%.2f",
		ImGuiSliderFlags_AlwaysClamp
	);

	ImGui::SliderFloat(
		"Trajectory height",
		&object.trajectory_height,
		0.0f,
		2.0f,
		"%.2f",
		ImGuiSliderFlags_AlwaysClamp
	);

	ImGui::SliderFloat(
		"Trajectory speed",
		&object.trajectory_speed,
		0.0f,
		4.0f,
		"%.2f rad/s",
		ImGuiSliderFlags_AlwaysClamp
	);

	if (ImGui::Button(
		animation.is_playing
			? "Pause animation"
			: "Resume animation"
	)) {
		animation.is_playing = !animation.is_playing;
	}

	ImGui::PopID();
}

void draw_point_lights_controls(scene::SceneState& state) {
    if (!ImGui::CollapsingHeader(
        "Point lights",
        ImGuiTreeNodeFlags_DefaultOpen
    )) {
        return;
    }

    ImGui::PushID("PointLights");

    auto& lights = state.point_lights;

    if (ImGui::Button("Add point light")) {
        lights.emplace_back();
    }

    ImGui::Text("Count: %zu", lights.size());

    std::size_t remove_index = lights.size();

    for (std::size_t i = 0; i < lights.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));

        if (ImGui::TreeNodeEx(
            "Light",
            ImGuiTreeNodeFlags_DefaultOpen,
            "Point light %zu",
            i + 1
        )) {
            auto& light = lights[i];

            ImGui::DragFloat3(
				"Position",
				&light.position.x,
				0.05f
			);

			ImGui::ColorEdit3(
				"Color",
				&light.color.x
			);

			ImGui::SliderFloat(
				"Intensity",
				&light.intensity,
				0.0f,
				20.0f,
				"%.2f",
				ImGuiSliderFlags_AlwaysClamp
			);

            if (ImGui::Button("Remove")) {
                remove_index = i;
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    if (remove_index < lights.size()) {
        lights.erase(
            lights.begin() +
            static_cast<std::ptrdiff_t>(remove_index)
        );
    }

	ImGui::Checkbox(
		"Show visualization",
		&state.show_point_light_gizmos
	);

    ImGui::PopID();
}

void draw_spot_lights_controls(scene::SceneState& state) {
    if (!ImGui::CollapsingHeader(
        "Spot lights",
        ImGuiTreeNodeFlags_DefaultOpen
    )) {
        return;
    }

    ImGui::PushID("SpotLights");

    auto& lights = state.spot_lights;

    if (ImGui::Button("Add spot light")) {
        lights.emplace_back();
    }

    ImGui::Text("Count: %zu", lights.size());

    std::size_t remove_index = lights.size();

    for (std::size_t i = 0; i < lights.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));

        if (ImGui::TreeNodeEx(
            "Light",
            ImGuiTreeNodeFlags_DefaultOpen,
            "Spot light %zu",
            i + 1
        )) {
            auto& light = lights[i];

            ImGui::DragFloat3(
				"Position",
				&light.position.x,
				0.05f
			);

			ImGui::SliderFloat3(
				"Direction",
				&light.direction.x,
				-1.0f,
				1.0f,
				"%.2f",
				ImGuiSliderFlags_AlwaysClamp
			);
			if (glm::length(light.direction) < 0.0001f) {
				ImGui::TextDisabled(
					"Zero direction: using (0, -1, 0)"
				);
			}

			ImGui::ColorEdit3(
				"Color",
				&light.color.x
			);

			ImGui::SliderFloat(
				"Intensity",
				&light.intensity,
				0.0f,
				20.0f,
				"%.2f",
				ImGuiSliderFlags_AlwaysClamp
			);

			ImGui::SliderFloat(
				"Inner half-angle",
				&light.inner_angle_degrees,
				0.0f,
				light.outer_angle_degrees,
				"%.1f deg",
				ImGuiSliderFlags_AlwaysClamp
			);

			ImGui::SliderFloat(
				"Outer half-angle",
				&light.outer_angle_degrees,
				light.inner_angle_degrees,
				89.0f,
				"%.1f deg",
				ImGuiSliderFlags_AlwaysClamp
			);

            if (ImGui::Button("Remove")) {
                remove_index = i;
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    if (remove_index < lights.size()) {
        lights.erase(
            lights.begin() +
            static_cast<std::ptrdiff_t>(remove_index)
        );
    }

	ImGui::Checkbox(
		"Show visualization",
		&state.show_spot_light_gizmos
	);

	if (state.show_spot_light_gizmos) {
		ImGui::SliderFloat(
			"Cone display length",
			&state.spot_light_cone_length,
			0.1f,
			5.0f,
			"%.2f",
			ImGuiSliderFlags_AlwaysClamp
		);
	}

    ImGui::PopID();
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
	draw_object_controls(state);
	draw_point_lights_controls(state);
	draw_spot_lights_controls(state);

	ImGui::Separator();

	if (ImGui::Button("Reset scene")) {
		state = scene::SceneState{};
	}

	if (ImGui::SliderFloat(
		"Interface scale",
		&ui_settings.scale,
		1.0f,
		2.5f,
		"%.2fx",
		ImGuiSliderFlags_AlwaysClamp
	)) {
		imgui_scale_pending = true;
	}

	ImGui::End();
}

} // namespace scene_ui
