#pragma once

#define WASM_EXPORT __attribute__((visibility("default")))

#define ARRAY_COUNT(array) (sizeof((array)) / sizeof((array)[0]))

#define MEMBER_SIZE(type, member) sizeof(((type*)0)->member)

#define BITS_SET(variable, bits) ((variable & (bits)) == (bits))

#define MIN(a, b) ({      \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a < _b ? _a : _b;    \
})

#define MAX(a, b) ({      \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a > _b ? _a : _b;    \
})

#define CLAMP(x, low, high) MIN(MAX(x, low), high)

#define ALIGN_FORWARD(value, alignment) ((alignment) > 0) ? ((value) + (alignment) - 1) & ~((alignment) - 1) : (value)

#define GROUP_COUNT(threadCount, localSize) ({ \
	__auto_type _threadCount = (threadCount);      \
	__auto_type _localSize   = (localSize);        \
	(threadCount + localSize - 1) / localSize;     \
})

#define BEFORE_INSTANCE_FUNCS(F)              \
	F(vkEnumerateInstanceExtensionProperties) \
	F(vkEnumerateInstanceVersion)             \
	F(vkCreateInstance)

#define INSTANCE_FUNCS(F)                         \
	F(vkCreateDevice)                             \
	F(vkCreateInstance)                           \
	F(vkCreateWin32SurfaceKHR)                    \
	F(vkEnumerateDeviceExtensionProperties)       \
	F(vkEnumeratePhysicalDevices)                 \
	F(vkGetDeviceProcAddr)                        \
	F(vkGetPhysicalDeviceFeatures)                \
	F(vkGetPhysicalDeviceFeatures2)               \
	F(vkGetPhysicalDeviceFeatures2KHR)            \
	F(vkGetPhysicalDeviceFormatProperties)        \
	F(vkGetPhysicalDeviceMemoryProperties)        \
	F(vkGetPhysicalDeviceProperties)              \
	F(vkGetPhysicalDeviceProperties2)             \
	F(vkGetPhysicalDeviceProperties2KHR)          \
	F(vkGetPhysicalDeviceQueueFamilyProperties)   \
	F(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)  \
	F(vkGetPhysicalDeviceSurfaceCapabilities2KHR) \
	F(vkGetPhysicalDeviceSurfaceFormatsKHR)       \
	F(vkGetPhysicalDeviceSurfaceSupportKHR)       \
	F(vkGetPhysicalDeviceFeatures2KHR)            \
	F(vkGetPhysicalDeviceFeatures2KHR)

#define DEVICE_FUNCS(f)                    \
	F(vkAcquireNextImageKHR)               \
	F(vkAcquireFullScreenExclusiveModeEXT) \
	F(vkAllocateCommandBuffers)            \
	F(vkAllocateDescriptorSets)            \
	F(vkAllocateMemory)                    \
	F(vkBeginCommandBuffer)                \
	F(vkBindBufferMemory)                  \
	F(vkBindImageMemory)                   \
	F(vkCmdBeginRenderPass)                \
	F(vkCmdBindDescriptorSets)             \
	F(vkCmdBindIndexBuffer)                \
	F(vkCmdBindPipeline)                   \
	F(vkCmdBindVertexBuffers)              \
	F(vkCmdCopyBuffer)                     \
	F(vkCmdCopyBufferToImage)              \
	F(vkCmdDispatch)                       \
	F(vkCmdDraw)                           \
	F(vkCmdDrawIndexed)                    \
	F(vkCmdDrawIndexedIndirect)            \
	F(vkCmdEndRenderPass)                  \
	F(vkCmdPipelineBarrier)                \
	F(vkCmdPushConstants)                  \
	F(vkCmdSetDepthWriteEnable)            \
	F(vkCmdSetDepthWriteEnableEXT)         \
	F(vkCmdSetScissor)                     \
	F(vkCmdSetStencilReference)            \
	F(vkCmdSetStencilTestEnable)           \
	F(vkCmdSetStencilTestEnableEXT)        \
	F(vkCmdSetViewport)                    \
	F(vkCreateBuffer)                      \
	F(vkCreateCommandPool)                 \
	F(vkCreateComputePipelines)            \
	F(vkCreateDescriptorPool)              \
	F(vkCreateDescriptorSetLayout)         \
	F(vkCreateFence)                       \
	F(vkCreateFramebuffer)                 \
	F(vkCreateGraphicsPipelines)           \
	F(vkCreateImage)                       \
	F(vkCreateImageView)                   \
	F(vkCreatePipelineLayout)              \
	F(vkCreateRenderPass)                  \
	F(vkCreateSampler)                     \
	F(vkCreateSemaphore)                   \
	F(vkCreateShaderModule)                \
	F(vkCreateSwapchainKHR)                \
	F(vkDestroyBuffer)                     \
	F(vkDestroyDescriptorSetLayout)        \
	F(vkDestroyFramebuffer)                \
	F(vkDestroyImage)                      \
	F(vkDestroyImageView)                  \
	F(vkDestroyShaderModule)               \
	F(vkDestroySwapchainKHR)               \
	F(vkDeviceWaitIdle)                    \
	F(vkEndCommandBuffer)                  \
	F(vkFlushMappedMemoryRanges)           \
	F(vkFreeMemory)                        \
	F(vkGetBufferMemoryRequirements)       \
	F(vkGetBufferMemoryRequirements2)      \
	F(vkGetBufferMemoryRequirements2KHR)   \
	F(vkGetDeviceQueue)                    \
	F(vkGetImageMemoryRequirements)        \
	F(vkGetImageMemoryRequirements2)       \
	F(vkGetImageMemoryRequirements2KHR)    \
	F(vkGetSwapchainImagesKHR)             \
	F(vkMapMemory)                         \
	F(vkQueuePresentKHR)                   \
	F(vkQueueSubmit)                       \
	F(vkQueueWaitIdle)                     \
	F(vkReleaseFullScreenExclusiveModeEXT) \
	F(vkResetCommandPool)                  \
	F(vkResetFences)                       \
	F(vkUpdateDescriptorSets)              \
	F(vkWaitForFences)