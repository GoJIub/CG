#include "application.hpp"
#include "scene_renderer.hpp"
#include "scene_ui.hpp"
#include "scene.hpp"

namespace {

scene::SceneState scene_state;

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

void update(double time) {
	scene_ui::draw_scene_controls(scene_state);

	auto& context = graphics::internal::context;

	if (context.swapchain_extent.height == 0) {
		return;
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

	scene::FrameUniformData uniforms{};

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

	scene_renderer::update_uniform_buffers(uniforms);
}

void render(const graphics::internal::FrameData& fd) {
    scene_renderer::render(fd);
}

} // namespace application
