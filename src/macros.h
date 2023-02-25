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

#define STR2(x) #x
#define STR(x) STR2(x)

#ifdef _WIN32
#define INCBIN_SECTION ".rdata, \"dr\""
#else
#define INCBIN_SECTION ".rodata"
#endif

// this aligns start address to 16 and terminates byte array with explict 0
// which is not really needed, feel free to change it to whatever you want/need
#define INCBIN(name, file) \
    __asm__(".section " INCBIN_SECTION "\n" \
            ".global incbin_" STR(name) "_start\n" \
            ".balign 16\n" \
            "incbin_" STR(name) "_start:\n" \
            ".incbin \"" file "\"\n" \
            \
            ".global incbin_" STR(name) "_end\n" \
            ".balign 1\n" \
            "incbin_" STR(name) "_end:\n" \
            ".byte 0\n" \
    ); \
    extern __attribute__((aligned(16))) const char incbin_ ## name ## _start[]; \
    extern                              const char incbin_ ## name ## _end[]

#define GROUP_COUNT(threadCount, localSize) ({ \
	__auto_type _threadCount = (threadCount);      \
	__auto_type _localSize   = (localSize);        \
	(threadCount + localSize - 1) / localSize;     \
})

#define BEFORE_INSTANCE_FUNCS(F)              \
	F(vkEnumerateInstanceVersion)             \
	F(vkCreateInstance)

#define INSTANCE_FUNCS(F)                         \
	F(vkCreateDevice)                             \
	F(vkCreateInstance)                           \
	F(vkCreateWin32SurfaceKHR)                    \
	F(vkEnumeratePhysicalDevices)                 \
	F(vkGetDeviceProcAddr)                        \
	F(vkGetPhysicalDeviceFeatures)                \
	F(vkGetPhysicalDeviceFormatProperties)        \
	F(vkGetPhysicalDeviceMemoryProperties)        \
	F(vkGetPhysicalDeviceProperties)              \
	F(vkGetPhysicalDeviceQueueFamilyProperties)   \
	F(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)  \
	F(vkGetPhysicalDeviceSurfaceFormatsKHR)       \
	F(vkGetPhysicalDeviceSurfaceSupportKHR)

#define DEVICE_FUNCS(f)                    \
	F(vkAcquireNextImageKHR)               \
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
	F(vkCmdEndRenderPass)                  \
	F(vkCmdPipelineBarrier)                \
	F(vkCmdPushConstants)                  \
	F(vkCmdSetScissor)                     \
	F(vkCmdSetStencilReference)            \
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
	F(vkDestroyFramebuffer)                \
	F(vkDestroyImage)                      \
	F(vkDestroyImageView)                  \
	F(vkDestroySwapchainKHR)               \
	F(vkDeviceWaitIdle)                    \
	F(vkEndCommandBuffer)                  \
	F(vkFreeMemory)                        \
	F(vkGetBufferMemoryRequirements)       \
	F(vkGetDeviceQueue)                    \
	F(vkGetImageMemoryRequirements)        \
	F(vkGetSwapchainImagesKHR)             \
	F(vkMapMemory)                         \
	F(vkQueuePresentKHR)                   \
	F(vkQueueSubmit)                       \
	F(vkQueueWaitIdle)                     \
	F(vkResetCommandPool)                  \
	F(vkResetFences)                       \
	F(vkUpdateDescriptorSets)              \
	F(vkWaitForFences)