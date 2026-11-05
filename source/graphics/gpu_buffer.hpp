#pragma once

#include <cstddef>

#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

namespace gpu_buffer {

bool create_buffer(
    VmaAllocator allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    const void* source_data,
    VkBuffer& out_buffer,
    VmaAllocation& out_allocation
);

bool write_buffer(
    VmaAllocator allocator,
    VmaAllocation allocation,
    const void* source,
    std::size_t size
);

bool create_device_buffer(
    VmaAllocator allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkBuffer& out_buffer,
    VmaAllocation& out_allocation
);

void record_geometry_upload(
    VkCommandBuffer command_buffer,
    VkBuffer staging_buffer,
    VkBuffer destination_buffer,
    VkDeviceSize size
);

} // namespace gpu_buffer