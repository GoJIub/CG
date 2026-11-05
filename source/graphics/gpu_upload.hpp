#pragma once

#include <cstdint>
#include <functional>

#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

namespace gpu_upload {

using RecordCommands =
    std::function<void(VkCommandBuffer, VkBuffer)>;

bool upload_staged(
    VkDevice device,
    VmaAllocator allocator,
    VkQueue queue,
    std::uint32_t queue_family_index,
    VkDeviceSize size,
    const void* source_data,
    const RecordCommands& record_commands
);

bool create_geometry_buffer(
    VkDevice device,
    VmaAllocator allocator,
    VkQueue queue,
    std::uint32_t queue_family_index,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    const void* source_data,
    VkBuffer& out_buffer,
    VmaAllocation& out_allocation
);

} // namespace gpu_upload