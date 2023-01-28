#include "macros.h"
#include "math.h"

#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "vcruntime.lib")
#pragma comment(lib, "ucrt.lib")
#pragma comment(lib, "ws2_32.lib")

#define TICKS_PER_SECOND 50
#define FRAMES_IN_FLIGHT 2

#define OEMRESOURCE
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#define VK_ENABLE_BETA_EXTENSIONS
#define WIN32_LEAN_AND_MEAN
#include <vulkan/vulkan.h>
#include <hidusage.h>
#include <stdio.h>
#include <stdbool.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define GET_GROUP_COUNT(threadCount, localSize) ({ \
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

typedef enum Input {
	INPUT_START_LEFT    = 0x0001,
	INPUT_START_FORWARD = 0x0002,
	INPUT_START_RIGHT   = 0x0004,
	INPUT_START_BACK    = 0x0008,
	INPUT_START_DOWN    = 0x0010,
	INPUT_START_UP      = 0x0020,
	INPUT_STOP_LEFT     = 0x0040,
	INPUT_STOP_FORWARD  = 0x0080,
	INPUT_STOP_RIGHT    = 0x0100,
	INPUT_STOP_BACK     = 0x0200,
	INPUT_STOP_DOWN     = 0x0400,
	INPUT_STOP_UP       = 0x0800,
	INPUT_LOCK_CURSOR   = 0x1000
} Input;

typedef enum GraphicsPipelines {
	GRAPHICS_PIPELINE_TRIANGLE,
	GRAPHICS_PIPELINE_MAX
} GraphicsPipelines;

typedef enum Buffers {
	BUFFER_INDICES,
	BUFFER_VERTICES,
	BUFFER_UNIFORM,
	BUFFER_MAX
} Buffers;

typedef struct KTX2 {
	char identifier[12];
	uint32_t vkFormat;
	uint32_t typeSize;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	uint32_t pixelDepth;
	uint32_t layerCount;
	uint32_t faceCount;
	uint32_t levelCount;
	uint32_t supercompressionScheme;

	uint32_t dfdByteOffset;
	uint32_t dfdByteLength;
	uint32_t kvdByteOffset;
	uint32_t kvdByteLength;
	uint64_t sgdByteOffset;
	uint64_t sgdByteLength;

	struct {
		uint64_t byteOffset;
		uint64_t byteLength;
		uint64_t uncompressedByteLength;
	} levels[];
} KTX2;

typedef struct VertexPosition {
	uint16_t x, y, z;
} VertexPosition;

typedef struct VertexAttribute {
	float nx, ny, nz;
	float tx, ty, tz, tw;
	float u, v;
} VertexAttribute;

typedef struct Primitive {
	uint32_t material;
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t vertexOffset;
	Vec3 min;
	Vec3 max;
} Primitive;

typedef struct Mesh {
	Primitive* primitives;
	float* weights;
	uint32_t primitiveCount;
	uint32_t weightsCount;
} Mesh;

typedef struct Entity {
	Vec3 translation;
	Quat rotation;
	Vec3 scale;

	Mesh* mesh;
	struct Entity** children;
	uint32_t childCount;
} Entity;

typedef struct Portal {
	Vec3 translation;
	Quat rotation;
	Vec3 scale;
	uint32_t link;
} Portal;

static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
#define F(fn) static PFN_##fn fn;
	BEFORE_INSTANCE_FUNCS(F)
	INSTANCE_FUNCS(F)
	DEVICE_FUNCS(F)
#undef F

static VkPhysicalDeviceVulkan13Properties physicalDeviceVulkan13Properties = {
	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES
};
static VkPhysicalDeviceVulkan12Properties physicalDeviceVulkan12Properties = {
	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
	.pNext = &physicalDeviceVulkan13Properties
};
static VkPhysicalDeviceVulkan11Properties physicalDeviceVulkan11Properties = {
	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
	.pNext = &physicalDeviceVulkan12Properties
};
static VkPhysicalDeviceProperties2 physicalDeviceProperties2 = {
	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
	.pNext = &physicalDeviceVulkan11Properties
};

static VkResult r;
static VkDevice device;
static VkPhysicalDevice physicalDevice;
static VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
static uint32_t queueFamilyIndex;
static uint32_t queueCount;
static VkRenderPass renderPass;
static VkSurfaceKHR surface;
static VkSurfaceFormatKHR surfaceFormat;
static VkSwapchainKHR swapchain;
static VkFramebuffer swapchainFramebuffers[8];
static VkRect2D scissor;
static VkViewport viewport;

static HMODULE hInstance;
static LPVOID mainFiber;

static uint64_t input;

static Entity player;
static Mat4 projectionMatrix;
static Vec3 cameraPosition;
static float cameraYaw;
static float cameraPitch;
static const float cameraFieldOfView = 0.78f;
static const float cameraNear        = 0.1f;

static char* vkResultToString(void) {
	switch (r) {
	#define CASE(r) case r: return #r
		CASE(VK_SUCCESS);
		CASE(VK_NOT_READY);
		CASE(VK_TIMEOUT);
		CASE(VK_EVENT_SET);
		CASE(VK_EVENT_RESET);
		CASE(VK_INCOMPLETE);
		CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
		CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
		CASE(VK_ERROR_INITIALIZATION_FAILED);
		CASE(VK_ERROR_DEVICE_LOST);
		CASE(VK_ERROR_MEMORY_MAP_FAILED);
		CASE(VK_ERROR_LAYER_NOT_PRESENT);
		CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
		CASE(VK_ERROR_FEATURE_NOT_PRESENT);
		CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
		CASE(VK_ERROR_TOO_MANY_OBJECTS);
		CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
		CASE(VK_ERROR_FRAGMENTED_POOL);
		CASE(VK_ERROR_UNKNOWN);
		CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
		CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
		CASE(VK_ERROR_FRAGMENTATION);
		CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
		CASE(VK_PIPELINE_COMPILE_REQUIRED);
		CASE(VK_ERROR_SURFACE_LOST_KHR);
		CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
		CASE(VK_SUBOPTIMAL_KHR);
		CASE(VK_ERROR_OUT_OF_DATE_KHR);
		CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
		CASE(VK_ERROR_VALIDATION_FAILED_EXT);
		CASE(VK_ERROR_INVALID_SHADER_NV);
		CASE(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
		CASE(VK_ERROR_NOT_PERMITTED_KHR);
		CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
		CASE(VK_THREAD_IDLE_KHR);
		CASE(VK_THREAD_DONE_KHR);
		CASE(VK_OPERATION_DEFERRED_KHR);
		CASE(VK_OPERATION_NOT_DEFERRED_KHR);
		CASE(VK_ERROR_COMPRESSION_EXHAUSTED_EXT);
	#undef CASE
		default: return "VK_ERROR_UNKNOWN";
	}
}

static void exitHRESULT(const char* function, HRESULT err) {
	char buffer[1024];
	char errorMessage[1024];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, errorMessage, sizeof(errorMessage), NULL);

	sprintf(buffer, "%s failed: %s", function, errorMessage);

	MessageBoxA(NULL, buffer, NULL, MB_OK);
	ExitProcess(0);
}

static void exitWin32(const char* function) {
	exitHRESULT(function, GetLastError());
}

static void exitWSA(const char* function) {
	exitHRESULT(function, WSAGetLastError());
}

static void exitVk(const char* function) {
	char buffer[512];
	sprintf(buffer, "%s failed: %s", function, vkResultToString());

	MessageBoxA(NULL, buffer, NULL, MB_OK);
	ExitProcess(0);
}

static uint32_t getMemoryTypeIndex(uint32_t memoryTypeBits, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags optionalFlags) {
	VkMemoryPropertyFlags combined = requiredFlags | optionalFlags;

	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if ((memoryTypeBits & (1 << i)) && ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & combined) == combined)) {
			return i;
		}
	}

	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if ((memoryTypeBits & (1 << i)) && ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags)) {
			return i;
		}
	}

	return 0;
}

static LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			if (!RegisterRawInputDevices(&(RAWINPUTDEVICE){
				.usUsagePage = HID_USAGE_PAGE_GENERIC,
				.usUsage     = HID_USAGE_GENERIC_MOUSE,
				.dwFlags     = 0,
				.hwndTarget	 = hWnd
			}, 1, sizeof(RAWINPUTDEVICE)))
				exitWin32("RegisterRawInputDevices");

			HMODULE vulkanModule  = LoadLibraryW(L"vulkan-1.dll");
			if (!vulkanModule)
				exitWin32("LoadLibraryW");

			vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkanModule, "vkGetInstanceProcAddr");

			#define F(fn) fn = (PFN_##fn)vkGetInstanceProcAddr(NULL, #fn);
			BEFORE_INSTANCE_FUNCS(F)
			#undef F

			VkApplicationInfo applicationInfo = {
				.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName   = "triangle",
				.applicationVersion = 1,
				.pEngineName        = "Extreme Engine",
				.engineVersion      = 1,
				.apiVersion         = VK_API_VERSION_1_0
			};

			if (vkEnumerateInstanceVersion) {
				if ((r = vkEnumerateInstanceVersion(&applicationInfo.apiVersion)) != VK_SUCCESS)
					exitVk("vkEnumerateInstanceVersion");
			}

			const char* enabledInstanceExtensions[5];
			const char* wantedInstanceExtensions[5] = {
				VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
				VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
				VK_KHR_SURFACE_EXTENSION_NAME,
				VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
			};
			uint32_t wantedInstanceExtensionsCount = 4;

			if (applicationInfo.apiVersion < VK_API_VERSION_1_1)
				wantedInstanceExtensions[wantedInstanceExtensionsCount++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;

			VkInstanceCreateInfo instanceCreateInfo = {
				.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pApplicationInfo        = &applicationInfo,
				// .enabledLayerCount       = 1,
				// .ppEnabledLayerNames     = (const char*[]){ "VK_LAYER_LUNARG_monitor" },
				.enabledExtensionCount   = 0,
				.ppEnabledExtensionNames = enabledInstanceExtensions
			};

			VkExtensionProperties instanceExtensionProperties[32];
			uint32_t instanceExtensionPropertiesCount = ARRAY_COUNT(instanceExtensionProperties);
			r = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionPropertiesCount, instanceExtensionProperties);
			if (r != VK_SUCCESS && r != VK_INCOMPLETE)
				exitVk("vkEnumerateInstanceExtensionProperties");

			for (uint32_t i = 0; i < wantedInstanceExtensionsCount; i++) {
				for (uint32_t j = 0; j < instanceExtensionPropertiesCount; j++) {
					if (strcmp(wantedInstanceExtensions[i], instanceExtensionProperties[j].extensionName) == 0) {
						enabledInstanceExtensions[instanceCreateInfo.enabledExtensionCount++] = wantedInstanceExtensions[i];
						break;
					}
				}
			}

			VkInstance instance;
			if ((r = vkCreateInstance(&instanceCreateInfo, NULL, &instance) != VK_SUCCESS))
				exitVk("vkCreateInstance");

			#define F(fn) fn = (PFN_##fn)vkGetInstanceProcAddr(instance, #fn);
			INSTANCE_FUNCS(F)
			#undef F

			if ((r = vkCreateWin32SurfaceKHR(instance, &(VkWin32SurfaceCreateInfoKHR){
				.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
				.hinstance = hInstance,
				.hwnd      = hWnd
			}, NULL, &surface)) != VK_SUCCESS)
				exitVk("vkCreateWin32SurfaceKHR");

			r = vkEnumeratePhysicalDevices(instance, &(uint32_t){ 1 }, &physicalDevice);
			if (r != VK_SUCCESS && r != VK_INCOMPLETE)
				exitVk("vkEnumeratePhysicalDevices");

			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

			VkSurfaceFormatKHR surfaceFormats[16];
			uint32_t surfaceFormatCount = ARRAY_COUNT(surfaceFormats);
			r = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats);
			if (r != VK_SUCCESS && r != VK_INCOMPLETE)
				exitVk("vkGetPhysicalDeviceSurfaceFormatsKHR");

			surfaceFormat = surfaceFormats[0];
			for (uint32_t j = 0; j < surfaceFormatCount; j++) {
				if (surfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && surfaceFormats[j].format == VK_FORMAT_B8G8R8A8_SRGB) {
					surfaceFormat = surfaceFormats[j];
					break;
				}
			}

			VkQueueFamilyProperties queueFamilyProperties[16];
			uint32_t queueFamilyPropertyCount = ARRAY_COUNT(queueFamilyProperties);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties);

			for (uint32_t i = 0; i < queueFamilyPropertyCount; i++) {
				if (BITS_SET(queueFamilyProperties[i].queueFlags, (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))) {
					VkBool32 presentSupport;
					if ((r = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport)) != VK_SUCCESS)
						exitVk("vkGetPhysicalDeviceSurfaceSupportKHR");

					if (presentSupport) {
						queueFamilyIndex = i;
						queueCount = MIN(2u, queueFamilyProperties[i].queueCount);
						break;
					}
				}
			}

			VkExtensionProperties deviceExtensionProperties[256];
			uint32_t deviceExtensionPropertiesCount = ARRAY_COUNT(deviceExtensionProperties);
			r = vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &deviceExtensionPropertiesCount, deviceExtensionProperties);
			if (r != VK_SUCCESS && r != VK_INCOMPLETE)
				exitVk("vkEnumerateDeviceExtensionProperties");

			VkPhysicalDeviceExtendedDynamicStateFeaturesEXT enabledPhysicalDeviceExtendedDynamicStateFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT
			};
			VkPhysicalDeviceVulkan13Features enabledPhysicalDevice13Features = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
			};
			VkPhysicalDeviceVulkan12Features enabledPhysicalDevice12Features = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
			};
			VkPhysicalDeviceVulkan11Features enabledPhysicalDevice11Features = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
			};
			VkPhysicalDeviceFeatures2 enabledPhysicalDeviceFeatures2 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
			};

			VkPhysicalDeviceExtendedDynamicStateFeaturesEXT availablePhysicalDeviceExtendedDynamicStateFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT
			};
			VkPhysicalDeviceVulkan13Features availablePhysicalDevice13Features = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
				.pNext = &availablePhysicalDeviceExtendedDynamicStateFeatures
			};
			VkPhysicalDeviceVulkan12Features availablePhysicalDevice12Features = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
				.pNext = &availablePhysicalDevice13Features
			};
			VkPhysicalDeviceVulkan11Features availablePhysicalDevice11Features = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
				.pNext = &availablePhysicalDevice12Features
			};
			VkPhysicalDeviceFeatures2 availablePhysicalDeviceFeatures2 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				.pNext = &availablePhysicalDevice11Features,
			};

			const char* enabledDeviceExtensions[16];
			const char* wantedDeviceExtensions[16] = {
				// VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,
				VK_KHR_SWAPCHAIN_EXTENSION_NAME
			};
			uint32_t wantedDeviceExtensionsCount = 1;

			VkDeviceCreateInfo deviceCreateInfo = {
				.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.queueCreateInfoCount    = 1,
				.enabledExtensionCount   = 0,
				.ppEnabledExtensionNames = enabledDeviceExtensions
			};

			if (vkGetPhysicalDeviceProperties2) {
				vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties2);
				vkGetPhysicalDeviceFeatures2(physicalDevice, &availablePhysicalDeviceFeatures2);
				deviceCreateInfo.pNext = &enabledPhysicalDeviceFeatures2;
			} else if (vkGetPhysicalDeviceProperties2KHR) {
				vkGetPhysicalDeviceProperties2KHR(physicalDevice, &physicalDeviceProperties2);
				vkGetPhysicalDeviceFeatures2KHR(physicalDevice, &availablePhysicalDeviceFeatures2);
				deviceCreateInfo.pNext = &enabledPhysicalDeviceFeatures2;
			} else {
				vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties2.properties);
				vkGetPhysicalDeviceFeatures(physicalDevice, &availablePhysicalDeviceFeatures2.features);
				deviceCreateInfo.pEnabledFeatures = &enabledPhysicalDeviceFeatures2.features;
			}

			enabledPhysicalDeviceFeatures2.features.multiDrawIndirect         = availablePhysicalDeviceFeatures2.features.multiDrawIndirect;
			enabledPhysicalDeviceFeatures2.features.drawIndirectFirstInstance = availablePhysicalDeviceFeatures2.features.drawIndirectFirstInstance;
			enabledPhysicalDeviceFeatures2.features.depthClamp                = availablePhysicalDeviceFeatures2.features.depthClamp;
			enabledPhysicalDeviceFeatures2.features.textureCompressionBC      = availablePhysicalDeviceFeatures2.features.textureCompressionBC;
			enabledPhysicalDeviceFeatures2.features.shaderClipDistance        = availablePhysicalDeviceFeatures2.features.shaderClipDistance;
			enabledPhysicalDeviceFeatures2.features.samplerAnisotropy         = availablePhysicalDeviceFeatures2.features.samplerAnisotropy;

			if (physicalDeviceProperties2.properties.apiVersion >= VK_API_VERSION_1_1) {
				enabledPhysicalDeviceFeatures2.pNext                 = &enabledPhysicalDevice11Features;
				enabledPhysicalDevice11Features.shaderDrawParameters = availablePhysicalDevice11Features.shaderDrawParameters;
			} else {
				wantedDeviceExtensions[wantedDeviceExtensionsCount++] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
				wantedDeviceExtensions[wantedDeviceExtensionsCount++] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
			}

			if (physicalDeviceProperties2.properties.apiVersion >= VK_API_VERSION_1_2) {
				enabledPhysicalDevice11Features.pNext             = &enabledPhysicalDevice12Features;
				enabledPhysicalDevice12Features.drawIndirectCount = availablePhysicalDevice12Features.drawIndirectCount;
			} else {

			}

			if (physicalDeviceProperties2.properties.apiVersion >= VK_API_VERSION_1_3) {
				enabledPhysicalDevice12Features.pNext = &enabledPhysicalDevice13Features;
			} else {
				if (availablePhysicalDeviceExtendedDynamicStateFeatures.extendedDynamicState) {
					for (uint32_t i = 0; i < deviceExtensionPropertiesCount; i++) {
						if (strcmp(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, deviceExtensionProperties[i].extensionName) == 0) {
							enabledDeviceExtensions[deviceCreateInfo.enabledExtensionCount++] = VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME;
							break;
						}
					}

					enabledPhysicalDeviceExtendedDynamicStateFeatures.extendedDynamicState = VK_TRUE;
					enabledPhysicalDeviceExtendedDynamicStateFeatures.pNext                = (void*)deviceCreateInfo.pNext;
					deviceCreateInfo.pNext                                                 = &enabledPhysicalDeviceExtendedDynamicStateFeatures;
				}
			}

			if (!enabledPhysicalDevice11Features.shaderDrawParameters)
				wantedDeviceExtensions[wantedDeviceExtensionsCount++] = VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME;

			if (!enabledPhysicalDevice12Features.drawIndirectCount)
				wantedDeviceExtensions[wantedDeviceExtensionsCount++] = VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME;

			for (uint32_t i = 0; i < wantedDeviceExtensionsCount; i++) {
				for (uint32_t j = 0; j < deviceExtensionPropertiesCount; j++) {
					if (strcmp(wantedDeviceExtensions[i], deviceExtensionProperties[j].extensionName) == 0) {
						enabledDeviceExtensions[deviceCreateInfo.enabledExtensionCount++] = wantedDeviceExtensions[i];
						break;
					}
				}
			}

			deviceCreateInfo.pQueueCreateInfos = &(VkDeviceQueueCreateInfo){
				.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = queueFamilyIndex,
				.queueCount       = queueCount,
				.pQueuePriorities = (float[]){ 1.f, 0.5f }
			};

			if ((r = vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device)) != VK_SUCCESS)
				exitVk("vkCreateDevice");

			#define F(fn) fn = (PFN_##fn)vkGetDeviceProcAddr(device, #fn);
			DEVICE_FUNCS(F)
			#undef F

			if ((r = vkCreateRenderPass(device, &(VkRenderPassCreateInfo){
				.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount   = 2,
				.pAttachments      = (VkAttachmentDescription[]){
					{
						.format         = surfaceFormat.format,
						.samples        = VK_SAMPLE_COUNT_1_BIT,
						.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
						.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
						.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
						.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
						.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
					}, {
						.format         = VK_FORMAT_D24_UNORM_S8_UINT,
						.samples        = VK_SAMPLE_COUNT_1_BIT,
						.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
						.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
						.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
						.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
					}
				},
				.subpassCount      = 1,
				.pSubpasses	       = &(VkSubpassDescription){
					.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
					.colorAttachmentCount    = 1,
					.pColorAttachments       = &(VkAttachmentReference){
						.attachment = 0,
						.layout	    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
					},
					.pDepthStencilAttachment = &(VkAttachmentReference){
						.attachment = 1,
						.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
					}
				},
				.dependencyCount   = 1,
				.pDependencies     = &(VkSubpassDependency){
					.srcSubpass      = VK_SUBPASS_EXTERNAL,
					.dstSubpass      = 0,
					.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
					.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
					.srcAccessMask   = 0,
					.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
				}
			}, NULL, &renderPass)) != VK_SUCCESS)
				exitVk("vkCreateRenderPass");
		} break;
		case WM_DESTROY: {
			PostQuitMessage(0);
		} break;
		case WM_SIZE: {
			static VkImage depthImage;
			static VkImageView depthImageView;
			static VkDeviceMemory depthImageMemory;
			static VkImageView swapchainImageViews[8];
			static uint32_t swapchainImagesCount;

			VkSurfaceCapabilitiesKHR surfaceCapabilities;
			if ((r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) != VK_SUCCESS)
				exitVk("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

			if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0) {
				// TODO
			}

			scissor.extent  = surfaceCapabilities.currentExtent;
			viewport.width  = (float)surfaceCapabilities.currentExtent.width;
			viewport.height = (float)surfaceCapabilities.currentExtent.height;

			float f                = 1.f / tanf(cameraFieldOfView / 2.f);
			projectionMatrix[0][0] = f / (viewport.width / viewport.height);
			projectionMatrix[1][1] = -f;
			projectionMatrix[3][2] = -1.f;
			projectionMatrix[2][3] = cameraNear;

			VkSwapchainCreateInfoKHR swapchainCreateInfo = {
				.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.surface          = surface,
				.minImageCount    = surfaceCapabilities.minImageCount + 1,
				.imageFormat      = surfaceFormat.format,
				.imageColorSpace  = surfaceFormat.colorSpace,
				.imageExtent      = scissor.extent,
				.imageArrayLayers = 1,
				.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode      = VK_PRESENT_MODE_FIFO_KHR,
				.preTransform     = surfaceCapabilities.currentTransform,
				.clipped          = VK_TRUE,
				.oldSwapchain     = swapchain
			};

			if ((r = vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain)) != VK_SUCCESS)
				exitVk("vkCreateSwapchainKHR");

			if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE) {
				if ((r = vkDeviceWaitIdle(device)) != VK_SUCCESS)
					exitVk("vkDeviceWaitIdle");

				vkDestroyImageView(device, depthImageView, NULL);
				vkDestroyImage(device, depthImage, NULL);
				vkFreeMemory(device, depthImageMemory, NULL);

				for (uint32_t i = 0; i < swapchainImagesCount; i++) {
					vkDestroyFramebuffer(device, swapchainFramebuffers[i], NULL);
					vkDestroyImageView(device, swapchainImageViews[i], NULL);
				}
				vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, NULL);
			}

			VkImage swapchainImages[8];
			swapchainImagesCount = ARRAY_COUNT(swapchainImages);
			if ((r = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImagesCount, swapchainImages)) != VK_SUCCESS)
				exitVk("vkGetSwapchainImagesKHR");

			if ((r = vkCreateImage(device, &(VkImageCreateInfo){
				.sType        = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType    = VK_IMAGE_TYPE_2D,
				.format       = VK_FORMAT_D24_UNORM_S8_UINT,
				.extent       = {
					.width  = scissor.extent.width,
					.height = scissor.extent.height,
					.depth  = 1
				},
				.mipLevels    = 1,
				.arrayLayers  = 1,
				.samples      = VK_SAMPLE_COUNT_1_BIT,
				.tiling       = VK_IMAGE_TILING_OPTIMAL,
				.usage        = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
			}, NULL, &depthImage)) != VK_SUCCESS)
				exitVk("vkCreateImage");

			VkMemoryDedicatedRequirements memoryDedicatedRequirements = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS
			};

			VkMemoryRequirements2 memoryRequirements2 = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
				.pNext = &memoryDedicatedRequirements
			};

			VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
				.image = depthImage
			};

			VkMemoryAllocateInfo memoryAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
			};

			if (vkGetImageMemoryRequirements2 || vkGetImageMemoryRequirements2KHR) {
				(vkGetImageMemoryRequirements2 ? vkGetImageMemoryRequirements2 : vkGetImageMemoryRequirements2KHR)(device, &(VkImageMemoryRequirementsInfo2){
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
					.image = depthImage
				}, &memoryRequirements2);

				if (memoryDedicatedRequirements.prefersDedicatedAllocation)
					memoryAllocateInfo.pNext = &memoryDedicatedAllocateInfo;
			} else {
				vkGetImageMemoryRequirements(device, depthImage, &memoryRequirements2.memoryRequirements);
			}

			memoryAllocateInfo.allocationSize  = memoryRequirements2.memoryRequirements.size;
			memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

			if ((r = vkAllocateMemory(device, &memoryAllocateInfo, NULL, &depthImageMemory)) != VK_SUCCESS)
				exitVk("vkAllocateMemory");

			if ((r = vkBindImageMemory(device, depthImage, depthImageMemory, 0)) != VK_SUCCESS)
				exitVk("vkBindImageMemory");

			if ((r = vkCreateImageView(device, &(VkImageViewCreateInfo){
				.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image            = depthImage,
				.viewType         = VK_IMAGE_VIEW_TYPE_2D,
				.format           = VK_FORMAT_D24_UNORM_S8_UINT,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
					.levelCount = 1,
					.layerCount = 1
				}
			}, NULL, &depthImageView)) != VK_SUCCESS)
				exitVk("vkCreateImageView");

			for (uint32_t i = 0; i < swapchainImagesCount; i++) {
				if ((r = vkCreateImageView(device, &(VkImageViewCreateInfo){
					.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image            = swapchainImages[i],
					.viewType         = VK_IMAGE_VIEW_TYPE_2D,
					.format           = surfaceFormat.format,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1
					}
				}, NULL, &swapchainImageViews[i])) != VK_SUCCESS)
					exitVk("vkCreateImageView");

				if ((r = vkCreateFramebuffer(device, &(VkFramebufferCreateInfo){
					.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
					.renderPass      = renderPass,
					.attachmentCount = 2,
					.pAttachments    = (VkImageView[]){
						swapchainImageViews[i],
						depthImageView
					},
					.width           = surfaceCapabilities.currentExtent.width,
					.height          = surfaceCapabilities.currentExtent.height,
					.layers          = 1
				}, NULL, &swapchainFramebuffers[i])) != VK_SUCCESS)
					exitVk("vkCreateFramebuffer");
			}
		} break;
		case WM_ACTIVATE: {
			if (wParam == WA_INACTIVE) {

			} else {

			}
		} break;
		case WM_CLOSE: {
			if (!DestroyWindow(hWnd))
				exitWin32("DestroyWindow");
		} break;
		case WM_GETMINMAXINFO: {
			((MINMAXINFO*)lParam)->ptMinTrackSize.x = 640;
			((MINMAXINFO*)lParam)->ptMinTrackSize.y = 360;
		} break;
		case WM_INPUT: {
			RAWINPUT rawInput;
			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &rawInput, &(UINT){ sizeof(RAWINPUT) }, sizeof(RAWINPUTHEADER)) == (UINT)-1)
				exitWin32("GetRawInputData");

			cameraYaw -= 0.0005f * (float)rawInput.data.mouse.lLastX;
			cameraPitch -= 0.0005f * (float)rawInput.data.mouse.lLastY;
			cameraYaw += (cameraYaw > (float)M_PI) ? -(float)M_PI * 2.f : (cameraYaw < -(float)M_PI) ? (float)M_PI * 2.f : 0.f;
			cameraPitch = CLAMP(cameraPitch, -(float)M_PI * 0.4f, (float)M_PI * 0.3f);
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		} break;
		case WM_KEYDOWN: {
			switch (wParam) {
				case VK_ESCAPE: {

				} break;
				case VK_LEFT:  case 0x41: input |= INPUT_START_LEFT;    break;
				case VK_UP:    case 0x57: input |= INPUT_START_FORWARD; break;
				case VK_RIGHT: case 0x44: input |= INPUT_START_RIGHT;   break;
				case VK_DOWN:  case 0x53: input |= INPUT_START_BACK;    break;
				case VK_SPACE:            input |= INPUT_START_UP;      break;
				case VK_SHIFT: {
					if (MapVirtualKeyW((lParam & 0x00FF0000) >> 16, MAPVK_VSC_TO_VK_EX) == VK_LSHIFT)
						input |= INPUT_START_DOWN;
				} break;
				case VK_F11: {
					static WINDOWPLACEMENT previous = { sizeof(WINDOWPLACEMENT) };

					LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
					if (!style)
						exitWin32("GetWindowLongPtrW");

					if (style & WS_OVERLAPPEDWINDOW) {
						MONITORINFO current = { sizeof(MONITORINFO) };
						if (!GetWindowPlacement(hWnd, &previous))
							exitWin32("GetWindowPlacement");

						if (!GetMonitorInfoW(MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY), &current))
							exitWin32("GetMonitorInfoW");

						if (!SetWindowLongPtrW(hWnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW))
							exitWin32("SetWindowLongPtrW");

						if (!SetWindowPos(hWnd, HWND_TOP, current.rcMonitor.left, current.rcMonitor.top, current.rcMonitor.right - current.rcMonitor.left, current.rcMonitor.bottom - current.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
							exitWin32("SetWindowPos");
					} else {
						if (!SetWindowPlacement(hWnd, &previous))
							exitWin32("SetWindowPlacement");

						if (!SetWindowLongPtrW(hWnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW))
							exitWin32("SetWindowLongPtrW");

						if (!SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
							exitWin32("SetWindowPos");
					}
				} break;
			}
		} break;
		case WM_KEYUP: {
			switch (wParam) {
				case VK_LEFT:  case 0x41: input |= INPUT_STOP_LEFT;    break;
				case VK_UP:    case 0x57: input |= INPUT_STOP_FORWARD; break;
				case VK_RIGHT: case 0x44: input |= INPUT_STOP_RIGHT;   break;
				case VK_DOWN:  case 0x53: input |= INPUT_STOP_BACK;    break;
				case VK_SPACE:            input |= INPUT_STOP_UP;      break;
				case VK_SHIFT: {
					if (MapVirtualKeyW((lParam & 0x00FF0000) >> 16, MAPVK_VSC_TO_VK_EX) == VK_LSHIFT)
						input |= INPUT_STOP_DOWN;
				} break;
			}
		} break;
		case WM_TIMER: {
			if (wParam == 1)
				SwitchToFiber(mainFiber);
		} break;
		case WM_ENTERMENULOOP:
		case WM_ENTERSIZEMOVE: {
			if (!SetTimer(hWnd, 1, 1, NULL))
				exitWin32("SetTimer");
		} break;
		case WM_EXITMENULOOP:
		case WM_EXITSIZEMOVE: {
			if (!KillTimer(hWnd, 1))
				exitWin32("KillTimer");
		} break;
		default: {
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		} break;
	}

	return 0;
}

void CALLBACK messageFiberProc(LPVOID lpParameter) {
	for (;;) {
		MSG msg;
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				ExitProcess(0);

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		SwitchToFiber(mainFiber);
	}
}

void WinMainCRTStartup(void) {
	LARGE_INTEGER performanceFrequency;
	if (!QueryPerformanceFrequency(&performanceFrequency))
		exitWin32("QueryPerformanceFrequency");

	LARGE_INTEGER start;
	if (!QueryPerformanceCounter(&start))
		exitWin32("QueryPerformanceCounter");

	if (!(hInstance = GetModuleHandleW(NULL)))
		exitWin32("GetModuleHandleW");

	if (!(mainFiber = ConvertThreadToFiber(NULL)))
		exitWin32("ConvertThreadToFiber");

	LPVOID messageFiber = CreateFiber(0, messageFiberProc, NULL);
	if (!messageFiber)
		exitWin32("CreateFiber");

	WNDCLASSEXW windowClass = {
		.cbSize        = sizeof(WNDCLASSEXW),
		.lpfnWndProc   = wndProc,
		.hInstance     = hInstance,
		.lpszClassName = L"a",
		.hCursor       = LoadImageW(NULL, MAKEINTRESOURCEW(OCR_NORMAL), IMAGE_CURSOR, 0, 0, LR_SHARED)
	};
	if (!windowClass.hCursor)
		exitWin32("LoadImageW");

	if (!RegisterClassExW(&windowClass))
		exitWin32("RegisterClassExW");

	if (!CreateWindowExW(0, windowClass.lpszClassName, L"Triangle", WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, windowClass.hInstance, NULL))
		exitWin32("CreateWindowExW");

	int wsaError;
	WSADATA wsaData;
	if ((wsaError = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
		exitHRESULT("WSAStartup", wsaError);

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET)
		exitWSA("socket");

	if (ioctlsocket(sock, FIONBIO, &(u_long){ 1 }))
		exitWSA("ioctlsocket");

	if (bind(sock, (struct sockaddr*)&(struct sockaddr_in){
		.sin_family      = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_ANY),
		.sin_port        = htons(9000)
	}, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
		exitWSA("bind");

	uint64_t ticksElapsed = 0;

	VkCommandPool commandPools[FRAMES_IN_FLIGHT];
	VkCommandBuffer commandBuffers[FRAMES_IN_FLIGHT];
	VkFence fences[FRAMES_IN_FLIGHT];
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipelines[GRAPHICS_PIPELINE_MAX];
	VkQueue queues[2];
	VkSemaphore imageAcquireSemaphores[FRAMES_IN_FLIGHT];
	VkSemaphore semaphores[FRAMES_IN_FLIGHT];
	VkBuffer buffers[BUFFER_MAX];

	vkGetDeviceQueue(device, queueFamilyIndex, 0, &queues[0]);
	vkGetDeviceQueue(device, queueFamilyIndex, queueCount - 1, &queues[1]);

	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		if ((r = vkCreateCommandPool(device, &(VkCommandPoolCreateInfo){
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.queueFamilyIndex = queueFamilyIndex
		}, NULL, &commandPools[i])) != VK_SUCCESS)
			exitVk("vkCreateCommandPool");

		if ((r = vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo){
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool        = commandPools[i],
			.commandBufferCount = 1
		}, &commandBuffers[i])) != VK_SUCCESS)
			exitVk("vkAllocateCommandBuffers");

		if ((r = vkCreateFence(device, &(VkFenceCreateInfo){
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		}, NULL, &fences[i])) != VK_SUCCESS)
			exitVk("vkCreateFence");

		if ((r = vkCreateSemaphore(device, &(VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		}, NULL, &imageAcquireSemaphores[i])) != VK_SUCCESS)
			exitVk("vkCreateSemaphore");

		if ((r = vkCreateSemaphore(device, &(VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		}, NULL, &semaphores[i])) != VK_SUCCESS)
			exitVk("vkCreateSemaphore");
	}

	if ((r = vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &(VkPushConstantRange){
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = sizeof(Mat4)
		}
	}, NULL, &pipelineLayout)) != VK_SUCCESS)
		exitVk("vkCreatePipelineLayout");

	{
		#include "shaders/triangle.vert.h"
		#include "shaders/triangle.frag.h"

		VkShaderModule triangleVert;
		VkShaderModule triangleFrag;

		if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
			.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pCode    = triangle_vert,
			.codeSize = sizeof(triangle_vert)
		}, NULL, &triangleVert)) != VK_SUCCESS)
			exitVk("vkCreateShaderModule");

		if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
			.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pCode    = triangle_frag,
			.codeSize = sizeof(triangle_frag)
		}, NULL, &triangleFrag)) != VK_SUCCESS)
			exitVk("vkCreateShaderModule");

		VkPipelineShaderStageCreateInfo shaderStages[] = {
			{
				.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage  = VK_SHADER_STAGE_VERTEX_BIT,
				.module = triangleVert,
				.pName  = "main"
			}, {
				.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = triangleFrag,
				.pName  = "main"
			}
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = {
			.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount   = 0,
			.pVertexBindingDescriptions      = NULL,
			.vertexAttributeDescriptionCount = 0,
			.pVertexAttributeDescriptions    = NULL
		};
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
			.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		};
		VkPipelineViewportStateCreateInfo viewportState = {
			.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount  = 1
		};
		VkPipelineRasterizationStateCreateInfo rasterizationState = {
			.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.lineWidth = 1.f
		};
		VkPipelineMultisampleStateCreateInfo multisampleState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
		};
		VkPipelineDepthStencilStateCreateInfo depthStencilState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		};
		VkPipelineColorBlendStateCreateInfo colorBlendState = {
			.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments    = &(VkPipelineColorBlendAttachmentState){
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
			}
		};
		VkPipelineDynamicStateCreateInfo dynamicState = {
			.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 2,
			.pDynamicStates    = (VkDynamicState[]){
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR
			}
		};

		if ((r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, GRAPHICS_PIPELINE_MAX, (VkGraphicsPipelineCreateInfo[]){
			[GRAPHICS_PIPELINE_TRIANGLE] = {
				.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.stageCount          = ARRAY_COUNT(shaderStages),
				.pStages             = shaderStages,
				.pVertexInputState   = &vertexInputState,
				.pInputAssemblyState = &inputAssemblyState,
				.pViewportState      = &viewportState,
				.pRasterizationState = &rasterizationState,
				.pMultisampleState   = &multisampleState,
				.pDepthStencilState  = &depthStencilState,
				.pColorBlendState    = &colorBlendState,
				.pDynamicState       = &dynamicState,
				.layout              = pipelineLayout,
				.renderPass          = renderPass
			}
		}, NULL, graphicsPipelines)) != VK_SUCCESS)
			exitVk("vkCreateGraphicsPipelines");
	}

	{
		float vertexPositions[] = {
			0.0, -0.5, 0.0,
			0.5, 0.5, 0.0,
			-0.5, 0.5, 0.0
		};
		uint16_t indices[48];

		struct {
			VkDeviceSize size;
			VkBufferUsageFlags usage;
			VkMemoryPropertyFlags requiredMemoryPropertyFlagBits;
			VkMemoryPropertyFlags optionalMemoryPropertyFlagBits;
		} bufferInfos[BUFFER_MAX] = {
			[BUFFER_INDICES] = {
				.size                           = sizeof(indices),
				.usage                          = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			}, [BUFFER_VERTICES] = {
				.size                           = sizeof(vertexPositions),
				.usage                          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			}, [BUFFER_UNIFORM] = {
				.size                           = 1024,
				.usage                          = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				.optionalMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			}
		};

		for (uint32_t i = 0; i < BUFFER_MAX; i++) {
			if ((r = vkCreateBuffer(device, &(VkBufferCreateInfo){
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size  = bufferInfos[i].size,
				.usage = bufferInfos[i].usage
			}, NULL, &buffers[i])) != VK_SUCCESS)
				exitVk("vkCreateBuffer");

			VkMemoryRequirements2 memoryRequirements2 = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2
			};

			if (vkGetBufferMemoryRequirements2 || vkGetBufferMemoryRequirements2KHR) {
				VkMemoryDedicatedRequirements memoryDedicatedRequirements = {
					.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS
				};
				memoryRequirements2.pNext = &memoryDedicatedRequirements;

				(vkGetBufferMemoryRequirements2 ? vkGetBufferMemoryRequirements2 : vkGetBufferMemoryRequirements2KHR) (device, &(VkBufferMemoryRequirementsInfo2){
					.sType  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
					.buffer = buffers[i]
				}, &memoryRequirements2);

				if (memoryDedicatedRequirements.prefersDedicatedAllocation) {
					VkDeviceMemory memory;
					if ((r = vkAllocateMemory(device, &(VkMemoryAllocateInfo){
						.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
						.pNext           = &(VkMemoryDedicatedAllocateInfo){
							.sType  = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
							.buffer = buffers[i]
						},
						.allocationSize  = memoryRequirements2.memoryRequirements.size,
						.memoryTypeIndex = getMemoryTypeIndex(
							memoryRequirements2.memoryRequirements.memoryTypeBits,
							bufferInfos[i].requiredMemoryPropertyFlagBits,
							bufferInfos[i].optionalMemoryPropertyFlagBits)
					}, NULL, &memory)) != VK_SUCCESS)
						exitVk("vkAllocateMemory");

					if ((r = vkBindBufferMemory(device, buffers[i], memory, 0)) != VK_SUCCESS)
						exitVk("vkBindImageMemory");
				}
			} else {
				vkGetBufferMemoryRequirements(device, buffers[i], &memoryRequirements2.memoryRequirements);
				// TODO
			}
		}
	}

	for (;;) {
		SwitchToFiber(messageFiber);

		char buffer[1024];
		struct sockaddr_in clientAddr;
		int n;
		while ((n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddr, &(int){ sizeof(struct sockaddr_in) })) > 0) {
			char str[INET_ADDRSTRLEN];
			if (inet_ntop(AF_INET, &clientAddr.sin_addr, str, sizeof(str)) == NULL)
				exitWSA("inet_ntop");

			char ABC[1024];
			sprintf(ABC, "Received %d bytes from %s:%d\n%s\n", n, str, ntohs(clientAddr.sin_port), buffer);
			MessageBoxA(NULL, ABC, NULL, MB_OK);
		}

		if (n == SOCKET_ERROR) {
			int err = WSAGetLastError();
			if (err != WSAEWOULDBLOCK)
				exitWSA("recvfrom");
		}

		while (({
			LARGE_INTEGER now;
			if (!QueryPerformanceCounter(&now))
				exitWin32("QueryPerformanceCounter");

			uint64_t targetTicksElapsed = ((now.QuadPart - start.QuadPart) * TICKS_PER_SECOND) / performanceFrequency.QuadPart;
			ticksElapsed < targetTicksElapsed;
		})) {
			float forward = 0.f;
			float strafe = 0.f;

			if ((input & INPUT_START_FORWARD) && !(input & INPUT_START_BACK)) {
				forward = 1.f;
			} else if ((input & INPUT_START_BACK) && !(input & INPUT_START_FORWARD)) {
				forward = -1.f;
			}

			if ((input & INPUT_START_LEFT) && !(input & INPUT_START_RIGHT)) {
				strafe = -1.f;
			} else if ((input & INPUT_START_RIGHT) && !(input & INPUT_START_LEFT)) {
				strafe = 1.f;
			}

			if ((input & INPUT_START_UP) && !(input & INPUT_START_DOWN)) {
				player.translation.y += 0.05f;
			} else if ((input & INPUT_START_DOWN) && !(input & INPUT_START_UP)) {
				player.translation.y -= 0.05f;
			}

			if (input & INPUT_STOP_FORWARD) input &= ~(INPUT_START_FORWARD | INPUT_STOP_FORWARD);
			if (input & INPUT_STOP_BACK)    input &= ~(INPUT_START_BACK | INPUT_STOP_BACK);
			if (input & INPUT_STOP_LEFT)    input &= ~(INPUT_START_LEFT | INPUT_STOP_LEFT);
			if (input & INPUT_STOP_RIGHT)   input &= ~(INPUT_START_RIGHT | INPUT_STOP_RIGHT);
			if (input & INPUT_STOP_DOWN)    input &= ~(INPUT_START_DOWN | INPUT_STOP_DOWN);
			if (input & INPUT_STOP_UP)      input &= ~(INPUT_START_UP | INPUT_STOP_UP);

			float sy = sinf(cameraYaw);
			float cy = cosf(cameraYaw);

			Vec3 forwardMovement  = { forward * -sy, 0, forward * -cy };
			Vec3 sidewaysMovement = { strafe * cy, 0, strafe * -sy };

			player.translation += 0.05f * forwardMovement;
			player.translation += 0.05f * sidewaysMovement;

			cameraPosition = player.translation; // TODO
			ticksElapsed++;
		}

		static uint32_t frame = 0;
		uint32_t imageIndex;
		if ((r = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquireSemaphores[frame], VK_NULL_HANDLE, &imageIndex)) != VK_SUCCESS)
			exitVk("vkAcquireNextImageKHR");

		if ((r = vkWaitForFences(device, 1, &fences[frame], VK_TRUE, UINT64_MAX)) != VK_SUCCESS)
			exitVk("vkWaitForFences");

		if ((r = vkResetFences(device, 1, &fences[frame])) != VK_SUCCESS)
			exitVk("vkResetFences");

		if ((r = vkResetCommandPool(device, commandPools[frame], 0)) != VK_SUCCESS)
			exitVk("vkResetCommandPool");

		if ((r = vkBeginCommandBuffer(commandBuffers[frame], &(VkCommandBufferBeginInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		})) != VK_SUCCESS)
			exitVk("vkBeginCommandBuffer");

		vkCmdSetScissor(commandBuffers[frame], 0, 1, &scissor);
		vkCmdSetViewport(commandBuffers[frame], 0, 1, &viewport);

		vkCmdBeginRenderPass(commandBuffers[frame], &(VkRenderPassBeginInfo){
			.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass      = renderPass,
			.framebuffer     = swapchainFramebuffers[imageIndex],
			.renderArea      = scissor,
			.clearValueCount = 2,
			.pClearValues    = (VkClearValue[]){ { 0 }, { 0 } }
		}, VK_SUBPASS_CONTENTS_INLINE);

		float sy = sinf(cameraYaw);
		float cy = cosf(cameraYaw);
		float sp = sinf(cameraPitch);
		float cp = cosf(cameraPitch);

		Vec3 xAxis = { cy, 0, -sy };
		Vec3 yAxis = { sy * sp, cp, cy * sp };
		Vec3 zAxis = { sy * cp, -sp, cp * cy };

		Mat4 viewMatrix;
		viewMatrix[0][0] = xAxis.x;
		viewMatrix[1][0] = yAxis.x;
		viewMatrix[2][0] = zAxis.x;
		viewMatrix[3][0] = 0.f;
		viewMatrix[0][1] = xAxis.y;
		viewMatrix[1][1] = yAxis.y;
		viewMatrix[2][1] = zAxis.y;
		viewMatrix[3][1] = 0.f;
		viewMatrix[0][2] = xAxis.z;
		viewMatrix[1][2] = yAxis.z;
		viewMatrix[2][2] = zAxis.z;
		viewMatrix[3][2] = 0.f;
		viewMatrix[0][3] = -vec3Dot(xAxis, cameraPosition);
		viewMatrix[1][3] = -vec3Dot(yAxis, cameraPosition);
		viewMatrix[2][3] = -vec3Dot(zAxis, cameraPosition);
		viewMatrix[3][3] = 1.f;

		Mat4 viewProjection = projectionMatrix * viewMatrix;
		vkCmdPushConstants(commandBuffers[frame], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &viewProjection);

		vkCmdBindPipeline(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_TRIANGLE]);
		vkCmdDraw(commandBuffers[frame], 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers[frame]);

		if ((r = vkEndCommandBuffer(commandBuffers[frame])) != VK_SUCCESS)
			exitVk("vkEndCommandBuffer");

		if ((r = vkQueueSubmit(queues[0], 1, &(VkSubmitInfo){
			.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount   = 1,
			.pWaitSemaphores      = &imageAcquireSemaphores[frame],
			.pWaitDstStageMask    = &(VkPipelineStageFlags){
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			},
			.commandBufferCount   = 1,
			.pCommandBuffers      = &commandBuffers[frame],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores    = &semaphores[frame]
		}, fences[frame])) != VK_SUCCESS)
			exitVk("vkQueueSubmit");

		if ((r = vkQueuePresentKHR(queues[0], &(VkPresentInfoKHR){
			.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores    = &semaphores[frame],
			.swapchainCount     = 1,
			.pSwapchains        = &swapchain,
			.pImageIndices      = &imageIndex
		})) != VK_SUCCESS)
			exitVk("vkQueuePresentKHR");

		frame = (frame + 1) % FRAMES_IN_FLIGHT;
	}
}

int _fltused;
