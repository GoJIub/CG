#include "materials_gpu.hpp"

#include "graphics_internal.hpp"
#include "image_loader.hpp"

#include "scene/scene.hpp"

#include <iostream>
#include <iterator>

namespace materials_gpu {

namespace {

bool load_texture(
    const graphics::internal::Context& context,
    const char* path,
    VkFormat format,
    gpu_texture::Resources& texture
) {
    image_loader::ImageData image;

    if (!image_loader::load_rgba8(path, image)) {
        std::cerr << "Failed to load texture: " << path << '\n';
        return false;
    }

	VkFormatProperties format_properties{};
    vkGetPhysicalDeviceFormatProperties(
        context.physical_device,
        format,
        &format_properties
    );

    constexpr VkFormatFeatureFlags required_features =
        VK_FORMAT_FEATURE_BLIT_SRC_BIT |
        VK_FORMAT_FEATURE_BLIT_DST_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

    if ((format_properties.optimalTilingFeatures &
         required_features) != required_features) {
        std::cerr
            << "Texture format does not support linear mipmap blits: "
            << format << '\n';
        return false;
    }

    if (!gpu_texture::create_image(
        context.allocator,
        image.width,
        image.height,
        format,
        texture
    )) {
        return false;
    }

    if (!gpu_texture::upload_rgba8(
        context.device,
        context.allocator,
        context.graphics_queue,
        context.graphics_queue_index,
        texture.image,
        image.width,
        image.height,
		texture.mip_levels,
        image.pixels.data()
    )) {
        return false;
    }

    if (!gpu_texture::create_view(
        context.device,
        format,
        texture
    )) {
        return false;
    }

    return gpu_texture::create_sampler(
        context.device,
        context.sampler_anisotropy,
        texture
    );
}

bool create_black_texture(
    const graphics::internal::Context& context,
    gpu_texture::Resources& texture
) {
    constexpr std::array<std::uint8_t, 4> pixel{
        0, 0, 0, 255
    };

    constexpr VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    if (!gpu_texture::create_image(
        context.allocator,
        1,
        1,
        format,
        texture
    )) {
        return false;
    }

    if (!gpu_texture::upload_rgba8(
        context.device,
        context.allocator,
        context.graphics_queue,
        context.graphics_queue_index,
        texture.image,
        1,
        1,
        texture.mip_levels,
        pixel.data()
    )) {
        return false;
    }

    if (!gpu_texture::create_view(
        context.device,
        format,
        texture
    )) {
        return false;
    }

    return gpu_texture::create_sampler(
        context.device,
        context.sampler_anisotropy,
        texture
    );
}

} // namespace

bool load_textures(
    const graphics::internal::Context& context,
    Resources& resources
) {
    if (!create_black_texture(
        context,
        resources.black_texture
    )) {
        return false;
    }

    for (std::size_t i = 0;
         i < scene::material_descriptions.size();
         ++i) {
        const auto& description = scene::material_descriptions[i];
        auto& material = resources.materials[i];

        if (!load_texture(
            context,
            description.albedo_path,
            VK_FORMAT_R8G8B8A8_SRGB,
            material.albedo_texture
        )) {
            return false;
        }

        if (description.specular_path != nullptr &&
            !load_texture(
                context,
                description.specular_path,
                VK_FORMAT_R8G8B8A8_UNORM,
                material.specular_texture
            )) {
            return false;
        }

        if (description.emissive_path != nullptr &&
            !load_texture(
                context,
                description.emissive_path,
                VK_FORMAT_R8G8B8A8_SRGB,
                material.emissive_texture
            )) {
            return false;
        }
    }

    return true;
}

void destroy_textures(
    VkDevice device,
    VmaAllocator allocator,
    Resources& resources
) {
    for (auto& material : resources.materials) {
        gpu_texture::destroy(
            device,
            allocator,
            material.albedo_texture
        );

        gpu_texture::destroy(
            device,
            allocator,
            material.specular_texture
        );

        gpu_texture::destroy(
            device,
            allocator,
            material.emissive_texture
        );
    }

    gpu_texture::destroy(
        device,
        allocator,
        resources.black_texture
    );
}

bool create_descriptor_sets(
    VkDevice device,
    VkDescriptorPool pool,
    VkDescriptorSetLayout set_layout,
    VkBuffer uniform_buffer,
    Resources& resources
) {
    const VkDescriptorSetAllocateInfo allocate_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &set_layout,
	};

    for (auto& material : resources.materials) {
		if (vkAllocateDescriptorSets(
			device,
			&allocate_info,
			&material.descriptor_set
		) != VK_SUCCESS) {
			std::cerr << "Failed to allocate descriptor sets\n";
			return false;
		}

        const VkDescriptorBufferInfo uniform_buffer_info{
            .buffer = uniform_buffer,
            .offset = 0,
            .range = sizeof(scene::ObjectUniformData)
        };

		const auto& specular_texture =
            material.specular_texture.view != VK_NULL_HANDLE
                ? material.specular_texture
                : resources.black_texture;

        const auto& emissive_texture =
            material.emissive_texture.view != VK_NULL_HANDLE
                ? material.emissive_texture
                : resources.black_texture;

		const VkDescriptorImageInfo albedo_image_info{
            .sampler = material.albedo_texture.sampler,
            .imageView = material.albedo_texture.view,
            .imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        const VkDescriptorImageInfo specular_image_info{
            .sampler = specular_texture.sampler,
            .imageView = specular_texture.view,
            .imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        const VkDescriptorImageInfo emissive_image_info{
            .sampler = emissive_texture.sampler,
            .imageView = emissive_texture.view,
            .imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        const VkWriteDescriptorSet writes[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = material.descriptor_set,
                .dstBinding = 0,
                .descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .pBufferInfo = &uniform_buffer_info
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = material.descriptor_set,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType =
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &albedo_image_info
            },
			{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = material.descriptor_set,
                .dstBinding = 2,
                .descriptorCount = 1,
                .descriptorType =
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &specular_image_info
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = material.descriptor_set,
                .dstBinding = 3,
                .descriptorCount = 1,
                .descriptorType =
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &emissive_image_info
            }
        };

        vkUpdateDescriptorSets(
            device,
            static_cast<uint32_t>(std::size(writes)),
            writes,
            0,
            nullptr
        );
	}

    return true;
}

void update_uniform_buffer_descriptors(
    VkDevice device,
    VkBuffer uniform_buffer,
    const Resources& resources
) {
    const VkDescriptorBufferInfo buffer_info{
        .buffer = uniform_buffer,
        .offset = 0,
        .range = sizeof(scene::ObjectUniformData)
    };

    std::array<VkWriteDescriptorSet, scene::material_count> writes{};

    for (std::size_t i = 0; i < resources.materials.size(); ++i) {
        writes[i] = VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = resources.materials[i].descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType =
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .pBufferInfo = &buffer_info
        };
    }

    vkUpdateDescriptorSets(
        device,
        static_cast<std::uint32_t>(writes.size()),
        writes.data(),
        0,
        nullptr
    );
}

} // namespace materials_gpu