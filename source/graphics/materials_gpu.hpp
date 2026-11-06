#pragma once

#include "gpu_texture.hpp"
#include "scene/materials.hpp"

#include <array>

namespace graphics::internal {
struct Context;
}

namespace materials_gpu {

struct Material {
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    gpu_texture::Resources albedo_texture;
    gpu_texture::Resources specular_texture;
    gpu_texture::Resources emissive_texture;
};

struct Resources {
    std::array<Material, scene::material_count> materials{};
    gpu_texture::Resources black_texture;
};

bool load_textures(
    const graphics::internal::Context& context,
    Resources& resources
);

void destroy_textures(
    VkDevice device,
    VmaAllocator allocator,
    Resources& resources
);

bool create_descriptor_sets(
    VkDevice device,
    VkDescriptorPool pool,
    VkDescriptorSetLayout set_layout,
    VkBuffer uniform_buffer,
    Resources& resources
);

void update_uniform_buffer_descriptors(
    VkDevice device,
    VkBuffer uniform_buffer,
    const Resources& resources
);

} // namespace materials_gpu