#include "application.hpp"
#include "scene_renderer.hpp"
#include "scene_ui.hpp"
#include "scene.hpp"

namespace {

scene::SceneSettings scene_settings;
scene::AnimationState animation_state;

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
	scene_ui::draw_scene_controls(scene_settings, animation_state);

	auto& context = graphics::internal::context;

	if (context.swapchain_extent.height == 0) {
		return;
	}

	scene::update_animation_state(
		scene_settings,
		animation_state,
		time
	);

	const float aspect_ratio =
		static_cast<float>(context.swapchain_extent.width) /
		static_cast<float>(context.swapchain_extent.height);

	const scene::GlobalUniforms uniforms = scene::compute_global_uniforms(
		scene_settings,
		animation_state,
		aspect_ratio
	);

	scene_renderer::update_uniform_buffer(uniforms);
}

void render(const graphics::internal::FrameData& fd) {
    scene_renderer::render(fd);
}

} // namespace application
