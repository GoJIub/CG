#include "gpu_buffer.hpp"

#include <cstring>
#include <iostream>

namespace gpu_buffer {

bool create_buffer(
    VmaAllocator allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    const void* source_data,
    VkBuffer& out_buffer,
    VmaAllocation& out_allocation
) {
	VkBufferCreateInfo buffer_info{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo alloc_info{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				 VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VmaAllocationInfo allocation_info{};
	VkResult result = vmaCreateBuffer(
		allocator,
		&buffer_info,
		&alloc_info,
		&out_buffer,
		&out_allocation,
		&allocation_info
	);

	if (result != VK_SUCCESS) {
		std::cerr << "Failed to create buffer\n";
		return false;
	}

	if (source_data != nullptr) {
		std::memcpy(allocation_info.pMappedData, source_data, static_cast<size_t>(size));

		const VkResult flush_result = vmaFlushAllocation(
            allocator,
            out_allocation,
            0,
            size
        );

        if (flush_result != VK_SUCCESS) {
            std::cerr << "Failed to flush initial buffer data\n";

            vmaDestroyBuffer(
                allocator,
                out_buffer,
                out_allocation
            );

            out_buffer = VK_NULL_HANDLE;
            out_allocation = nullptr;

            return false;
        }
	}

	return true;
}

bool write_buffer(
    VmaAllocator allocator,
    VmaAllocation allocation,
    const void* source,
    std::size_t size
) {
    void* mapped_data = nullptr;

    if (vmaMapMemory(
        allocator,
        allocation,
        &mapped_data
    ) != VK_SUCCESS) {
        std::cerr << "Failed to map buffer\n";
        return false;
    }

    std::memcpy(mapped_data, source, size);

    const VkResult result = vmaFlushAllocation(
        allocator,
        allocation,
        0,
        static_cast<VkDeviceSize>(size)
    );

    vmaUnmapMemory(allocator, allocation);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to flush buffer\n";
        return false;
    }

    return true;
}

} // namespace gpu_buffer