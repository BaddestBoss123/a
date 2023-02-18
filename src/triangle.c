#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "vcruntime.lib")
#pragma comment(lib, "ucrt.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment (lib, "dwrite.lib")

#define TICKS_PER_SECOND 50
#define FRAMES_IN_FLIGHT 2
#define MAX_INSTANCES 2048
#define MAX_PARTICLES 1024
#define MAX_PORTAL_RECURSION 2

#define OEMRESOURCE
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#define VK_ENABLE_BETA_EXTENSIONS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "vulkan/vulkan.h"
#include <hidusage.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <stdbool.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "macros.h"

#include "shaders/blit.vert.h"
#include "shaders/blit.frag.h"
#include "shaders/triangle.vert.h"
#include "shaders/triangle.frag.h"
#include "shaders/skybox.vert.h"
#include "shaders/skybox.frag.h"
#include "shaders/particle.vert.h"
#include "shaders/particle.frag.h"
#include "shaders/portal.vert.h"
#include "shaders/portal.frag.h"
#include "shaders/voxel.vert.h"
#include "assets/assets.h"

static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
#define F(fn) static PFN_##fn fn;
	BEFORE_INSTANCE_FUNCS(F)
	INSTANCE_FUNCS(F)
	DEVICE_FUNCS(F)
#undef F

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

typedef enum GraphicsPipeline {
	GRAPHICS_PIPELINE_BLIT,
	GRAPHICS_PIPELINE_TRIANGLE,
	GRAPHICS_PIPELINE_SKYBOX,
	GRAPHICS_PIPELINE_PARTICLE,
	GRAPHICS_PIPELINE_PORTAL_STENCIL,
	GRAPHICS_PIPELINE_PORTAL_DEPTH,
	GRAPHICS_PIPELINE_VOXEL,
	GRAPHICS_PIPELINE_MAX
} GraphicsPipeline;

typedef struct Buffer {
	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
	void* data;
	VkDeviceSize size;
	VkBufferUsageFlags usage;
	VkMemoryPropertyFlags requiredMemoryPropertyFlagBits;
	VkMemoryPropertyFlags optionalMemoryPropertyFlagBits;
} Buffer;

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

// typedef struct Scene {
// 	Entity* root;
// 	Portal* portals;
// 	Mat4* modelMatrices;
// 	uint32_t drawCount;
// 	GraphicsPipeline skybox;
// } Scene;

typedef uint8_t VoxelChunk[16][16][16];

static uint16_t vertexIndices[] = { 0, 1, 2 };
static float vertexPositions[] = {
	0.0, -0.5, 0.0,
	0.5, 0.5, 0.0,
	-0.5, 0.5, 0.0
};
static uint16_t quadIndices[48];

typedef enum BufferRanges {
	BUFFER_RANGE_INDEX_VERTICES     = ALIGN_FORWARD(sizeof(vertexIndices), 256),
	BUFFER_RANGE_INDEX_QUADS        = ALIGN_FORWARD(sizeof(quadIndices), 256),
	BUFFER_RANGE_VERTEX_POSITIONS   = ALIGN_FORWARD(sizeof(vertexPositions), 256),
	BUFFER_RANGE_PARTICLES          = ALIGN_FORWARD(MAX_PARTICLES, 256),
	BUFFER_RANGE_INSTANCE_MVPS      = ALIGN_FORWARD(MAX_INSTANCES * sizeof(Mat4), 256),
	BUFFER_RANGE_INSTANCE_MATERIALS = ALIGN_FORWARD(MAX_INSTANCES * sizeof(Material), 256)
} BufferRanges;

typedef enum BufferOffsets {
	BUFFER_OFFSET_INDEX_VERTICES     = 0,
	BUFFER_OFFSET_INDEX_QUADS        = 0,
	BUFFER_OFFSET_VERTEX_POSITIONS   = 0,
	BUFFER_OFFSET_PARTICLES          = 0,
	BUFFER_OFFSET_INSTANCE_MVPS      = FRAMES_IN_FLIGHT * 0,
	BUFFER_OFFSET_INSTANCE_MATERIALS = FRAMES_IN_FLIGHT * BUFFER_RANGE_INSTANCE_MVPS
} BufferOffsets;

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
static VkPhysicalDeviceProperties2 physicalDeviceProperties2               = {
	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
	.pNext = &physicalDeviceVulkan11Properties
};

static struct {
	Buffer staging;
	Buffer index;
	Buffer vertex;
	Buffer particles;
	Buffer instance;
	Buffer uniform;
} buffers = {
	.staging   = {
		.size                           = 32 * 1024 * 1024,
		.usage                          = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	},
	.index     = {
		.size                           = BUFFER_RANGE_INDEX_QUADS,
		.usage                          = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	},
	.vertex    = {
		.size                           = BUFFER_RANGE_VERTEX_POSITIONS,
		.usage                          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	},
	.particles = {
		.size                           = FRAMES_IN_FLIGHT * BUFFER_RANGE_PARTICLES,
		.usage                          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	},
	.instance  = {
		.size                           = (FRAMES_IN_FLIGHT * BUFFER_RANGE_INSTANCE_MVPS) + (FRAMES_IN_FLIGHT * BUFFER_RANGE_INSTANCE_MATERIALS),
		.usage                          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		.optionalMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	},
	.uniform   = {
		.size                           = FRAMES_IN_FLIGHT * 1024,
		.usage                          = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		.optionalMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	}
};

static VkResult r;
static VkDevice device;
static VkPhysicalDevice physicalDevice;
static VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
static VkMemoryRequirements memoryRequirements;
static uint32_t queueFamilyIndex;
static uint32_t frame;
static VkRenderPass renderPass;
static VkRenderPass renderPassBlit;
static VkDescriptorSetLayout descriptorSetLayout;
static VkDescriptorPool descriptorPool;
static VkDescriptorSet descriptorSet;
static VkSurfaceKHR surface;
static VkSurfaceFormatKHR surfaceFormat;
static VkSwapchainKHR swapchain;
static VkFramebuffer framebuffer;
static VkFramebuffer swapchainFramebuffers[8];
static VkRect2D scissor;
static VkPipelineLayout pipelineLayout;
static VkPipeline graphicsPipelines[GRAPHICS_PIPELINE_MAX];
static VkQueue queue;
static VkCommandPool commandPools[FRAMES_IN_FLIGHT];
static VkCommandBuffer commandBuffers[FRAMES_IN_FLIGHT];
static VkFence fences[FRAMES_IN_FLIGHT];
static VkSemaphore imageAcquireSemaphores[FRAMES_IN_FLIGHT];
static VkSemaphore semaphores[FRAMES_IN_FLIGHT];
static VkShaderModule blitVert;
static VkShaderModule blitFrag;
static VkShaderModule triangleVert;
static VkShaderModule triangleFrag;
static VkShaderModule skyboxVert;
static VkShaderModule skyboxFrag;
static VkShaderModule particleVert;
static VkShaderModule particleFrag;
static VkShaderModule portalVert;
static VkShaderModule portalFrag;
static VkShaderModule voxelVert;
static VkViewport viewport = {
	.minDepth = 0.0,
	.maxDepth = 1.0
};

static HMODULE hInstance;
static LPVOID mainFiber;
static uint64_t ticksElapsed;
static uint64_t input;

static Entity player;
static Mat4 projectionMatrix;
static Vec3 cameraPosition;
static float cameraYaw;
static float cameraPitch;
static const float cameraFieldOfView = 0.78f;
static const float cameraNear        = 0.1f;

static Portal portals[] = {
	{
		.translation = { -20.f, 3.f, 0.f },
		.rotation    = { 0, 0, 0, 1 },
		.scale       = { 3, 3, 1 },
		.link        = 1
	}, {
		.translation = { 20.f, 3.f, 0.f },
		.rotation    = { 0, 0, 0, 1 },
		.scale       = { 3, 3, 1 },
		.link        = 0
	}
};

// static Scene mainScene = {
// 	.portals = portals,
// 	.skybox = GRAPHICS_PIPELINE_SKYBOX
// };

static VkCommandBuffer commandBuffer;
static uint32_t instanceIndex;
static Mat4 models[MAX_INSTANCES];
static Mat4* mvps;
static Material* materials;

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

void exitError(const char* function, HRESULT err) {
	char buff[1024];
	char errorMessage[1024];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, errorMessage, sizeof(errorMessage), NULL);
	sprintf(buff, "%s failed: %s", function, errorMessage);
	MessageBoxA(NULL, buff, NULL, MB_OK);
	ExitProcess(0);
}

HRESULT hr;
void exitHRESULT(const char* function) {
	exitError(function, hr);
}

static void exitWin32(const char* function) {
	exitError(function, GetLastError());
}

static void exitWSA(const char* function) {
	exitError(function, WSAGetLastError());
}

static void exitVk(const char* function) {
	char buff[512];
	sprintf(buff, "%s failed: %s", function, vkResultToString());
	MessageBoxA(NULL, buff, NULL, MB_OK);
	ExitProcess(0);
}

static uint32_t getMemoryTypeIndex(VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags optionalFlags) {
	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if ((memoryRequirements.memoryTypeBits & (1 << i)) && BITS_SET(physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags, requiredFlags | optionalFlags))
			return i;
	}

	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if ((memoryRequirements.memoryTypeBits & (1 << i)) && BITS_SET(physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags, requiredFlags))
			return i;
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
				.hwndTarget  = hWnd
			}, 1, sizeof(RAWINPUTDEVICE)))
				exitWin32("RegisterRawInputDevices");

			HMODULE vulkanModule = LoadLibraryW(L"vulkan-1.dll");
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

			const char* enabledInstanceExtensions[2];
			const char* wantedInstanceExtensions[2] = {
				VK_KHR_SURFACE_EXTENSION_NAME,
				VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
			};
			uint32_t wantedInstanceExtensionsCount = 2;

			if (applicationInfo.apiVersion < VK_API_VERSION_1_1)
				wantedInstanceExtensions[wantedInstanceExtensionsCount++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;

			VkInstanceCreateInfo instanceCreateInfo = {
				.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pApplicationInfo        = &applicationInfo,
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

			if (!vkGetPhysicalDeviceProperties2) vkGetPhysicalDeviceProperties2 = vkGetPhysicalDeviceProperties2KHR;

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
			for (uint32_t i = 0; i < surfaceFormatCount; i++) {
				if (surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
					surfaceFormat = surfaceFormats[i];
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
			} else {
				vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties2.properties);
				vkGetPhysicalDeviceFeatures(physicalDevice, &availablePhysicalDeviceFeatures2.features);
				deviceCreateInfo.pEnabledFeatures = &enabledPhysicalDeviceFeatures2.features;
			}

			enabledPhysicalDeviceFeatures2.features.textureCompressionBC = availablePhysicalDeviceFeatures2.features.textureCompressionBC;
			enabledPhysicalDeviceFeatures2.features.depthClamp           = availablePhysicalDeviceFeatures2.features.depthClamp;
			enabledPhysicalDeviceFeatures2.features.shaderClipDistance   = availablePhysicalDeviceFeatures2.features.shaderClipDistance;
			enabledPhysicalDeviceFeatures2.features.samplerAnisotropy    = availablePhysicalDeviceFeatures2.features.samplerAnisotropy;

			if (physicalDeviceProperties2.properties.apiVersion >= VK_API_VERSION_1_1) {
				enabledPhysicalDeviceFeatures2.pNext                 = &enabledPhysicalDevice11Features;
				// enabledPhysicalDevice11Features.shaderDrawParameters = availablePhysicalDevice11Features.shaderDrawParameters;
			} else {
				wantedDeviceExtensions[wantedDeviceExtensionsCount++] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
			}

			if (physicalDeviceProperties2.properties.apiVersion >= VK_API_VERSION_1_2) {
				enabledPhysicalDevice11Features.pNext             = &enabledPhysicalDevice12Features;
				// enabledPhysicalDevice12Features.drawIndirectCount = availablePhysicalDevice12Features.drawIndirectCount;
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

			// if (!enabledPhysicalDevice11Features.shaderDrawParameters)
			// 	wantedDeviceExtensions[wantedDeviceExtensionsCount++] = VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME;

			// if (!enabledPhysicalDevice12Features.drawIndirectCount)
			// 	wantedDeviceExtensions[wantedDeviceExtensionsCount++] = VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME;

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
				.queueCount       = 1,
				.pQueuePriorities = (float[]){ 1.f }
			};

			if ((r = vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device)) != VK_SUCCESS)
				exitVk("vkCreateDevice");

			#define F(fn) fn = (PFN_##fn)vkGetDeviceProcAddr(device, #fn);
			DEVICE_FUNCS(F)
			#undef F

			if ((r = vkCreateRenderPass(device, &(VkRenderPassCreateInfo){
				.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 2,
				.pAttachments    = (VkAttachmentDescription[]){
					{
						.format         = VK_FORMAT_R16G16B16A16_SFLOAT,
						.samples        = VK_SAMPLE_COUNT_1_BIT,
						.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
						.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
						.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
						.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
						.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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
				.subpassCount    = 1,
				.pSubpasses	     = &(VkSubpassDescription){
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
				.dependencyCount = 1,
				.pDependencies   = &(VkSubpassDependency){
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

			if ((r = vkCreateRenderPass(device, &(VkRenderPassCreateInfo){
				.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 1,
				.pAttachments    = &(VkAttachmentDescription){
					.format         = surfaceFormat.format,
					.samples        = VK_SAMPLE_COUNT_1_BIT,
					.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
					.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
					.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
				},
				.subpassCount    = 1,
				.pSubpasses	     = &(VkSubpassDescription){
					.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
					.colorAttachmentCount = 1,
					.pColorAttachments    = &(VkAttachmentReference){
						.attachment = 0,
						.layout	    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
					}
				},
				.dependencyCount = 1,
				.pDependencies   = &(VkSubpassDependency){
					.srcSubpass      = VK_SUBPASS_EXTERNAL,
					.dstSubpass      = 0,
					.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					.srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
				}
			}, NULL, &renderPassBlit)) != VK_SUCCESS)
				exitVk("vkCreateRenderPass");

			VkSampler samplers[1];
			if ((r = vkCreateSampler(device, &(VkSamplerCreateInfo){
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO
			}, NULL, &samplers[0])) != VK_SUCCESS)
				exitVk("vkCreateSampler");

			if ((r = vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo){
				.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = 2,
				.pBindings    = (VkDescriptorSetLayoutBinding[]){
					/*{
						.binding            = 0,
						.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
						.descriptorCount    = 1,
						.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
						.pImmutableSamplers = samplers
					},*/ {
						.binding         = 1,
						.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
						.descriptorCount = 1,
						.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT
					}, {
						.binding         = 2,
						.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
						.descriptorCount = 1,
						.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT
					}
				}
			}, NULL, &descriptorSetLayout)) != VK_SUCCESS)
				exitVk("vkCreateDescriptorSetLayout");

			if ((r = vkCreateDescriptorPool(device, &(VkDescriptorPoolCreateInfo){
				.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.maxSets       = 1,
				.poolSizeCount = 2,
				.pPoolSizes    = (VkDescriptorPoolSize[]){
					{
						.type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
						.descriptorCount = 1
					}, {
						.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
						.descriptorCount = 1
					}
				}
			}, NULL, &descriptorPool)) != VK_SUCCESS)
				exitVk("vkCreateDescriptorPool");

			if ((r = vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo){
				.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool     = descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts        = &descriptorSetLayout
			}, &descriptorSet)) != VK_SUCCESS)
				exitVk("vkAllocateDescriptorSets");
		} break;
		case WM_DESTROY: {
			PostQuitMessage(0);
		} break;
		case WM_SIZE: {
			static VkImage colorImage;
			static VkImageView colorImageView;
			static VkDeviceMemory colorDeviceMemory;
			static VkImage depthImage;
			static VkImageView depthImageView;
			static VkDeviceMemory depthDeviceMemory;
			static VkImageView swapchainImageViews[8];
			static uint32_t swapchainImagesCount;

			VkImage swapchainImages[8];
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
			projectionMatrix[2][3] = cameraNear;
			projectionMatrix[3][2] = -1.f;

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

				vkDestroyFramebuffer(device, framebuffer, NULL);

				vkDestroyImageView(device, colorImageView, NULL);
				vkDestroyImage(device, colorImage, NULL);
				vkFreeMemory(device, colorDeviceMemory, NULL);

				vkDestroyImageView(device, depthImageView, NULL);
				vkDestroyImage(device, depthImage, NULL);
				vkFreeMemory(device, depthDeviceMemory, NULL);

				for (uint32_t i = 0; i < swapchainImagesCount; i++) {
					vkDestroyFramebuffer(device, swapchainFramebuffers[i], NULL);
					vkDestroyImageView(device, swapchainImageViews[i], NULL);
				}
				vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, NULL);
			}

			swapchainImagesCount = ARRAY_COUNT(swapchainImages);
			if ((r = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImagesCount, swapchainImages)) != VK_SUCCESS)
				exitVk("vkGetSwapchainImagesKHR");

			if ((r = vkCreateImage(device, &(VkImageCreateInfo){
				.sType        = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType    = VK_IMAGE_TYPE_2D,
				.format       = VK_FORMAT_R16G16B16A16_SFLOAT,
				.extent       = {
					.width  = scissor.extent.width,
					.height = scissor.extent.height,
					.depth  = 1
				},
				.mipLevels    = 1,
				.arrayLayers  = 1,
				.samples      = VK_SAMPLE_COUNT_1_BIT,
				.tiling       = VK_IMAGE_TILING_OPTIMAL,
				.usage        = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			}, NULL, &colorImage)) != VK_SUCCESS)
				exitVk("vkCreateImage");

			vkGetImageMemoryRequirements(device, colorImage, &memoryRequirements);
			if ((r = vkAllocateMemory(device, &(VkMemoryAllocateInfo){
				.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext           = &(VkMemoryDedicatedAllocateInfo){
					.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
					.image = colorImage
				},
				.allocationSize  = memoryRequirements.size,
				.memoryTypeIndex = getMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0)
			}, NULL, &colorDeviceMemory)) != VK_SUCCESS)
				exitVk("vkAllocateMemory");
			if ((r = vkBindImageMemory(device, colorImage, colorDeviceMemory, 0)) != VK_SUCCESS)
				exitVk("vkBindImageMemory");

			if ((r = vkCreateImageView(device, &(VkImageViewCreateInfo){
				.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image            = colorImage,
				.viewType         = VK_IMAGE_VIEW_TYPE_2D,
				.format           = VK_FORMAT_R16G16B16A16_SFLOAT,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1
				}
			}, NULL, &colorImageView)) != VK_SUCCESS)
				exitVk("vkCreateImageView");

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

			vkGetImageMemoryRequirements(device, depthImage, &memoryRequirements);
			if ((r = vkAllocateMemory(device, &(VkMemoryAllocateInfo){
				.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext           = &(VkMemoryDedicatedAllocateInfo){
					.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
					.image = depthImage
				},
				.allocationSize  = memoryRequirements.size,
				.memoryTypeIndex = getMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0)
			}, NULL, &depthDeviceMemory)) != VK_SUCCESS)
				exitVk("vkAllocateMemory");
			if ((r = vkBindImageMemory(device, depthImage, depthDeviceMemory, 0)) != VK_SUCCESS)
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

			if ((r = vkCreateFramebuffer(device, &(VkFramebufferCreateInfo){
				.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass      = renderPass,
				.attachmentCount = 2,
				.pAttachments    = (VkImageView[]){
					colorImageView,
					depthImageView
				},
				.width           = surfaceCapabilities.currentExtent.width,
				.height          = surfaceCapabilities.currentExtent.height,
				.layers          = 1
			}, NULL, &framebuffer)) != VK_SUCCESS)
				exitVk("vkCreateFramebuffer");

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
					.renderPass      = renderPassBlit,
					.attachmentCount = 1,
					.pAttachments    = &swapchainImageViews[i],
					.width           = surfaceCapabilities.currentExtent.width,
					.height          = surfaceCapabilities.currentExtent.height,
					.layers          = 1
				}, NULL, &swapchainFramebuffers[i])) != VK_SUCCESS)
					exitVk("vkCreateFramebuffer");
			}

			vkUpdateDescriptorSets(device, 1, &(VkWriteDescriptorSet){
				.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet          = descriptorSet,
				.dstBinding      = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.pImageInfo      = &(VkDescriptorImageInfo){
					.imageView   = colorImageView,
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				}
			}, 0, NULL);
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
				case VK_LEFT:  case 'A': input |= INPUT_START_LEFT;    break;
				case VK_UP:    case 'W': input |= INPUT_START_FORWARD; break;
				case VK_RIGHT: case 'D': input |= INPUT_START_RIGHT;   break;
				case VK_DOWN:  case 'S': input |= INPUT_START_BACK;    break;
				case VK_SPACE:           input |= INPUT_START_UP;      break;
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
				case VK_LEFT:  case 'A': input |= INPUT_STOP_LEFT;    break;
				case VK_UP:    case 'W': input |= INPUT_STOP_FORWARD; break;
				case VK_RIGHT: case 'D': input |= INPUT_STOP_RIGHT;   break;
				case VK_DOWN:  case 'S': input |= INPUT_STOP_BACK;    break;
				case VK_SPACE:           input |= INPUT_STOP_UP;      break;
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

static void CALLBACK messageFiberProc(LPVOID lpParameter) {
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

static inline uint32_t generateChunk(VoxelChunk* chunk, uint32_t* faces) {
	uint32_t faceCount = 0;

	// for (uint32_t z = 0; z < 16; z++) {
	// 	for (uint32_t y = 0; y < 16; y++) {
	// 		for (uint32_t x = 0; x < 16; x++) {
	// 			uint8_t block = *chunk[x][y][z];

	// 			if (block == 0)
	// 				continue;

	// 			// if ()
	// 			faces[faceCount++] = 5;
	// 		}
	// 	}
	// }

	return faceCount;
}

static inline void drawScene(Vec3 cameraPosition, Vec3 xAxis, Vec3 yAxis, Vec3 zAxis, uint32_t recursionDepth, int32_t skipPortal) {
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

	if (recursionDepth < MAX_PORTAL_RECURSION) {
		for (uint32_t i = 0; i < ARRAY_COUNT(portals); i++) {
			if (i == skipPortal)
				continue;

			// todo: portal visibility test HERE
			Portal* portal = &portals[i];
			Portal* link = &portals[portal->link];

			Mat4 portalMVP = viewProjection * mat4FromRotationTranslationScale(portal->rotation, portal->translation, portal->scale);

			Quat q = quatMultiply(link->rotation, quatConjugate(portal->rotation));
			Vec3 portalCameraPosition = link->translation + vec3TransformQuat(cameraPosition - portal->translation, q);
			Vec3 x = vec3TransformQuat(xAxis, q);
			Vec3 y = vec3TransformQuat(yAxis, q);
			Vec3 z = vec3TransformQuat(zAxis, q);

			vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, recursionDepth);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_PORTAL_STENCIL]);
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &portalMVP);
			vkCmdDraw(commandBuffer, 6, 1, 0, 0);

			// todo: calculate clipping plane
			// Vec4 clippingPlane = { 0, 0, 0, 0 };
			// vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Vec4), &clippingPlane);
			drawScene(portalCameraPosition, x, y, z, recursionDepth + 1, portal->link);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_PORTAL_DEPTH]);
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &portalMVP);
			vkCmdDraw(commandBuffer, 6, 1, 0, 0);
		}
	}

	vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, recursionDepth);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_VOXEL]);
	vkCmdBindIndexBuffer(commandBuffer, buffers.index.buffer, BUFFER_OFFSET_INDEX_QUADS, VK_INDEX_TYPE_UINT16);

	// todo: loop over generated model matrices/aabbs and do culling
	for (uint32_t i = 0; i < 2; i++) {
		mvps[instanceIndex] = viewProjection * models[i];
		materials[instanceIndex] = (Material){ 127 + i * 50, 127, 127 - i * 50, 255 }; // materials[i];
		vkCmdDrawIndexed(commandBuffer, 36, 1, 0, 0, instanceIndex++);
	}

	// vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_TRIANGLE]);
	// vkCmdBindIndexBuffer(commandBuffer, buffers.index.buffer, BUFFER_OFFSET_INDEX_VERTICES, VK_INDEX_TYPE_UINT16);
	// mvps[instanceIndex] = viewProjection;
	// materials[instanceIndex] = (Material){ 167, 127, 17, 255 };
	// vkCmdDraw(commandBuffer, 3, 1, 0, instanceIndex++);

	// vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_PARTICLE]);
	// vkCmdDraw(commandBuffer, 1, 1, 0, 0);

	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &viewProjection);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_SKYBOX]);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

static const GUID CLSID_MMDeviceEnumerator = { 0xbcde0395, 0xe52f, 0x467c, { 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e } };
static const GUID IID_IMMDeviceEnumerator  = { 0xa95664d2, 0x9614, 0x4f35, { 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6 } };
static const GUID IID_IAudioClient         = { 0x1cb9ad4c, 0xdbfa, 0x4c32, { 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2 } };
static const GUID IID_IAudioRenderClient   = { 0xf294acfc, 0x3146, 0x4483, { 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2 } };

extern void abc(void);

void WinMainCRTStartup(void) {
	abc();

	portals[0].rotation = quatRotateY(portals[0].rotation, 0.3);
	portals[1].rotation = quatRotateY(portals[1].rotation, 0.6);

	VoxelChunk chunk;
	uint32_t faces[(16 * 16 * 16 * 4) / 2];
	generateChunk(&chunk, faces);

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

	WSADATA wsaData;
	int wsaError = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaError != 0)
		exitError("WSAStartup", wsaError);

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

	if (FAILED(hr = CoInitializeEx(NULL, COINIT_SPEED_OVER_MEMORY)))
		exitHRESULT("CoInitializeEx");

	IMMDeviceEnumerator* enumerator;
	IMMDevice* audioDevice;
	IAudioClient* audioClient;
	// IAudioRenderClient* audioRenderClient;
	WAVEFORMATEX waveFormat = {
		.wFormatTag      = WAVE_FORMAT_EXTENSIBLE,
		.nChannels       = 2,
		.nSamplesPerSec  = 480000,
		.wBitsPerSample  = sizeof(float) * 8,
		.nBlockAlign     = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels,
		.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign,
		.cbSize          = sizeof(WAVEFORMATEX)
	};
	// UINT32 bufferSize;

	if (FAILED(hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&enumerator)))
		exitHRESULT("CoCreateInstance");
	if (FAILED(hr = enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &audioDevice)))
		exitHRESULT("IMMDeviceEnumerator::GetDefaultAudioEndpoint");
	if (FAILED(hr = audioDevice->lpVtbl->Activate(audioDevice, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audioClient)))
		exitHRESULT("IMMDevice::Activate");
	// if (FAILED(hr = audioClient->lpVtbl->Initialize(audioClient, AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, &waveFormat, NULL)))
	// 	exitHRESULT("IAudioClient::Initialize");
	// if (FAILED(hr = audioClient->lpVtbl->GetService(audioClient, &IID_IAudioRenderClient, (void**)&audioRenderClient)))
	// 	exitHRESULT("IAudioClient::GetService");
	// if (FAILED(hr = audioClient->lpVtbl->GetBufferSize(audioClient, &bufferSize)))
	// 	exitHRESULT("IAudioClient::GetBufferSize");
	// if (FAILED(hr = audioClient->lpVtbl->Start(audioClient)))
	// 	exitHRESULT("IAudioClient::Start");

	vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
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
			.flags = i > 0 ? VK_FENCE_CREATE_SIGNALED_BIT : 0
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

	for (uint32_t i = 0; i < ARRAY_COUNT(quadIndices); i += 6) {
		quadIndices[i + 0] = (4 * (i / 6)) + 0;
		quadIndices[i + 1] = (4 * (i / 6)) + 1;
		quadIndices[i + 2] = (4 * (i / 6)) + 2;
		quadIndices[i + 3] = (4 * (i / 6)) + 0;
		quadIndices[i + 4] = (4 * (i / 6)) + 2;
		quadIndices[i + 5] = (4 * (i / 6)) + 3;
	}

	for (uint32_t i = 0; i < sizeof(buffers) / sizeof(Buffer); i++) {
		Buffer* b = (Buffer*)((char*)&buffers + (i * sizeof(Buffer)));
		if ((r = vkCreateBuffer(device, &(VkBufferCreateInfo){
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size  = b->size,
			.usage = b->usage
		}, NULL, &b->buffer)) != VK_SUCCESS)
			exitVk("vkCreateBuffer");

		vkGetBufferMemoryRequirements(device, b->buffer, &memoryRequirements);

		if ((r = vkAllocateMemory(device, &(VkMemoryAllocateInfo){
			.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext           = &(VkMemoryDedicatedAllocateInfo){
				.sType  = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
				.buffer = b->buffer
			},
			.allocationSize  = memoryRequirements.size,
			.memoryTypeIndex = getMemoryTypeIndex(b->requiredMemoryPropertyFlagBits, b->optionalMemoryPropertyFlagBits)
		}, NULL, &b->deviceMemory)) != VK_SUCCESS)
			exitVk("vkAllocateMemory");

		if ((r = vkBindBufferMemory(device, b->buffer, b->deviceMemory, 0)) != VK_SUCCESS)
			exitVk("vkBindImageMemory");

		if (b->requiredMemoryPropertyFlagBits & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			if ((r = vkMapMemory(device, b->deviceMemory, 0, VK_WHOLE_SIZE, 0, &b->data)) != VK_SUCCESS)
				exitVk("vkMapMemory");
		}
	}

	memcpy(buffers.staging.data, vertexPositions, sizeof(vertexPositions));
	memcpy(buffers.staging.data + BUFFER_RANGE_VERTEX_POSITIONS, vertexIndices, sizeof(vertexIndices));
	memcpy(buffers.staging.data + BUFFER_RANGE_VERTEX_POSITIONS + BUFFER_RANGE_INDEX_VERTICES, quadIndices, sizeof(quadIndices));

	if ((r = vkBeginCommandBuffer(commandBuffers[frame], &(VkCommandBufferBeginInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	})) != VK_SUCCESS)
		exitVk("vkBeginCommandBuffer");

	vkCmdCopyBuffer(commandBuffers[frame], buffers.staging.buffer, buffers.vertex.buffer, 1, &(VkBufferCopy){
		.srcOffset = 0,
		.dstOffset = BUFFER_OFFSET_VERTEX_POSITIONS,
		.size      = BUFFER_RANGE_VERTEX_POSITIONS
	});
	vkCmdCopyBuffer(commandBuffers[frame], buffers.staging.buffer, buffers.index.buffer, 2, (VkBufferCopy[]){
		{
			.srcOffset = BUFFER_RANGE_VERTEX_POSITIONS,
			.dstOffset = BUFFER_OFFSET_INDEX_VERTICES,
			.size      = BUFFER_RANGE_INDEX_VERTICES
		}, {
			.srcOffset = BUFFER_RANGE_VERTEX_POSITIONS + BUFFER_RANGE_INDEX_VERTICES,
			.dstOffset = BUFFER_OFFSET_INDEX_QUADS,
			.size      = BUFFER_RANGE_INDEX_QUADS
		}
	});

	if ((r = vkEndCommandBuffer(commandBuffers[frame])) != VK_SUCCESS)
		exitVk("vkEndCommandBuffer");
	if ((r = vkQueueSubmit(queue, 1, &(VkSubmitInfo){
		.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers    = &commandBuffers[frame]
	}, fences[frame])) != VK_SUCCESS)
		exitVk("vkQueueSubmit");

	if ((r = vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo){
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount         = 1,
		.pSetLayouts            = &descriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &(VkPushConstantRange){
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset     = 0,
			.size       = sizeof(Mat4)
		}
	}, NULL, &pipelineLayout)) != VK_SUCCESS)
		exitVk("vkCreatePipelineLayout");

	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = blit_vert,
		.codeSize = sizeof(blit_vert)
	}, NULL, &blitVert)) != VK_SUCCESS)
		exitVk("vkCreateShaderModule");
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = blit_frag,
		.codeSize = sizeof(blit_frag)
	}, NULL, &blitFrag)) != VK_SUCCESS)
		exitVk("vkCreateShaderModule");
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
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = skybox_vert,
		.codeSize = sizeof(skybox_vert)
	}, NULL, &skyboxVert)) != VK_SUCCESS)
		exitVk("vkCreateShaderModule");
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = skybox_frag,
		.codeSize = sizeof(skybox_frag)
	}, NULL, &skyboxFrag)) != VK_SUCCESS)
		exitVk("vkCreateShaderModule");
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = particle_vert,
		.codeSize = sizeof(particle_vert)
	}, NULL, &particleVert)) != VK_SUCCESS)
		exitVk("vkCreateShaderModule");
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = particle_frag,
		.codeSize = sizeof(particle_frag)
	}, NULL, &particleFrag)) != VK_SUCCESS)
		exitVk("vkCreateShaderModule");
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = portal_vert,
		.codeSize = sizeof(portal_vert)
	}, NULL, &portalVert)) != VK_SUCCESS)
		exitVk("vkCreateShaderModule");
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = portal_frag,
		.codeSize = sizeof(portal_frag)
	}, NULL, &portalFrag)) != VK_SUCCESS)
		exitVk("vkCreateShaderModule");
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = voxel_vert,
		.codeSize = sizeof(voxel_vert)
	}, NULL, &voxelVert)) != VK_SUCCESS)
		exitVk("vkCreateShaderModule");

	VkPipelineShaderStageCreateInfo shaderStagesBlit[] = {
		{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_VERTEX_BIT,
			.module = blitVert,
			.pName  = "main"
		}, {
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = blitFrag,
			.pName  = "main"
		}
	};
	VkPipelineShaderStageCreateInfo shaderStagesTriangle[] = {
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
	VkPipelineShaderStageCreateInfo shaderStagesSkybox[] = {
		{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_VERTEX_BIT,
			.module = skyboxVert,
			.pName  = "main"
		}, {
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = skyboxFrag,
			.pName  = "main"
		}
	};
	VkPipelineShaderStageCreateInfo shaderStagesParticle[] = {
		{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_VERTEX_BIT,
			.module = particleVert,
			.pName  = "main"
		}, {
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = particleFrag,
			.pName  = "main"
		}
	};
	VkPipelineShaderStageCreateInfo shaderStagesPortal[] = {
		{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_VERTEX_BIT,
			.module = portalVert,
			.pName  = "main"
		}, {
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = portalFrag,
			.pName  = "main"
		}
	};
	VkPipelineShaderStageCreateInfo shaderStagesVoxel[] = {
		{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_VERTEX_BIT,
			.module = voxelVert,
			.pName  = "main"
		}, {
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = triangleFrag,
			.pName  = "main"
		}
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateNone = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateTriangle = {
		.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount   = 3,
		.pVertexBindingDescriptions      = (VkVertexInputBindingDescription[]){
			{
				.binding   = 0,
				.stride    = sizeof(float) * 3,
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			}, {
				.binding   = 2,
				.stride    = sizeof(Mat4),
				.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
			}, {
				.binding   = 3,
				.stride    = sizeof(Material),
				.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
			}
		},
		.vertexAttributeDescriptionCount = 6,
		.pVertexAttributeDescriptions    = (VkVertexInputAttributeDescription[]){
			{
				.location = 0,
				.binding  = 0,
				.format   = VK_FORMAT_R32G32B32_SFLOAT,
				.offset   = 0
			}, {
				.location = 4,
				.binding  = 2,
				.format   = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset   = 0
			}, {
				.location = 5,
				.binding  = 2,
				.format   = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset   = 4 * sizeof(float)
			}, {
				.location = 6,
				.binding  = 2,
				.format   = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset   = 8 * sizeof(float)
			}, {
				.location = 7,
				.binding  = 2,
				.format   = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset   = 12 * sizeof(float)
			}, {
				.location = 8,
				.binding  = 3,
				.format   = VK_FORMAT_R8G8B8A8_UNORM,
				.offset   = 0
			}
		}
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateVoxel = {
		.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount   = 2,
		.pVertexBindingDescriptions      = (VkVertexInputBindingDescription[]){
			{
				.binding   = 2,
				.stride    = sizeof(Mat4),
				.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
			}, {
				.binding   = 3,
				.stride    = sizeof(Material),
				.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
			}
		},
		.vertexAttributeDescriptionCount = 5,
		.pVertexAttributeDescriptions    = (VkVertexInputAttributeDescription[]){
			{
				.location = 4,
				.binding  = 2,
				.format   = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset   = 0
			}, {
				.location = 5,
				.binding  = 2,
				.format   = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset   = 4 * sizeof(float)
			}, {
				.location = 6,
				.binding  = 2,
				.format   = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset   = 8 * sizeof(float)
			}, {
				.location = 7,
				.binding  = 2,
				.format   = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset   = 12 * sizeof(float)
			}, {
				.location = 8,
				.binding  = 3,
				.format   = VK_FORMAT_R8G8B8A8_UNORM,
				.offset   = 0
			}
		}
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateParticle = {
		.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount   = 1,
		.pVertexBindingDescriptions      = (VkVertexInputBindingDescription[]){
			{
				.binding   = 1,
				.stride    = sizeof(float) * 3,
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			}
		},
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions    = (VkVertexInputAttributeDescription[]){
			{
				.location = 0,
				.binding  = 1,
				.format   = VK_FORMAT_R32G32B32_SFLOAT,
				.offset   = 0
			}
		}
	};
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
		.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParticle = {
		.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST
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
	VkPipelineRasterizationStateCreateInfo rasterizationStatePortal = {
		.sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_TRUE,
		.lineWidth        = 1.f
	};
	VkPipelineMultisampleStateCreateInfo multisampleState = {
		.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilStateTriangle = {
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable   = VK_TRUE,
		.depthWriteEnable  = VK_TRUE,
		.depthCompareOp    = VK_COMPARE_OP_GREATER_OR_EQUAL,
		.stencilTestEnable = VK_TRUE,
		.front             = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask   = 0,
			.reference   = 0
		},
		.back              = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask   = 0,
			.reference   = 0
		}
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilStateNone = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilStateSkybox = {
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable   = VK_TRUE,
		.depthCompareOp    = VK_COMPARE_OP_EQUAL,
		.stencilTestEnable = VK_TRUE,
		.front             = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask   = 0,
			.reference   = 0
		},
		.back              = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask   = 0,
			.reference   = 0
		}
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilStatePortalStencil = {
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable   = VK_TRUE,
		.depthCompareOp    = VK_COMPARE_OP_GREATER_OR_EQUAL,
		.stencilTestEnable = VK_TRUE,
		.front             = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask   = 0xFF,
			.reference   = 0
		},
		.back              = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask   = 0xFF,
			.reference   = 0
		}
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilStatePortalDepth = {
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable   = VK_TRUE,
		.depthWriteEnable  = VK_TRUE,
		.depthCompareOp    = VK_COMPARE_OP_GREATER_OR_EQUAL,
		.stencilTestEnable = VK_TRUE,
		.front             = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask   = 0xFF,
			.reference   = 0
		},
		.back              = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask   = 0xFF,
			.reference   = 0
		}
	};
	VkPipelineColorBlendStateCreateInfo colorBlendState = {
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments    = &(VkPipelineColorBlendAttachmentState){
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		}
	};
	VkPipelineColorBlendStateCreateInfo colorBlendStatePortal = {
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments    = &(VkPipelineColorBlendAttachmentState){
			.colorWriteMask = 0
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
	VkPipelineDynamicStateCreateInfo dynamicStateTriangle = {
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 3,
		.pDynamicStates    = (VkDynamicState[]){
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_STENCIL_REFERENCE
		}
	};
	VkPipelineDynamicStateCreateInfo dynamicStatePortal = {
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 3,
		.pDynamicStates    = (VkDynamicState[]){
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_STENCIL_REFERENCE
		}
	};

	if ((r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, GRAPHICS_PIPELINE_MAX, (VkGraphicsPipelineCreateInfo[]){
		[GRAPHICS_PIPELINE_BLIT] = {
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount          = ARRAY_COUNT(shaderStagesBlit),
			.pStages             = shaderStagesBlit,
			.pVertexInputState   = &vertexInputStateNone,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState   = &multisampleState,
			.pDepthStencilState  = &depthStencilStateNone,
			.pColorBlendState    = &colorBlendState,
			.pDynamicState       = &dynamicState,
			.layout              = pipelineLayout,
			.renderPass          = renderPassBlit
		}, [GRAPHICS_PIPELINE_TRIANGLE] = {
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount          = ARRAY_COUNT(shaderStagesTriangle),
			.pStages             = shaderStagesTriangle,
			.pVertexInputState   = &vertexInputStateTriangle,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState   = &multisampleState,
			.pDepthStencilState  = &depthStencilStateTriangle,
			.pColorBlendState    = &colorBlendState,
			.pDynamicState       = &dynamicStateTriangle,
			.layout              = pipelineLayout,
			.renderPass          = renderPass
		}, [GRAPHICS_PIPELINE_SKYBOX] = {
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount          = ARRAY_COUNT(shaderStagesSkybox),
			.pStages             = shaderStagesSkybox,
			.pVertexInputState   = &vertexInputStateNone,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState   = &multisampleState,
			.pDepthStencilState  = &depthStencilStateSkybox,
			.pColorBlendState    = &colorBlendState,
			.pDynamicState       = &dynamicStateTriangle,
			.layout              = pipelineLayout,
			.renderPass          = renderPass
		}, [GRAPHICS_PIPELINE_PARTICLE] = {
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount          = ARRAY_COUNT(shaderStagesParticle),
			.pStages             = shaderStagesParticle,
			.pVertexInputState   = &vertexInputStateParticle,
			.pInputAssemblyState = &inputAssemblyStateParticle,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState   = &multisampleState,
			.pDepthStencilState  = &depthStencilStateTriangle,
			.pColorBlendState    = &colorBlendState,
			.pDynamicState       = &dynamicState,
			.layout              = pipelineLayout,
			.renderPass          = renderPass
		}, [GRAPHICS_PIPELINE_PORTAL_STENCIL] = {
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount          = ARRAY_COUNT(shaderStagesPortal),
			.pStages             = shaderStagesPortal,
			.pVertexInputState   = &vertexInputStateNone,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizationStatePortal,
			.pMultisampleState   = &multisampleState,
			.pDepthStencilState  = &depthStencilStatePortalStencil,
			.pColorBlendState    = &colorBlendStatePortal,
			.pDynamicState       = &dynamicStatePortal,
			.layout              = pipelineLayout,
			.renderPass          = renderPass
		}, [GRAPHICS_PIPELINE_PORTAL_DEPTH] = {
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount          = ARRAY_COUNT(shaderStagesPortal),
			.pStages             = shaderStagesPortal,
			.pVertexInputState   = &vertexInputStateNone,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizationStatePortal,
			.pMultisampleState   = &multisampleState,
			.pDepthStencilState  = &depthStencilStatePortalDepth,
			.pColorBlendState    = &colorBlendStatePortal,
			.pDynamicState       = &dynamicStatePortal,
			.layout              = pipelineLayout,
			.renderPass          = renderPass
		}, [GRAPHICS_PIPELINE_VOXEL] = {
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount          = ARRAY_COUNT(shaderStagesVoxel),
			.pStages             = shaderStagesVoxel,
			.pVertexInputState   = &vertexInputStateVoxel,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState   = &multisampleState,
			.pDepthStencilState  = &depthStencilStateTriangle,
			.pColorBlendState    = &colorBlendState,
			.pDynamicState       = &dynamicStateTriangle,
			.layout              = pipelineLayout,
			.renderPass          = renderPass
		}
	}, NULL, graphicsPipelines)) != VK_SUCCESS)
		exitVk("vkCreateGraphicsPipelines");

	// if ((r = vkWaitForFences(device, 1, &fences[frame], VK_FALSE, UINT64_MAX)) != VK_SUCCESS)
	// 	exitVk("vkWaitForFences");
	// vkDestroyBuffer(device, buffers.staging.buffer, NULL);
	// vkFreeMemory(device, buffers.staging.deviceMemory, NULL);

	for (;;) {
		SwitchToFiber(messageFiber);

		float sy = sinf(cameraYaw);
		float cy = cosf(cameraYaw);
		float sp = sinf(cameraPitch);
		float cp = cosf(cameraPitch);

		char buff[1024];
		struct sockaddr_in clientAddr;
		int n;
		while (({
			n = recvfrom(sock, buff, sizeof(buff), 0, (struct sockaddr*)&clientAddr, &(int){ sizeof(struct sockaddr_in) });
			if (n == SOCKET_ERROR) {
				int err = WSAGetLastError();
				if (err != WSAEWOULDBLOCK)
					exitWSA("recvfrom");
			}
			n > 0;
		})) {
			char str[INET_ADDRSTRLEN];
			if (inet_ntop(AF_INET, &clientAddr.sin_addr, str, sizeof(str)) == NULL)
				exitWSA("inet_ntop");

			char ABC[1024];
			sprintf(ABC, "Received %d bytes from %s:%d\n%s\n", n, str, ntohs(clientAddr.sin_port), buff);
			MessageBoxA(NULL, ABC, NULL, MB_OK);
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

			if ((input & INPUT_START_FORWARD) && !(input & INPUT_START_BACK))
				forward = 1.f;
			else if ((input & INPUT_START_BACK) && !(input & INPUT_START_FORWARD))
				forward = -1.f;

			if ((input & INPUT_START_LEFT) && !(input & INPUT_START_RIGHT))
				strafe = -1.f;
			else if ((input & INPUT_START_RIGHT) && !(input & INPUT_START_LEFT))
				strafe = 1.f;

			if ((input & INPUT_START_UP) && !(input & INPUT_START_DOWN))
				player.translation.y += 0.09f;
			else if ((input & INPUT_START_DOWN) && !(input & INPUT_START_UP))
				player.translation.y -= 0.09f;

			if (input & INPUT_STOP_FORWARD) input &= ~(INPUT_START_FORWARD | INPUT_STOP_FORWARD);
			if (input & INPUT_STOP_BACK)    input &= ~(INPUT_START_BACK | INPUT_STOP_BACK);
			if (input & INPUT_STOP_LEFT)    input &= ~(INPUT_START_LEFT | INPUT_STOP_LEFT);
			if (input & INPUT_STOP_RIGHT)   input &= ~(INPUT_START_RIGHT | INPUT_STOP_RIGHT);
			if (input & INPUT_STOP_DOWN)    input &= ~(INPUT_START_DOWN | INPUT_STOP_DOWN);
			if (input & INPUT_STOP_UP)      input &= ~(INPUT_START_UP | INPUT_STOP_UP);

			Vec3 forwardMovement  = { forward * -sy, 0, forward * -cy };
			Vec3 sidewaysMovement = { strafe * cy, 0, strafe * -sy };

			player.translation += 0.09f * forwardMovement;
			player.translation += 0.09f * sidewaysMovement;

			ticksElapsed++;
		}

		commandBuffer    = commandBuffers[frame];
		instanceIndex    = 0;
		mvps             = buffers.instance.data + BUFFER_OFFSET_INSTANCE_MVPS + (frame * BUFFER_RANGE_INSTANCE_MVPS);
		materials        = buffers.instance.data + BUFFER_OFFSET_INSTANCE_MATERIALS + (frame * BUFFER_RANGE_INSTANCE_MATERIALS);

		Vec3 xAxis     = { cy, 0, -sy };
		Vec3 yAxis     = { sy * sp, cp, cy * sp };
		Vec3 zAxis     = { sy * cp, -sp, cp * cy };
		cameraPosition = player.translation; // todo: inter/extrapolate between frames

		models[0] = mat4FromRotationTranslationScale(quatIdentity(), (Vec3){ -20.f, 0.f, 0.f }, (Vec3){ 20.f, 1.f, 20.f });
		models[1] = mat4FromRotationTranslationScale(quatIdentity(), (Vec3){  20.f, 0.f, 0.f }, (Vec3){ 20.f, 1.f, 20.f });

		uint32_t imageIndex;
		if ((r = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquireSemaphores[frame], VK_NULL_HANDLE, &imageIndex)) != VK_SUCCESS)
			exitVk("vkAcquireNextImageKHR");
		if ((r = vkWaitForFences(device, 1, &fences[frame], VK_TRUE, UINT64_MAX)) != VK_SUCCESS)
			exitVk("vkWaitForFences");
		if ((r = vkResetFences(device, 1, &fences[frame])) != VK_SUCCESS)
			exitVk("vkResetFences");
		if ((r = vkResetCommandPool(device, commandPools[frame], 0)) != VK_SUCCESS)
			exitVk("vkResetCommandPool");
		if ((r = vkBeginCommandBuffer(commandBuffer, &(VkCommandBufferBeginInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		})) != VK_SUCCESS)
			exitVk("vkBeginCommandBuffer");

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 1, &(uint32_t){ 0 });
		vkCmdBindVertexBuffers(commandBuffer, 0, 4, (VkBuffer[]){
			buffers.vertex.buffer,
			buffers.particles.buffer,
			buffers.instance.buffer,
			buffers.instance.buffer
		}, (VkDeviceSize[]){
			BUFFER_OFFSET_VERTEX_POSITIONS,
			BUFFER_OFFSET_PARTICLES + (frame * BUFFER_RANGE_PARTICLES),
			BUFFER_OFFSET_INSTANCE_MVPS + (frame * BUFFER_RANGE_INSTANCE_MVPS),
			BUFFER_OFFSET_INSTANCE_MATERIALS + (frame * BUFFER_RANGE_INSTANCE_MATERIALS)
		});

		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo){
			.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass      = renderPass,
			.framebuffer     = framebuffer,
			.renderArea      = scissor,
			.clearValueCount = 2,
			.pClearValues    = (VkClearValue[]){ { 0 }, { 0 } }
		}, VK_SUBPASS_CONTENTS_INLINE);

		drawScene(cameraPosition, xAxis, yAxis, zAxis, 0, -1);

		vkCmdEndRenderPass(commandBuffer);

		vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo){
			.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass      = renderPassBlit,
			.framebuffer     = swapchainFramebuffers[imageIndex],
			.renderArea      = scissor,
			.clearValueCount = 1,
			.pClearValues    = &(VkClearValue){ { 0 } }
		}, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_BLIT]);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);

		if ((r = vkEndCommandBuffer(commandBuffer)) != VK_SUCCESS)
			exitVk("vkEndCommandBuffer");
		if ((r = vkQueueSubmit(queue, 1, &(VkSubmitInfo){
			.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount   = 1,
			.pWaitSemaphores      = &imageAcquireSemaphores[frame],
			.pWaitDstStageMask    = &(VkPipelineStageFlags){ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
			.commandBufferCount   = 1,
			.pCommandBuffers      = &commandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores    = &semaphores[frame]
		}, fences[frame])) != VK_SUCCESS)
			exitVk("vkQueueSubmit");
		if ((r = vkQueuePresentKHR(queue, &(VkPresentInfoKHR){
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
