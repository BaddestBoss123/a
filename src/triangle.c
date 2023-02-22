#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "vcruntime.lib")
#pragma comment(lib, "ucrt.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "dwrite.lib")

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
#include "assets.h"

#include "blit.frag.h"
#include "triangle.vert.h"
#include "triangle.frag.h"
#include "skybox.vert.h"
#include "skybox.frag.h"
#include "particle.vert.h"
#include "particle.frag.h"
#include "portal.vert.h"
#include "voxel.vert.h"

INCBIN(shaders_spv, "shaders.spv");

#define F(fn) static PFN_##fn fn;
	BEFORE_INSTANCE_FUNCS(F)
	INSTANCE_FUNCS(F)
	DEVICE_FUNCS(F)
#undef F

enum Input {
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
};

enum GraphicsPipeline {
	GRAPHICS_PIPELINE_BLIT,
	GRAPHICS_PIPELINE_TRIANGLE,
	GRAPHICS_PIPELINE_SKYBOX,
	GRAPHICS_PIPELINE_PARTICLE,
	GRAPHICS_PIPELINE_PORTAL_STENCIL,
	GRAPHICS_PIPELINE_PORTAL_DEPTH,
	GRAPHICS_PIPELINE_VOXEL,
	GRAPHICS_PIPELINE_MAX
};

struct Buffer {
	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
	void* data;
	VkDeviceSize size;
	VkBufferUsageFlags usage;
	VkMemoryPropertyFlags requiredMemoryPropertyFlagBits;
	VkMemoryPropertyFlags optionalMemoryPropertyFlagBits;
};

struct Transform {
	Vec3 translation;
	Quat rotation;
	Vec3 scale;
};

struct Entity {
	struct Transform;
	struct Mesh* mesh;
	struct Entity** children;
	uint32_t childCount;
};

struct Portal {
	struct Transform;
	uint32_t link;
};

typedef uint8_t VoxelChunk[16][16][16];

static uint16_t vertexIndices[] = { 0, 1, 2 };
static float vertexPositions[] = {
	0.0, -0.5, 0.0,
	0.5, 0.5, 0.0,
	-0.5, 0.5, 0.0
};
static uint16_t quadIndices[48];

enum BufferRanges {
	BUFFER_RANGE_INDEX_VERTICES     = ALIGN_FORWARD(sizeof(vertexIndices), 256),
	BUFFER_RANGE_INDEX_QUADS        = ALIGN_FORWARD(sizeof(quadIndices), 256),
	BUFFER_RANGE_VERTEX_POSITIONS   = ALIGN_FORWARD(sizeof(vertexPositions), 256),
	BUFFER_RANGE_PARTICLES          = ALIGN_FORWARD(MAX_PARTICLES, 256),
	BUFFER_RANGE_INSTANCE_MVPS      = ALIGN_FORWARD(MAX_INSTANCES * sizeof(Mat4), 256),
	BUFFER_RANGE_INSTANCE_MATERIALS = ALIGN_FORWARD(MAX_INSTANCES * sizeof(struct Material), 256)
};

enum BufferOffsets {
	BUFFER_OFFSET_INDEX_VERTICES     = 0,
	BUFFER_OFFSET_INDEX_QUADS        = 0,
	BUFFER_OFFSET_VERTEX_POSITIONS   = 0,
	BUFFER_OFFSET_PARTICLES          = 0,
	BUFFER_OFFSET_INSTANCE_MVPS      = FRAMES_IN_FLIGHT * 0,
	BUFFER_OFFSET_INSTANCE_MATERIALS = FRAMES_IN_FLIGHT * BUFFER_RANGE_INSTANCE_MVPS
};

static struct {
	struct Buffer staging;
	struct Buffer index;
	struct Buffer vertex;
	struct Buffer particles;
	struct Buffer instance;
	struct Buffer uniform;
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

static VkDevice device;
static VkPhysicalDevice physicalDevice;
static VkPhysicalDeviceFeatures physicalDeviceFeatures;
static VkPhysicalDeviceProperties physicalDeviceProperties;
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
static VkShaderModule blitFrag;
static VkShaderModule triangleVert;
static VkShaderModule triangleFrag;
static VkShaderModule skyboxVert;
static VkShaderModule skyboxFrag;
static VkShaderModule particleVert;
static VkShaderModule particleFrag;
static VkShaderModule portalVert;
static VkShaderModule voxelVert;
static VkViewport viewport = {
	.minDepth = 0.0,
	.maxDepth = 1.0
};

static HMODULE hInstance;
static LPVOID mainFiber;
static uint64_t ticksElapsed;
static uint64_t input;

static struct Entity player;
static Mat4 projectionMatrix;
static Vec3 cameraPosition;
static float cameraYaw;
static float cameraPitch;
static const float cameraFieldOfView = 0.78f;
static const float cameraNear        = 0.1f;

static struct Portal portals[] = {
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

static struct Entity tree = {
	.translation = { -20.f, 2.f, 5.f },
};

static VkCommandBuffer commandBuffer;
static uint32_t instanceIndex;
static Mat4 models[MAX_INSTANCES];
static Mat4* mvps;
static struct Material* materials;

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
			RegisterRawInputDevices(&(RAWINPUTDEVICE){
				.usUsagePage = HID_USAGE_PAGE_GENERIC,
				.usUsage     = HID_USAGE_GENERIC_MOUSE,
				.dwFlags     = 0,
				.hwndTarget  = hWnd
			}, 1, sizeof(RAWINPUTDEVICE));

			PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(LoadLibraryW(L"vulkan-1.dll"), "vkGetInstanceProcAddr");

			#define F(fn) fn = (PFN_##fn)vkGetInstanceProcAddr(NULL, #fn);
			BEFORE_INSTANCE_FUNCS(F)
			#undef F

			VkInstance instance;
			vkCreateInstance(&(VkInstanceCreateInfo){
				.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pApplicationInfo        = &(VkApplicationInfo){
					.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
					// .pApplicationName   = "triangle",
					// .applicationVersion = 1,
					// .pEngineName        = "Extreme Engine",
					// .engineVersion      = 1,
					.apiVersion = VK_API_VERSION_1_1 // STUPID VALIDATION LAYER
				},
				.enabledExtensionCount   = 2,
				.ppEnabledExtensionNames = (const char*[]){
					VK_KHR_SURFACE_EXTENSION_NAME,
					VK_KHR_WIN32_SURFACE_EXTENSION_NAME
				}
			}, NULL, &instance);

			#define F(fn) fn = (PFN_##fn)vkGetInstanceProcAddr(instance, #fn);
			INSTANCE_FUNCS(F)
			#undef F

			vkCreateWin32SurfaceKHR(instance, &(VkWin32SurfaceCreateInfoKHR){
				.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
				.hinstance = hInstance,
				.hwnd      = hWnd
			}, NULL, &surface);

			vkEnumeratePhysicalDevices(instance, &(uint32_t){ 1 }, &physicalDevice);
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
			vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
			vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

			VkSurfaceFormatKHR surfaceFormats[16];
			uint32_t surfaceFormatCount = ARRAY_COUNT(surfaceFormats);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats);

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
					vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

					if (presentSupport) {
						queueFamilyIndex = i;
						break;
					}
				}
			}

			vkCreateDevice(physicalDevice, &(VkDeviceCreateInfo){
				.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.queueCreateInfoCount    = 1,
				.pQueueCreateInfos       = &(VkDeviceQueueCreateInfo){
					.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = queueFamilyIndex,
					.queueCount       = 1,
					.pQueuePriorities = &(float){ 1.f }
				},
				.enabledExtensionCount   = 1,
				.ppEnabledExtensionNames = (const char*[]){ VK_KHR_SWAPCHAIN_EXTENSION_NAME },
				.pEnabledFeatures        = &(VkPhysicalDeviceFeatures){
					.textureCompressionBC = physicalDeviceFeatures.textureCompressionBC,
					.depthClamp           = physicalDeviceFeatures.depthClamp,
					.shaderClipDistance   = physicalDeviceFeatures.shaderClipDistance,
					.samplerAnisotropy    = physicalDeviceFeatures.samplerAnisotropy
				}
			}, NULL, &device);

			#define F(fn) fn = (PFN_##fn)vkGetDeviceProcAddr(device, #fn);
			DEVICE_FUNCS(F)
			#undef F

			vkCreateRenderPass(device, &(VkRenderPassCreateInfo){
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
			}, NULL, &renderPass);

			vkCreateRenderPass(device, &(VkRenderPassCreateInfo){
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
			}, NULL, &renderPassBlit);

			VkSampler samplers[1];
			vkCreateSampler(device, &(VkSamplerCreateInfo){
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO
			}, NULL, &samplers[0]);

			vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo){
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
			}, NULL, &descriptorSetLayout);

			vkCreateDescriptorPool(device, &(VkDescriptorPoolCreateInfo){
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
			}, NULL, &descriptorPool);

			vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo){
				.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool     = descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts        = &descriptorSetLayout
			}, &descriptorSet);
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
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

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

			vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain);

			if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE) {
				vkDeviceWaitIdle(device);

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
			vkGetSwapchainImagesKHR(device, swapchain, &swapchainImagesCount, swapchainImages);

			vkCreateImage(device, &(VkImageCreateInfo){
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
			}, NULL, &colorImage);

			vkGetImageMemoryRequirements(device, colorImage, &memoryRequirements);
			vkAllocateMemory(device, &(VkMemoryAllocateInfo){
				.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext           = &(VkMemoryDedicatedAllocateInfo){
					.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
					.image = colorImage
				},
				.allocationSize  = memoryRequirements.size,
				.memoryTypeIndex = getMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0)
			}, NULL, &colorDeviceMemory);
			vkBindImageMemory(device, colorImage, colorDeviceMemory, 0);
			vkCreateImageView(device, &(VkImageViewCreateInfo){
				.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image            = colorImage,
				.viewType         = VK_IMAGE_VIEW_TYPE_2D,
				.format           = VK_FORMAT_R16G16B16A16_SFLOAT,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1
				}
			}, NULL, &colorImageView);
			vkCreateImage(device, &(VkImageCreateInfo){
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
			}, NULL, &depthImage);

			vkGetImageMemoryRequirements(device, depthImage, &memoryRequirements);
			vkAllocateMemory(device, &(VkMemoryAllocateInfo){
				.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext           = &(VkMemoryDedicatedAllocateInfo){
					.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
					.image = depthImage
				},
				.allocationSize  = memoryRequirements.size,
				.memoryTypeIndex = getMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0)
			}, NULL, &depthDeviceMemory);
			vkBindImageMemory(device, depthImage, depthDeviceMemory, 0);
			vkCreateImageView(device, &(VkImageViewCreateInfo){
				.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image            = depthImage,
				.viewType         = VK_IMAGE_VIEW_TYPE_2D,
				.format           = VK_FORMAT_D24_UNORM_S8_UINT,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
					.levelCount = 1,
					.layerCount = 1
				}
			}, NULL, &depthImageView);

			vkCreateFramebuffer(device, &(VkFramebufferCreateInfo){
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
			}, NULL, &framebuffer);

			for (uint32_t i = 0; i < swapchainImagesCount; i++) {
				vkCreateImageView(device, &(VkImageViewCreateInfo){
					.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image            = swapchainImages[i],
					.viewType         = VK_IMAGE_VIEW_TYPE_2D,
					.format           = surfaceFormat.format,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1
					}
				}, NULL, &swapchainImageViews[i]);

				vkCreateFramebuffer(device, &(VkFramebufferCreateInfo){
					.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
					.renderPass      = renderPassBlit,
					.attachmentCount = 1,
					.pAttachments    = &swapchainImageViews[i],
					.width           = surfaceCapabilities.currentExtent.width,
					.height          = surfaceCapabilities.currentExtent.height,
					.layers          = 1
				}, NULL, &swapchainFramebuffers[i]);
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
			DestroyWindow(hWnd);
		} break;
		case WM_GETMINMAXINFO: {
			((MINMAXINFO*)lParam)->ptMinTrackSize.x = 640;
			((MINMAXINFO*)lParam)->ptMinTrackSize.y = 360;
		} break;
		case WM_INPUT: {
			RAWINPUT rawInput;
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &rawInput, &(UINT){ sizeof(RAWINPUT) }, sizeof(RAWINPUTHEADER));

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

					if (style & WS_OVERLAPPEDWINDOW) {
						MONITORINFO current = { sizeof(MONITORINFO) };
						GetWindowPlacement(hWnd, &previous);
						GetMonitorInfoW(MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY), &current);
						SetWindowLongPtrW(hWnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
						SetWindowPos(hWnd, HWND_TOP, current.rcMonitor.left, current.rcMonitor.top, current.rcMonitor.right - current.rcMonitor.left, current.rcMonitor.bottom - current.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
					} else {
						SetWindowPlacement(hWnd, &previous);
						SetWindowLongPtrW(hWnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
						SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
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
			SetTimer(hWnd, 1, 1, NULL);
		} break;
		case WM_EXITMENULOOP:
		case WM_EXITSIZEMOVE: {
			KillTimer(hWnd, 1);
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

static inline void drawScene(Vec3 cameraPosition, Vec3 xAxis, Vec3 yAxis, Vec3 zAxis, Vec4 clippingPlane, uint32_t recursionDepth, int32_t skipPortal) {
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

			// todo: portal visibility test here
			struct Portal* portal = &portals[i];
			struct Portal* link = &portals[portal->link];

			Mat4 portalMVP = viewProjection * mat4FromRotationTranslationScale(portal->rotation, portal->translation, portal->scale);

			Quat q = quatMultiply(link->rotation, quatConjugate(portal->rotation));
			Vec3 portalCameraPosition = link->translation + vec3TransformQuat(cameraPosition - portal->translation, q);
			Vec3 x = vec3TransformQuat(xAxis, q);
			Vec3 y = vec3TransformQuat(yAxis, q);
			Vec3 z = vec3TransformQuat(zAxis, q);

			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &portalMVP);

			vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, recursionDepth);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_PORTAL_STENCIL]);
			vkCmdDraw(commandBuffer, 6, 1, 0, 0);

			Vec3 normal = vec3Normalize(vec3TransformQuat((Vec3){ 0.f, 0.f, 1.f }, link->rotation));
			float distance = -vec3Dot(link->translation, normal);
			Vec4 newClippingPlane = { normal.x, normal.y, normal.z, distance };

			Vec3 normalA = vec3Normalize(vec3TransformQuat((Vec3){ 0.f, 0.f, 1.f }, portal->rotation));
			if (vec3Dot(normalA, cameraPosition) > vec3Dot(portal->translation, normalA))
				newClippingPlane *= -1.f;

			drawScene(portalCameraPosition, x, y, z, newClippingPlane, recursionDepth + 1, portal->link);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_PORTAL_DEPTH]);
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &portalMVP);
			vkCmdDraw(commandBuffer, 6, 1, 0, 0);
		}
	}

	vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, recursionDepth);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_VOXEL]);
	vkCmdBindIndexBuffer(commandBuffer, buffers.index.buffer, BUFFER_OFFSET_INDEX_QUADS, VK_INDEX_TYPE_UINT16);

	struct Test {
		Mat4 viewProjection;
		Vec4 clippingPlane;
	} test = {
		.viewProjection = viewProjection,
		.clippingPlane = clippingPlane
	};
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(test), &test);

	// todo: culling
	for (uint32_t i = 0; i < 2; i++) {
		mvps[instanceIndex] = models[i]; // viewProjection * models[i];
		materials[instanceIndex] = (struct Material){ 127 + i * 50, 127, 127 - i * 50, 255 }; // materials[i];
		vkCmdDrawIndexed(commandBuffer, 36, 1, 0, 0, instanceIndex++);
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_TRIANGLE]);
	vkCmdBindIndexBuffer(commandBuffer, buffers.index.buffer, BUFFER_OFFSET_INDEX_VERTICES, VK_INDEX_TYPE_UINT16);
	mvps[instanceIndex] = viewProjection * models[2];
	materials[instanceIndex] = (struct Material){ 167, 127, 17, 255 };
	vkCmdDraw(commandBuffer, 3, 1, 0, instanceIndex++);

	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &viewProjection);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[GRAPHICS_PIPELINE_PARTICLE]);
	vkCmdDraw(commandBuffer, 1, 1, 0, 0);

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
	QueryPerformanceFrequency(&performanceFrequency);

	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);

	hInstance = GetModuleHandleW(NULL);
	mainFiber = ConvertThreadToFiber(NULL);

	LPVOID messageFiber = CreateFiber(0, messageFiberProc, NULL);

	WNDCLASSEXW windowClass = {
		.cbSize        = sizeof(WNDCLASSEXW),
		.lpfnWndProc   = wndProc,
		.hInstance     = hInstance,
		.lpszClassName = L"a",
		.hCursor       = LoadImageW(NULL, MAKEINTRESOURCEW(OCR_NORMAL), IMAGE_CURSOR, 0, 0, LR_SHARED)
	};
	RegisterClassExW(&windowClass);
	CreateWindowExW(0, windowClass.lpszClassName, L"Triangle", WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, windowClass.hInstance, NULL);

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	ioctlsocket(sock, FIONBIO, &(u_long){ 1 });

	(void)bind(sock, (struct sockaddr*)&(struct sockaddr_in){
		.sin_family      = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_ANY),
		.sin_port        = htons(9000)
	}, sizeof(struct sockaddr_in));

	CoInitializeEx(NULL, COINIT_SPEED_OVER_MEMORY);

	IMMDeviceEnumerator* enumerator;
	IMMDevice* audioDevice;
	IAudioClient* audioClient;
	IAudioRenderClient* audioRenderClient;
	WAVEFORMATEX waveFormat    = {
		.wFormatTag      = WAVE_FORMAT_PCM,
		.nChannels       = 2,
		.nSamplesPerSec  = 44100,
		.wBitsPerSample  = sizeof(uint16_t) * 8,
	};
	waveFormat.nBlockAlign     = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	UINT32 bufferSize;

	CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&enumerator);
	enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &audioDevice);
	audioDevice->lpVtbl->Activate(audioDevice, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audioClient);
	audioClient->lpVtbl->Initialize(audioClient, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, 10000000, 0, &waveFormat, NULL);
	audioClient->lpVtbl->GetService(audioClient, &IID_IAudioRenderClient, (void**)&audioRenderClient);
	audioClient->lpVtbl->GetBufferSize(audioClient, &bufferSize);
	audioClient->lpVtbl->Start(audioClient);

	vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		vkCreateCommandPool(device, &(VkCommandPoolCreateInfo){
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.queueFamilyIndex = queueFamilyIndex
		}, NULL, &commandPools[i]);

		vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo){
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool        = commandPools[i],
			.commandBufferCount = 1
		}, &commandBuffers[i]);

		vkCreateFence(device, &(VkFenceCreateInfo){
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = i > 0 ? VK_FENCE_CREATE_SIGNALED_BIT : 0
		}, NULL, &fences[i]);

		vkCreateSemaphore(device, &(VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		}, NULL, &imageAcquireSemaphores[i]);

		vkCreateSemaphore(device, &(VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		}, NULL, &semaphores[i]);
	}

	for (uint32_t i = 0; i < ARRAY_COUNT(quadIndices); i += 6) {
		quadIndices[i + 0] = (4 * (i / 6)) + 0;
		quadIndices[i + 1] = (4 * (i / 6)) + 1;
		quadIndices[i + 2] = (4 * (i / 6)) + 2;
		quadIndices[i + 3] = (4 * (i / 6)) + 0;
		quadIndices[i + 4] = (4 * (i / 6)) + 2;
		quadIndices[i + 5] = (4 * (i / 6)) + 3;
	}

	for (uint32_t i = 0; i < sizeof(buffers) / sizeof(struct Buffer); i++) {
		struct Buffer* b = (struct Buffer*)((char*)&buffers + (i * sizeof(struct Buffer)));
		vkCreateBuffer(device, &(VkBufferCreateInfo){
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size  = b->size,
			.usage = b->usage
		}, NULL, &b->buffer);

		vkGetBufferMemoryRequirements(device, b->buffer, &memoryRequirements);

		vkAllocateMemory(device, &(VkMemoryAllocateInfo){
			.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext           = &(VkMemoryDedicatedAllocateInfo){
				.sType  = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
				.buffer = b->buffer
			},
			.allocationSize  = memoryRequirements.size,
			.memoryTypeIndex = getMemoryTypeIndex(b->requiredMemoryPropertyFlagBits, b->optionalMemoryPropertyFlagBits)
		}, NULL, &b->deviceMemory);

		vkBindBufferMemory(device, b->buffer, b->deviceMemory, 0);

		if (b->requiredMemoryPropertyFlagBits & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
			vkMapMemory(device, b->deviceMemory, 0, VK_WHOLE_SIZE, 0, &b->data);
	}

	memcpy(buffers.staging.data, vertexPositions, sizeof(vertexPositions));
	memcpy(buffers.staging.data + BUFFER_RANGE_VERTEX_POSITIONS, vertexIndices, sizeof(vertexIndices));
	memcpy(buffers.staging.data + BUFFER_RANGE_VERTEX_POSITIONS + BUFFER_RANGE_INDEX_VERTICES, quadIndices, sizeof(quadIndices));

	vkBeginCommandBuffer(commandBuffers[frame], &(VkCommandBufferBeginInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

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

	vkEndCommandBuffer(commandBuffers[frame]);
	vkQueueSubmit(queue, 1, &(VkSubmitInfo){
		.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers    = &commandBuffers[frame]
	}, fences[frame]);

	vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo){
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount         = 1,
		.pSetLayouts            = &descriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &(VkPushConstantRange){
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset     = 0,
			.size       = sizeof(Mat4) + sizeof(Vec4)
		}
	}, NULL, &pipelineLayout);

	VkShaderModule shaderModule;
	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = (uint32_t*)&incbin_shaders_spv_start,
		.codeSize = (char*)&incbin_shaders_spv_end - (char*)&incbin_shaders_spv_start
	}, NULL, &shaderModule);
	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = blit_frag,
		.codeSize = sizeof(blit_frag)
	}, NULL, &blitFrag);
	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = triangle_vert,
		.codeSize = sizeof(triangle_vert)
	}, NULL, &triangleVert);
	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = triangle_frag,
		.codeSize = sizeof(triangle_frag)
	}, NULL, &triangleFrag);
	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = skybox_vert,
		.codeSize = sizeof(skybox_vert)
	}, NULL, &skyboxVert);
	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = skybox_frag,
		.codeSize = sizeof(skybox_frag)
	}, NULL, &skyboxFrag);
	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = particle_vert,
		.codeSize = sizeof(particle_vert)
	}, NULL, &particleVert);
	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = particle_frag,
		.codeSize = sizeof(particle_frag)
	}, NULL, &particleFrag);
	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = portal_vert,
		.codeSize = sizeof(portal_vert)
	}, NULL, &portalVert);
	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode    = voxel_vert,
		.codeSize = sizeof(voxel_vert)
	}, NULL, &voxelVert);

	VkPipelineShaderStageCreateInfo shaderStagesBlit[] = {
		{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shaderModule,
			.pName  = "blit_vert"
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
			.module = shaderModule,
			.pName  = "portal_frag"
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
				.stride    = sizeof(struct Material),
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
				.stride    = sizeof(struct Material),
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
	VkPipelineRasterizationStateCreateInfo rasterizationStateNone = {
		.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.lineWidth = 1.f
	};
	VkPipelineRasterizationStateCreateInfo rasterizationState = {
		.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.lineWidth = 1.f,
		.cullMode  = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
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

	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, GRAPHICS_PIPELINE_MAX, (VkGraphicsPipelineCreateInfo[]){
		[GRAPHICS_PIPELINE_BLIT] = {
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount          = ARRAY_COUNT(shaderStagesBlit),
			.pStages             = shaderStagesBlit,
			.pVertexInputState   = &vertexInputStateNone,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizationStateNone,
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
			.pRasterizationState = &rasterizationStateNone,
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
			.pRasterizationState = &rasterizationStateNone,
			.pMultisampleState   = &multisampleState,
			.pDepthStencilState  = &depthStencilStateTriangle,
			.pColorBlendState    = &colorBlendState,
			.pDynamicState       = &dynamicStateTriangle,
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
	}, NULL, graphicsPipelines);

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
			n > 0;
		})) {
			char str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &clientAddr.sin_addr, str, sizeof(str));

			char ABC[1024];
			sprintf(ABC, "Received %d bytes from %s:%d\n%s\n", n, str, ntohs(clientAddr.sin_port), buff);
			MessageBoxA(NULL, ABC, NULL, MB_OK);
		}

		while (({
			LARGE_INTEGER now;
			QueryPerformanceCounter(&now);

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

			player.translation += 0.2f * forwardMovement;
			player.translation += 0.2f * sidewaysMovement;

			for (uint32_t i = 0; i < ARRAY_COUNT(portals); i++) {
				// todo: portal travel
				// Portal* portal = &portals[i];
				// Portal* link = &portals[portal->link];
			}

			ticksElapsed++;
		}

		commandBuffer = commandBuffers[frame];
		instanceIndex = 0;
		mvps          = buffers.instance.data + BUFFER_OFFSET_INSTANCE_MVPS + (frame * BUFFER_RANGE_INSTANCE_MVPS);
		materials     = buffers.instance.data + BUFFER_OFFSET_INSTANCE_MATERIALS + (frame * BUFFER_RANGE_INSTANCE_MATERIALS);

		Vec3 xAxis     = { cy, 0, -sy };
		Vec3 yAxis     = { sy * sp, cp, cy * sp };
		Vec3 zAxis     = { sy * cp, -sp, cp * cy };
		cameraPosition = player.translation; // todo: inter/extrapolate between frames

		models[0] = mat4FromRotationTranslationScale(quatIdentity(), (Vec3){ -20.f, 0.f, 0.f }, (Vec3){ 20.f, 1.f, 20.f });
		models[1] = mat4FromRotationTranslationScale(quatIdentity(), (Vec3){  20.f, 0.f, 0.f }, (Vec3){ 20.f, 1.f, 20.f });
		models[2] = mat4FromRotationTranslationScale(quatIdentity(), (Vec3){  0.f, 2.f, 0.f }, (Vec3){ 1.f, 1.f, 1.f });
		models[3] = mat4FromRotationTranslationScale(quatIdentity(), (Vec3){  0.f, 4.f, 0.f }, (Vec3){ 1.f, 1.f, 1.f });

		uint32_t imageIndex;
		vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquireSemaphores[frame], VK_NULL_HANDLE, &imageIndex);
		vkWaitForFences(device, 1, &fences[frame], VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &fences[frame]);
		vkResetCommandPool(device, commandPools[frame], 0);
		vkBeginCommandBuffer(commandBuffer, &(VkCommandBufferBeginInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		});

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

		drawScene(cameraPosition, xAxis, yAxis, zAxis, (Vec4){ 0.f, 0.f, 0.f, 0.f }, 0, -1);

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

		vkEndCommandBuffer(commandBuffer);
		vkQueueSubmit(queue, 1, &(VkSubmitInfo){
			.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount   = 1,
			.pWaitSemaphores      = &imageAcquireSemaphores[frame],
			.pWaitDstStageMask    = &(VkPipelineStageFlags){ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
			.commandBufferCount   = 1,
			.pCommandBuffers      = &commandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores    = &semaphores[frame]
		}, fences[frame]);
		vkQueuePresentKHR(queue, &(VkPresentInfoKHR){
			.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores    = &semaphores[frame],
			.swapchainCount     = 1,
			.pSwapchains        = &swapchain,
			.pImageIndices      = &imageIndex
		});

		frame = (frame + 1) % FRAMES_IN_FLIGHT;
	}
}

int _fltused;
