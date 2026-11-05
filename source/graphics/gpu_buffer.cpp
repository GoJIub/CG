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

bool create_device_buffer(
    VmaAllocator allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkBuffer& out_buffer,
    VmaAllocation& out_allocation
) {
    if (size == 0 ||
        out_buffer != VK_NULL_HANDLE ||
        out_allocation != nullptr) {
        std::cerr << "Invalid device buffer creation arguments\n";
        return false;
    }

    const VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    const VmaAllocationCreateInfo allocation_info{
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    VkBuffer new_buffer = VK_NULL_HANDLE;
    VmaAllocation new_allocation = nullptr;

    const VkResult result = vmaCreateBuffer(
        allocator,
        &buffer_info,
        &allocation_info,
        &new_buffer,
        &new_allocation,
        nullptr
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create device buffer: "
                  << result << '\n';
        return false;
    }

    out_buffer = new_buffer;
    out_allocation = new_allocation;

    return true;
}

void record_geometry_upload(
    VkCommandBuffer command_buffer,
    VkBuffer staging_buffer,
    VkBuffer destination_buffer,
    VkDeviceSize size
) {
    const VkBufferCopy copy_region{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };

    vkCmdCopyBuffer(
        command_buffer,
        staging_buffer,
        destination_buffer,
        1,
        &copy_region
    );

    const VkBufferMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask =
            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
            VK_ACCESS_INDEX_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = destination_buffer,
        .offset = 0,
        .size = size
    };

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );
}

} // namespace gpu_buffer