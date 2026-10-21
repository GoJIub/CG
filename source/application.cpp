#include "application.hpp"
#include "scene_renderer.hpp"
#include "scene_ui.hpp"
#include "scene.hpp"

namespace {

scene::SceneState scene_state;
scene::FrameUniformData frame_uniforms{};

}

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

void shutdown() {
    scene_renderer::shutdown();
}

bool update(double time) {
	scene_ui::draw_scene_controls(scene_state);

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

	if (!scene_renderer::update_uniform_buffers(uniforms)) {
		return false;
	}

	return true;
}

void render(const graphics::internal::FrameData& fd) {
    scene_renderer::render(fd);
}

} // namespace application
