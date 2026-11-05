#pragma once

#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include <cstdint>

namespace gpu_texture {

struct Resources {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;

    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    std::uint32_t mip_levels = 1;
};

bool create_image(
    VmaAllocator allocator,
    std::uint32_t width,
    std::uint32_t height,
    VkFormat format,
    Resources& texture
);

void destroy(
    VkDevice device,
    VmaAllocator allocator,
    Resources& texture
);

void record_upload_rgba8(
    VkCommandBuffer command_buffer,
    VkBuffer staging_buffer,
    VkImage image,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t mip_levels
);

bool upload_rgba8(
    VkDevice device,
    VmaAllocator allocator,
    VkQueue queue,
    std::uint32_t queue_family_index,
    VkImage image,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t mip_levels,
    const void* pixels
);

bool create_view(
    VkDevice device,
    VkFormat format,
    Resources& texture
);

bool create_sampler(
    VkDevice device,
    float anisotropy,
    Resources& texture
);

} // namespace gpu_texture