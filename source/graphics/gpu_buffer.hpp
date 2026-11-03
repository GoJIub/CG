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

} // namespace gpu_buffer