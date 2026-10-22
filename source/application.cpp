#include "application.hpp"
#include "scene_renderer.hpp"
#include "scene_ui.hpp"
#include "scene.hpp"
#include "camera_controller.hpp"

namespace {

scene::SceneState scene_state;
scene::FrameUniformData frame_uniforms{};
camera_controller::State camera_input_state;

} // namespace


namespace application {

void configure_imgui() {
    scene_ui::configure();
}

void apply_pending_imgui_scale() {
    scene_ui::apply_pending_scale();
}

bool initialize() {
    return scene_renderer::initialize();
}

void shutdown(GLFWwindow* window) {
    camera_controller::release(camera_input_state, window);
    scene_renderer::shutdown();
}

bool update(double time, GLFWwindow* window) {
    scene_ui::draw_scene_controls(scene_state);

    camera_controller::update(
        camera_input_state,
        window,
        scene_state.camera
    );

	auto& context = graphics::internal::context;

	if (context.swapchain_extent.height == 0) {
		return true;
	}

	for (std::size_t i = 0; i < scene_state.objects.size(); ++i) {
		scene::update_animation_state(
			scene_state.objects[i].settings,
			scene_state.objects[i].animation,
			time
		);
	}

	const float aspect_ratio =
		static_cast<float>(context.swapchain_extent.width) /
		static_cast<float>(context.swapchain_extent.height);

	auto& uniforms = frame_uniforms;

	uniforms.scene = scene::compute_scene_uniforms(
		scene_state,
		aspect_ratio
	);

	for (std::size_t i = 0; i < scene_state.objects.size(); ++i) {
		auto& object = scene_state.objects[i];

		uniforms.objects[i] = scene::compute_object_uniforms(
			object.settings,
			object.animation,
			scene_state.center_mode
		);
	}

	uniforms.point_lights.resize(scene_state.point_lights.size());

	for (std::size_t i = 0; i < scene_state.point_lights.size(); ++i) {
		uniforms.point_lights[i] = scene::compute_point_light_data(
			scene_state.point_lights[i]
		);
	}

	uniforms.spot_lights.resize(scene_state.spot_lights.size());

	for (std::size_t i = 0; i < scene_state.spot_lights.size(); ++i) {
		uniforms.spot_lights[i] = scene::compute_spot_light_data(
			scene_state.spot_lights[i]
		);
	}

	auto& markers = uniforms.light_markers;
	markers.clear();

	if (scene_state.show_point_light_gizmos) {
		for (const auto& light : scene_state.point_lights) {
			markers.push_back({
				light.position,
				light.color
			});
		}
	}

	if (scene_state.show_spot_light_gizmos) {
		for (const auto& light : scene_state.spot_lights) {
			markers.push_back({
				light.position,
				light.color
			});
		}
	}

	auto& cone_models = uniforms.spot_light_cone_models;
	cone_models.clear();

	if (scene_state.show_spot_light_gizmos) {
		cone_models.reserve(uniforms.spot_lights.size() * 2);

		for (const auto& light : uniforms.spot_lights) {
			cone_models.push_back(
				scene::compute_spot_light_cone_model(
					light,
					light.cone_cosines.y,
					scene_state.spot_light_cone_length
				)
			);

			if (light.cone_cosines.x > light.cone_cosines.y) {
				cone_models.push_back(
					scene::compute_spot_light_cone_model(
						light,
						light.cone_cosines.x,
						scene_state.spot_light_cone_length
					)
				);
			}
		}
	}

	if (!scene_renderer::update_uniform_buffers(uniforms)) {
		return false;
	}

	return true;
}

void render(const graphics::internal::FrameData& fd) {
    scene_renderer::render(fd, frame_uniforms);
}

} // namespace application
