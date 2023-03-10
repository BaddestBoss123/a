#define FRAMES_IN_FLIGHT 2
#define MAX_INSTANCES 0x10000
#define MAX_PARTICLES 1024
#define MAX_PORTAL_RECURSION 2
#define SHADOW_MAP_RESOLUTION 1024

#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "vcruntime.lib")
#pragma comment(lib, "ucrt.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")

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
#undef near
#undef far

#include "game.h"
#include "triangle.vert.h"
#include "triangle.frag.h"
#include "skybox.frag.h"
#include "particle.vert.h"
#include "particle.frag.h"
#include "shadow.vert.h"

int _fltused;
static const GUID CLSID_MMDeviceEnumerator = { 0xbcde0395, 0xe52f, 0x467c, { 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e } };
static const GUID IID_IMMDeviceEnumerator = { 0xa95664d2, 0x9614, 0x4f35, { 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6 } };
static const GUID IID_IAudioClient = { 0x1cb9ad4c, 0xdbfa, 0x4c32, { 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2 } };
static const GUID IID_IAudioRenderClient = { 0xf294acfc, 0x3146, 0x4483, { 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2 } };

INCBIN(shaders_spv, "shaders.spv");
INCBIN(indices, "indices");
INCBIN(vertices, "vertices");
INCBIN(attributes, "attributes");

#define F(fn) static PFN_##fn fn;
	BEFORE_INSTANCE_FUNCS(F)
	INSTANCE_FUNCS(F)
	DEVICE_FUNCS(F)
#undef F

enum Input {
	INPUT_START_LEFT = 1 << 0,
	INPUT_START_FORWARD = 1 << 1,
	INPUT_START_RIGHT = 1 << 2,
	INPUT_START_BACK = 1 << 3,
	INPUT_START_DOWN = 1 << 4,
	INPUT_START_UP = 1 << 5,
	INPUT_STOP_LEFT = 1 << 6,
	INPUT_STOP_FORWARD = 1 << 7,
	INPUT_STOP_RIGHT = 1 << 8,
	INPUT_STOP_BACK = 1 << 9,
	INPUT_STOP_DOWN = 1 << 10,
	INPUT_STOP_UP = 1 << 11,
	INPUT_LOCK_CURSOR = 1 << 12
};

struct Button {
	union {
		float x;
		float left;
	};
	union {
		float y;
		float top;
	};
	float width;
	float height;
};

struct Camera {
	Vec3 position;
	Vec3 right;
	Vec3 up;
	Vec3 forward;
};

struct PushConstants {
	Mat4 mvp;
	Vec4 clippingPlane;
};

struct Instance {
	Mat4 mvp;
	struct Material material;
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

enum BufferRanges {
	BUFFER_RANGE_INDEX_VERTICES = ALIGN_FORWARD(16 * 1024 * 1024, 256), //ALIGN_FORWARD(((char*)&incbin_indices_end - (char*)&incbin_indices_start), 256),
	BUFFER_RANGE_INDEX_QUADS = ALIGN_FORWARD(6 * 1024, 256), // todo calculate how many
	BUFFER_RANGE_VERTEX_POSITIONS = ALIGN_FORWARD(32 * 1024 * 1024, 256), //ALIGN_FORWARD(((char*)&incbin_vertices_end - (char*)&incbin_vertices_start), 256),
	BUFFER_RANGE_VERTEX_ATTRIBUTES = ALIGN_FORWARD(32 * 1024 * 1024, 256), //ALIGN_FORWARD(((char*)&incbin_vertices_end - (char*)&incbin_vertices_start), 256),
	BUFFER_RANGE_PARTICLES = ALIGN_FORWARD(MAX_PARTICLES, 256),
	BUFFER_RANGE_INSTANCES = ALIGN_FORWARD(MAX_INSTANCES * sizeof(struct Instance), 256),
};

enum BufferOffsets {
	BUFFER_OFFSET_INDEX_VERTICES = 0,
	BUFFER_OFFSET_INDEX_QUADS = BUFFER_RANGE_INDEX_VERTICES,
	BUFFER_OFFSET_VERTEX_POSITIONS = 0,
	BUFFER_OFFSET_VERTEX_ATTRIBUTES = BUFFER_RANGE_VERTEX_POSITIONS,
	BUFFER_OFFSET_PARTICLES = 0,
	BUFFER_OFFSET_INSTANCES = 0
};

static IAudioClient* audioClient;
static UINT32 audioBufferSize;
static IMMDevice* audioDevice;
static IAudioRenderClient* audioRenderClient;
static struct {
	struct Buffer staging;
	struct Buffer index;
	struct Buffer vertex;
	struct Buffer particles;
	struct Buffer instance;
} buffers = {
	.staging = {
		.size = 256 * 1024 * 1024,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	},
	.index = {
		.size = BUFFER_RANGE_INDEX_VERTICES + BUFFER_RANGE_INDEX_QUADS,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	},
	.vertex = {
		.size = BUFFER_RANGE_VERTEX_POSITIONS + BUFFER_RANGE_VERTEX_ATTRIBUTES,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	},
	.particles = {
		.size = FRAMES_IN_FLIGHT * BUFFER_RANGE_PARTICLES,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	},
	.instance = {
		.size = (FRAMES_IN_FLIGHT * BUFFER_RANGE_INSTANCES),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		.optionalMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	},
};
static struct Camera camera;
static int32_t clientID = -1;
static VkCommandBuffer commandBuffers[FRAMES_IN_FLIGHT];
static VkCommandPool commandPools[FRAMES_IN_FLIGHT];
static LARGE_INTEGER countsPerSecond;
static VkImage depthImage;
static VkDeviceMemory depthImageDeviceMemory;
static VkImageView depthImageView;
static VkDescriptorPool descriptorPool;
static VkDescriptorSet descriptorSet;
static VkDescriptorSetLayout descriptorSetLayout;
static VkDevice device;
static struct {
	Mat4 model; // todo: adjoint stuff
	struct Primitive* primitive;
} draws[MAX_INSTANCES];
static uint32_t drawCount;
static uint16_t entities[UINT16_MAX];
static uint16_t entityCount;
static int32_t entityID = -1;
static struct Entity entityMap[UINT16_MAX];
static IMMDeviceEnumerator* enumerator;
static VkFence fences[FRAMES_IN_FLIGHT];
static uint32_t frame;
static HMODULE hInstance;
static uint64_t input;
static VkInstance instance;
static uint32_t instanceCount;
static struct Instance* instances;
static VkSemaphore imageAcquireSemaphores[FRAMES_IN_FLIGHT];
static VkDescriptorImageInfo ktx2DescriptorImageInfos[_countof(ktx2ImageFiles)];
static VkDeviceSize ktx2ImageAllocationSize;
static VkDeviceSize ktx2ImageMemoryOffset;
static VkDeviceMemory ktx2ImageMemory;
static VkImageMemoryBarrier ktx2ImageMemoryBarriers[_countof(ktx2ImageFiles)];
static VkImage ktx2Images[_countof(ktx2ImageFiles)];
static LPVOID mainFiber;
static VkMemoryRequirements memoryRequirements;
static VkPhysicalDevice physicalDevice;
static VkPhysicalDeviceFeatures physicalDeviceFeatures;
static VkPhysicalDeviceProperties physicalDeviceProperties;
static VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
static float pitch;
static VkPipelineLayout pipelineLayout;
static struct {
	VkPipeline triangle;
	VkPipeline skybox;
	VkPipeline particle;
	VkPipeline portalStencil;
	VkPipeline portalDepthClear;
	VkPipeline portalDepthWrite;
	VkPipeline shadow;
} pipelines;
static struct Portal portals[] = {
	{
		.translation = { 1.f, 3.f, 0.f },
		.rotation = { 0, 0, 0, 1 },
		.scale = { 1, 1, 1 },
		.link = 1
	}, {
		.translation = { -1.f, 6.f, 0.f },
		.rotation = { 0, 0, 0, 1 },
		.scale = { 1, 1, 1 },
		.link = 0
	}
};
static Mat4 projectionMatrix;
static VkQueue queue;
static uint32_t queueFamilyIndex;
static VkRenderPass renderPass;
static VkRenderPass renderPassShadow;
static VkSampler sampler;
static VkSemaphore semaphores[FRAMES_IN_FLIGHT];
static VkShaderModule shaderModule; // todo: everything in shaderModule
static VkShaderModule triangleVert;
static VkShaderModule triangleFrag;
static VkShaderModule skyboxFrag;
static VkShaderModule particleVert;
static VkShaderModule particleFrag;
static VkShaderModule shadowVert;
static VkFramebuffer shadowFramebuffer;
static VkImage shadowImage;
static VkDeviceMemory shadowImageMemory;
static VkImageView shadowImageView;
static float smoothFactor;
static VkDeviceSize stagingBufferDataOffset;
static LARGE_INTEGER start;
static VkSurfaceKHR surface;
static VkSurfaceCapabilitiesKHR surfaceCapabilities;
static VkSurfaceFormatKHR surfaceFormat;
static VkSwapchainKHR swapchain;
static VkFramebuffer swapchainFramebuffers[8];
static VkImage swapchainImages[8];
static uint32_t swapchainImagesCount;
static uint32_t swapchainImageIndex;
static VkImageView swapchainImageViews[8];
static uint64_t ticksElapsed;
// static Mat4 viewportMatrix;
static WSADATA wsaData;
static float yaw;

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
				.usUsage = HID_USAGE_GENERIC_MOUSE,
				.dwFlags = 0,
				.hwndTarget = hWnd
			}, 1, sizeof(RAWINPUTDEVICE));

			PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(LoadLibraryW(L"vulkan-1.dll"), "vkGetInstanceProcAddr");

			#define F(fn) fn = (PFN_##fn)vkGetInstanceProcAddr(NULL, #fn);
			BEFORE_INSTANCE_FUNCS(F)
			#undef F

			uint32_t apiVersion = VK_API_VERSION_1_0;
			if (vkEnumerateInstanceVersion)
				vkEnumerateInstanceVersion(&apiVersion);

			vkCreateInstance(&(VkInstanceCreateInfo){
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pApplicationInfo = &(VkApplicationInfo){
					.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
					.apiVersion = apiVersion
				},
				.enabledExtensionCount = 2,
				.ppEnabledExtensionNames = (const char*[]){
					VK_KHR_SURFACE_EXTENSION_NAME,
					VK_KHR_WIN32_SURFACE_EXTENSION_NAME
				}
			}, NULL, &instance);

			#define F(fn) fn = (PFN_##fn)vkGetInstanceProcAddr(instance, #fn);
			INSTANCE_FUNCS(F)
			#undef F

			vkCreateWin32SurfaceKHR(instance, &(VkWin32SurfaceCreateInfoKHR){
				.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
				.hinstance = hInstance,
				.hwnd = hWnd
			}, NULL, &surface);

			vkEnumeratePhysicalDevices(instance, &(uint32_t){ 1 }, &physicalDevice);
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
			vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
			vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

			VkSurfaceFormatKHR surfaceFormats[16];
			uint32_t surfaceFormatCount = _countof(surfaceFormats);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats);
			surfaceFormat = surfaceFormats[0];
			for (uint32_t i = 0; i < surfaceFormatCount; i++) {
				if (surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
					surfaceFormat = surfaceFormats[i];
					break;
				}
			}

			VkQueueFamilyProperties queueFamilyProperties[16];
			uint32_t queueFamilyPropertyCount = _countof(queueFamilyProperties);
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
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.queueCreateInfoCount = 1,
				.pQueueCreateInfos = &(VkDeviceQueueCreateInfo){
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = queueFamilyIndex,
					.queueCount = 1,
					.pQueuePriorities = &(float){ 1.f }
				},
				.enabledExtensionCount = 1,
				.ppEnabledExtensionNames = (const char*[]){ VK_KHR_SWAPCHAIN_EXTENSION_NAME },
				.pEnabledFeatures = &(VkPhysicalDeviceFeatures){
					.textureCompressionBC = physicalDeviceFeatures.textureCompressionBC,
					.depthClamp = physicalDeviceFeatures.depthClamp,
					.shaderClipDistance = physicalDeviceFeatures.shaderClipDistance
				}
			}, NULL, &device);

			#define F(fn) fn = (PFN_##fn)vkGetDeviceProcAddr(device, #fn);
			DEVICE_FUNCS(F)
			#undef F

			vkCreateRenderPass(device, &(VkRenderPassCreateInfo){
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 2,
				.pAttachments = (VkAttachmentDescription[]){
					{
						.format = surfaceFormat.format,
						.samples = VK_SAMPLE_COUNT_1_BIT,
						.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
						.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
						.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
						.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					}, {
						.format = VK_FORMAT_D24_UNORM_S8_UINT,
						.samples = VK_SAMPLE_COUNT_1_BIT,
						.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
						.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
						.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
					}
				},
				.subpassCount = 1,
				.pSubpasses = &(VkSubpassDescription){
					.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
					.colorAttachmentCount = 1,
					.pColorAttachments = &(VkAttachmentReference){
						.attachment = 0,
						.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
					},
					.pDepthStencilAttachment = &(VkAttachmentReference){
						.attachment = 1,
						.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
					}
				},
				.dependencyCount = 1,
				.pDependencies = &(VkSubpassDependency){
					.srcSubpass = VK_SUBPASS_EXTERNAL,
					.dstSubpass = 0,
					.srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
					.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
				}
			}, NULL, &renderPass);

			vkCreateSampler(device, &(VkSamplerCreateInfo){
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.minLod = 0.f,
				.maxLod = VK_LOD_CLAMP_NONE
			}, NULL, &sampler);
			VkSampler samplers[_countof(ktx2ImageFiles)];
			for (uint32_t i = 0; i < _countof(samplers); i++)
				samplers[i] = sampler;

			vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo){
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = 1,
				.pBindings = (VkDescriptorSetLayoutBinding[]){
					{
						.binding = 0,
						.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.descriptorCount = _countof(ktx2ImageFiles),
						.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
						.pImmutableSamplers = samplers
					}
				}
			}, NULL, &descriptorSetLayout);

			vkCreateDescriptorPool(device, &(VkDescriptorPoolCreateInfo){
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.maxSets = 1,
				.poolSizeCount = 1,
				.pPoolSizes = (VkDescriptorPoolSize[]){
					{
						.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.descriptorCount = _countof(ktx2ImageFiles)
					}
				}
			}, NULL, &descriptorPool);

			vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo){
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &descriptorSetLayout
			}, &descriptorSet);
		} break;
		case WM_DESTROY: {
			PostQuitMessage(0);
		} break;
		case WM_SIZE: {
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
			if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0)
				break;

			float f = 1.f / tanf(0.78 / 2.f);
			projectionMatrix[0][0] = f / ((float)surfaceCapabilities.currentExtent.width / (float)surfaceCapabilities.currentExtent.height);
			projectionMatrix[1][1] = -f;
			projectionMatrix[2][3] = 0.1f;
			projectionMatrix[3][2] = -1.f;

			VkSwapchainCreateInfoKHR swapchainCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.surface = surface,
				.minImageCount = surfaceCapabilities.minImageCount + 1,
				.imageFormat = surfaceFormat.format,
				.imageColorSpace = surfaceFormat.colorSpace,
				.imageExtent = surfaceCapabilities.currentExtent,
				.imageArrayLayers = 1,
				.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = VK_PRESENT_MODE_FIFO_KHR,
				.preTransform = surfaceCapabilities.currentTransform,
				.clipped = VK_TRUE,
				.oldSwapchain = swapchain
			};
			vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain);

			if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE) {
				vkDeviceWaitIdle(device);

				vkDestroyImageView(device, depthImageView, NULL);
				vkDestroyImage(device, depthImage, NULL);
				vkFreeMemory(device, depthImageDeviceMemory, NULL);

				for (uint32_t i = 0; i < swapchainImagesCount; i++) {
					vkDestroyFramebuffer(device, swapchainFramebuffers[i], NULL);
					vkDestroyImageView(device, swapchainImageViews[i], NULL);
				}

				vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, NULL);
			}

			swapchainImagesCount = _countof(swapchainImages);
			vkGetSwapchainImagesKHR(device, swapchain, &swapchainImagesCount, swapchainImages);

			vkCreateImage(device, &(VkImageCreateInfo){
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_D24_UNORM_S8_UINT,
				.extent = {
					.width = surfaceCapabilities.currentExtent.width,
					.height = surfaceCapabilities.currentExtent.height,
					.depth = 1
				},
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
			}, NULL, &depthImage);

			vkGetImageMemoryRequirements(device, depthImage, &memoryRequirements);

			vkAllocateMemory(device, &(VkMemoryAllocateInfo){
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = &(VkMemoryDedicatedAllocateInfo){
					.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
					.image = depthImage
				},
				.allocationSize = memoryRequirements.size,
				.memoryTypeIndex = getMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0)
			}, NULL, &depthImageDeviceMemory);

			vkBindImageMemory(device, depthImage, depthImageDeviceMemory, 0);

			vkCreateImageView(device, &(VkImageViewCreateInfo){
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = depthImage,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = VK_FORMAT_D24_UNORM_S8_UINT,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
					.levelCount = 1,
					.layerCount = 1
				}
			}, NULL, &depthImageView);

			for (uint32_t i = 0; i < swapchainImagesCount; i++) {
				vkCreateImageView(device, &(VkImageViewCreateInfo){
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = swapchainImages[i],
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = surfaceFormat.format,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1
					}
				}, NULL, &swapchainImageViews[i]);

				vkCreateFramebuffer(device, &(VkFramebufferCreateInfo){
					.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
					.renderPass = renderPass,
					.attachmentCount = 2,
					.pAttachments = (VkImageView[]){
						swapchainImageViews[i],
						depthImageView
					},
					.width = surfaceCapabilities.currentExtent.width,
					.height = surfaceCapabilities.currentExtent.height,
					.layers = 1
				}, NULL, &swapchainFramebuffers[i]);
			}
		} break;
		case WM_ACTIVATE: {
			if (wParam == WA_INACTIVE) {
				input |= (INPUT_STOP_FORWARD | INPUT_STOP_LEFT | INPUT_STOP_BACK |INPUT_STOP_RIGHT | INPUT_STOP_UP | INPUT_STOP_DOWN);
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

			pitch -= 0.0005f * (float)rawInput.data.mouse.lLastY;
			yaw -= 0.0005f * (float)rawInput.data.mouse.lLastX;

			pitch = CLAMP(pitch, -(float)M_PI * 0.4f, (float)M_PI * 0.3f);
			yaw += (yaw > M_PI) ? -(float)M_PI * 2.f : (yaw < -M_PI) ? (float)M_PI * 2.f : 0.f;

			float sp = sinf(pitch);
			float cp = cosf(pitch);
			float sy = sinf(yaw);
			float cy = cosf(yaw);

			camera.right = (Vec3){ cy, 0, -sy };
			camera.up = (Vec3){ sy * sp, cp, cy * sp };
			camera.forward = (Vec3){ sy * cp, -sp, cp * cy };

			return DefWindowProcW(hWnd, msg, wParam, lParam);
		} break;
		case WM_KEYDOWN: {
			switch (wParam) {
				case VK_ESCAPE: { } break;
				case VK_UP: case 'W': input |= INPUT_START_FORWARD; break;
				case VK_LEFT: case 'A': input |= INPUT_START_LEFT; break;
				case VK_DOWN: case 'S': input |= INPUT_START_BACK; break;
				case VK_RIGHT: case 'D': input |= INPUT_START_RIGHT; break;
				case VK_SPACE: input |= INPUT_START_UP; break;
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
				case VK_UP: case 'W': input |= INPUT_STOP_FORWARD; break;
				case VK_LEFT: case 'A': input |= INPUT_STOP_LEFT; break;
				case VK_DOWN: case 'S': input |= INPUT_STOP_BACK; break;
				case VK_RIGHT: case 'D': input |= INPUT_STOP_RIGHT; break;
				case VK_SPACE: input |= INPUT_STOP_UP; break;
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

static void travelPortal(Vec3* origin, Vec3* velocity) {
	if (vec3Length(*velocity) > 0.f) {
		Vec3 direction = *velocity;

		for (uint32_t i = 0; i < _countof(portals); i++) {
			struct Portal* portal = &portals[i];
			struct Portal* link = &portals[portal->link];

			Vec3 normal = vec3TransformQuat((Vec3){ 0.f, 0.f, 1.f }, portal->rotation);
			Vec4 plane = { normal.x, normal.y, normal.z, -vec3Dot(portal->translation, normal) };

			float t = rayPlane(*origin, direction, plane);
			if (t > 1.f || t < 0.f)
				continue;

			Vec3 hitPosition = vec3TransformQuat((*origin + direction * t) - portal->translation, quatConjugate(portal->rotation));
			if (fabs(hitPosition.x) > portal->scale.x || fabs(hitPosition.y) > portal->scale.y)
				continue;

			Quat q = quatMultiply(link->rotation, quatConjugate(portal->rotation));

			*origin = link->translation + vec3TransformQuat(*origin - portal->translation, q);
			*velocity = vec3TransformQuat(*velocity, q);

			float sinp = 2 * (q.w * q.y - q.z * q.x);
			yaw += fabs(sinp) >= 1.f ? copysignf((float)M_PI * 0.5f, sinp) : asinf(sinp);

			// clamp pitch/yaw again
			pitch = CLAMP(pitch, -(float)M_PI * 0.4f, (float)M_PI * 0.3f);
			yaw += (yaw > M_PI) ? -(float)M_PI * 2.f : (yaw < -M_PI) ? (float)M_PI * 2.f : 0.f;

			float sp = sinf(pitch);
			float cp = cosf(pitch);
			float sy = sinf(yaw);
			float cy = cosf(yaw);

			camera.right = (Vec3){ cy, 0, -sy };
			camera.up = (Vec3){ sy * sp, cp, cy * sp };
			camera.forward = (Vec3){ sy * cp, -sp, cp * cy };
		}
	}
}

static void drawScene(VkCommandBuffer commandBuffer, struct Camera camera, Vec4 clippingPlane, uint32_t recursionDepth, int32_t skipPortal) {
	Mat4 viewMatrix;
	viewMatrix[0][0] = camera.right.x;
	viewMatrix[1][0] = camera.up.x;
	viewMatrix[2][0] = camera.forward.x;
	viewMatrix[3][0] = 0.f;
	viewMatrix[0][1] = camera.right.y;
	viewMatrix[1][1] = camera.up.y;
	viewMatrix[2][1] = camera.forward.y;
	viewMatrix[3][1] = 0.f;
	viewMatrix[0][2] = camera.right.z;
	viewMatrix[1][2] = camera.up.z;
	viewMatrix[2][2] = camera.forward.z;
	viewMatrix[3][2] = 0.f;
	viewMatrix[0][3] = -vec3Dot(camera.right, camera.position);
	viewMatrix[1][3] = -vec3Dot(camera.up, camera.position);
	viewMatrix[2][3] = -vec3Dot(camera.forward, camera.position);
	viewMatrix[3][3] = 1.f;
	Mat4 viewProjection = projectionMatrix * viewMatrix;

	clippingPlane = vec4TransformMat4(clippingPlane, mat4Inverse(__builtin_matrix_transpose(viewProjection)));

	if (recursionDepth < MAX_PORTAL_RECURSION) {
		for (uint32_t i = 0; i < _countof(portals); i++) {
			if (i == skipPortal)
				continue;

			// todo: portal culling
			struct Portal* portal = &portals[i];
			struct Portal* link = &portals[portal->link];

			Mat4 portalMVP = viewProjection * mat4FromRotationTranslationScale(portal->rotation, portal->translation, portal->scale);
			Quat q = quatMultiply(link->rotation, quatConjugate(portal->rotation));

			Vec3 portalNormal = vec3TransformQuat((Vec3){ 0.f, 0.f, 1.f }, portal->rotation);
			Vec3 linkNormal = vec3TransformQuat((Vec3){ 0.f, 0.f, 1.f }, link->rotation);

			Vec4 newClippingPlane = { linkNormal.x, linkNormal.y, linkNormal.z, -vec3Dot(link->translation, linkNormal) };
			if (vec3Dot(portalNormal, camera.position) > vec3Dot(portal->translation, portalNormal))
				newClippingPlane *= -1.f;

			struct PushConstants pushConstants = {
				.mvp = portalMVP,
				.clippingPlane = clippingPlane
			};

			vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, recursionDepth);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.portalStencil);
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(struct PushConstants), &pushConstants);
			vkCmdDrawIndexed(commandBuffer, 6, 1, BUFFER_OFFSET_INDEX_QUADS / sizeof(uint16_t), 0, 0);

			vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, recursionDepth + 1);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.portalDepthClear);
			pushConstants.mvp[2][0] = pushConstants.mvp[2][1] = pushConstants.mvp[2][2] = pushConstants.mvp[2][3] = 0;
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(struct PushConstants), &pushConstants);
			vkCmdDrawIndexed(commandBuffer, 6, 1, BUFFER_OFFSET_INDEX_QUADS / sizeof(uint16_t), 0, 0);

			drawScene(commandBuffer, (struct Camera){
				.position = link->translation + vec3TransformQuat(camera.position - portal->translation, q),
				.right = vec3TransformQuat(camera.right, q),
				.up = vec3TransformQuat(camera.up, q),
				.forward = vec3TransformQuat(camera.forward, q)
			}, newClippingPlane, recursionDepth + 1, portal->link);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.portalDepthWrite);
			pushConstants.mvp = portalMVP;
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(struct PushConstants), &pushConstants);
			vkCmdDrawIndexed(commandBuffer, 6, 1, BUFFER_OFFSET_INDEX_QUADS / sizeof(uint16_t), 0, 0);
		}
	}

	vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, recursionDepth);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.triangle);
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Vec4), &clippingPlane);

	for (uint32_t i = 0; i < drawCount; i++) {
		// todo: culling
		instances[instanceCount].mvp = viewProjection * draws[i].model;
		instances[instanceCount].material = materials[draws[i].primitive->material];
		vkCmdDrawIndexed(commandBuffer, draws[i].primitive->indexCount, 1, draws[i].primitive->firstIndex, draws[i].primitive->vertexOffset, instanceCount++);
	}

	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &viewProjection);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void WinMainCRTStartup(void) {
	QueryPerformanceFrequency(&countsPerSecond);

	portals[0].rotation = quatFromAxisAngle((Vec3){ 0.f, 1.f, 0.f }, M_PI * 0.5f);

	hInstance = GetModuleHandleW(NULL);
	mainFiber = ConvertThreadToFiber(NULL);
	LPVOID messageFiber = CreateFiber(0, messageFiberProc, NULL);

	WNDCLASSEXW windowClass = {
		.cbSize = sizeof(WNDCLASSEXW),
		.lpfnWndProc = wndProc,
		.hInstance = hInstance,
		.lpszClassName = L"a",
		.hCursor = LoadImageW(NULL, MAKEINTRESOURCEW(OCR_NORMAL), IMAGE_CURSOR, 0, 0, LR_SHARED)
	};
	RegisterClassExW(&windowClass);
	CreateWindowExW(0, windowClass.lpszClassName, L"BaddestBoss123 Game", WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, windowClass.hInstance, NULL);

	vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		vkCreateCommandPool(device, &(VkCommandPoolCreateInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.queueFamilyIndex = queueFamilyIndex
		}, NULL, &commandPools[i]);

		vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = commandPools[i],
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

	for (uint32_t i = 0; i < sizeof(buffers) / sizeof(struct Buffer); i++) {
		struct Buffer* buffer = (struct Buffer*)((char*)&buffers + (i * sizeof(struct Buffer)));

		vkCreateBuffer(device, &(VkBufferCreateInfo){
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = buffer->size,
			.usage = buffer->usage
		}, NULL, &buffer->buffer);

		vkGetBufferMemoryRequirements(device, buffer->buffer, &memoryRequirements);

		vkAllocateMemory(device, &(VkMemoryAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = &(VkMemoryDedicatedAllocateInfo){
				.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
				.buffer = buffer->buffer
			},
			.allocationSize = memoryRequirements.size,
			.memoryTypeIndex = getMemoryTypeIndex(buffer->requiredMemoryPropertyFlagBits, buffer->optionalMemoryPropertyFlagBits)
		}, NULL, &buffer->deviceMemory);

		vkBindBufferMemory(device, buffer->buffer, buffer->deviceMemory, 0);

		if (buffer->requiredMemoryPropertyFlagBits & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
			vkMapMemory(device, buffer->deviceMemory, 0, VK_WHOLE_SIZE, 0, &buffer->data);
	}

	memcpy(buffers.staging.data + stagingBufferDataOffset, &incbin_vertices_start, ((char*)&incbin_vertices_end - (char*)&incbin_vertices_start)); stagingBufferDataOffset += BUFFER_RANGE_VERTEX_POSITIONS;
	memcpy(buffers.staging.data + stagingBufferDataOffset, &incbin_attributes_start, ((char*)&incbin_attributes_end - (char*)&incbin_attributes_start)); stagingBufferDataOffset += BUFFER_RANGE_VERTEX_ATTRIBUTES;
	memcpy(buffers.staging.data + stagingBufferDataOffset, &incbin_indices_start, ((char*)&incbin_indices_end - (char*)&incbin_indices_start)); stagingBufferDataOffset += BUFFER_RANGE_INDEX_VERTICES;

	uint16_t* quadIndices = (uint16_t*)(buffers.staging.data + stagingBufferDataOffset); stagingBufferDataOffset += BUFFER_RANGE_INDEX_QUADS;
	for (uint32_t i = 0; i < BUFFER_RANGE_INDEX_QUADS; i += 6) {
		quadIndices[i + 0] = (4 * (i / 6)) + 0;
		quadIndices[i + 1] = (4 * (i / 6)) + 1;
		quadIndices[i + 2] = (4 * (i / 6)) + 2;
		quadIndices[i + 3] = (4 * (i / 6)) + 2;
		quadIndices[i + 4] = (4 * (i / 6)) + 1;
		quadIndices[i + 5] = (4 * (i / 6)) + 3;
	}

	for (uint32_t i = 0; i < _countof(ktx2ImageFiles); i++) {
		vkCreateImage(device, &(VkImageCreateInfo){
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = ktx2ImageFiles[i]->vkFormat,
			.extent = {
				.width = ktx2ImageFiles[i]->pixelWidth,
				.height = ktx2ImageFiles[i]->pixelHeight,
				.depth = 1
			},
			.mipLevels = ktx2ImageFiles[i]->levelCount,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		}, NULL, &ktx2Images[i]);

		vkGetImageMemoryRequirements(device, ktx2Images[i], &memoryRequirements);

		ktx2ImageAllocationSize += ALIGN_FORWARD(memoryRequirements.size, physicalDeviceProperties.limits.bufferImageGranularity);

		ktx2ImageMemoryBarriers[i] = (VkImageMemoryBarrier){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_NONE,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = ktx2Images[i],
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = ktx2ImageFiles[i]->levelCount,
				.layerCount = 1
			}
		};
	}

	vkAllocateMemory(device, &(VkMemoryAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.memoryTypeIndex = getMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0),
		.allocationSize = ktx2ImageAllocationSize
	}, NULL, &ktx2ImageMemory);

	for (uint32_t i = 0; i < _countof(ktx2ImageFiles); i++) {
		vkBindImageMemory(device, ktx2Images[i], ktx2ImageMemory, ktx2ImageMemoryOffset);

		vkGetImageMemoryRequirements(device, ktx2Images[i], &memoryRequirements);

		ktx2ImageMemoryOffset += ALIGN_FORWARD(memoryRequirements.size, physicalDeviceProperties.limits.bufferImageGranularity);

		vkCreateImageView(device, &(VkImageViewCreateInfo){
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = ktx2Images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = ktx2ImageFiles[i]->vkFormat,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = ktx2ImageFiles[i]->levelCount,
				.layerCount = 1
			}
		}, NULL, &ktx2DescriptorImageInfos[i].imageView);

		ktx2DescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	vkBeginCommandBuffer(commandBuffers[frame], &(VkCommandBufferBeginInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	vkCmdCopyBuffer(commandBuffers[frame], buffers.staging.buffer, buffers.vertex.buffer, 2, (VkBufferCopy[]){
		{
			.srcOffset = 0,
			.dstOffset = BUFFER_OFFSET_VERTEX_POSITIONS,
			.size = BUFFER_RANGE_VERTEX_POSITIONS
		}, {
			.srcOffset = BUFFER_RANGE_VERTEX_POSITIONS,
			.dstOffset = BUFFER_OFFSET_VERTEX_ATTRIBUTES,
			.size = BUFFER_RANGE_VERTEX_ATTRIBUTES
		}
	});

	vkCmdCopyBuffer(commandBuffers[frame], buffers.staging.buffer, buffers.index.buffer, 2, (VkBufferCopy[]){
		{
			.srcOffset = BUFFER_RANGE_VERTEX_POSITIONS + BUFFER_RANGE_VERTEX_ATTRIBUTES,
			.dstOffset = BUFFER_OFFSET_INDEX_VERTICES,
			.size = BUFFER_RANGE_INDEX_VERTICES
		}, {
			.srcOffset = BUFFER_RANGE_VERTEX_POSITIONS + BUFFER_RANGE_VERTEX_ATTRIBUTES + BUFFER_RANGE_INDEX_VERTICES,
			.dstOffset = BUFFER_OFFSET_INDEX_QUADS,
			.size = BUFFER_RANGE_INDEX_QUADS
		}
	});

	vkCmdPipelineBarrier(commandBuffers[frame], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, _countof(ktx2ImageMemoryBarriers), ktx2ImageMemoryBarriers);

	for (uint32_t i = 0; i < _countof(ktx2Images); i++) {
		VkBufferImageCopy bufferImageCopies[16];

		for (uint32_t j = 0; j < ktx2ImageFiles[i]->levelCount; j++) {
			memcpy(buffers.staging.data + stagingBufferDataOffset, (char*)ktx2ImageFiles[i] + ktx2ImageFiles[i]->levels[j].byteOffset, ktx2ImageFiles[i]->levels[j].byteLength);

			bufferImageCopies[j] = (VkBufferImageCopy){
				.bufferOffset = stagingBufferDataOffset,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = j,
					.layerCount = 1
				},
				.imageExtent = {
					.width = ktx2ImageFiles[i]->pixelWidth >> j,
					.height = ktx2ImageFiles[i]->pixelHeight >> j,
					.depth = 1
				}
			};

			stagingBufferDataOffset += ALIGN_FORWARD(ktx2ImageFiles[i]->levels[j].byteLength, physicalDeviceProperties.limits.optimalBufferCopyOffsetAlignment);
		}

		vkCmdCopyBufferToImage(commandBuffers[frame], buffers.staging.buffer, ktx2Images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ktx2ImageFiles[i]->levelCount, bufferImageCopies);

		ktx2ImageMemoryBarriers[i] = (VkImageMemoryBarrier){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.image = ktx2Images[i],
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = ktx2ImageFiles[i]->levelCount,
				.layerCount = 1
			}
		};
	}

	vkCmdPipelineBarrier(commandBuffers[frame], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, _countof(ktx2ImageMemoryBarriers), ktx2ImageMemoryBarriers);

	vkEndCommandBuffer(commandBuffers[frame]);

	vkQueueSubmit(queue, 1, &(VkSubmitInfo){
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffers[frame]
	}, fences[frame]);

	vkUpdateDescriptorSets(device, 1, &(VkWriteDescriptorSet){
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descriptorSet,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = _countof(ktx2ImageFiles),
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = ktx2DescriptorImageInfos
	}, 0, NULL);

	vkCreateImage(device, &(VkImageCreateInfo){
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_D16_UNORM,
		.extent = {
			.width = SHADOW_MAP_RESOLUTION,
			.height = SHADOW_MAP_RESOLUTION,
			.depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
	}, NULL, &shadowImage);

	vkGetImageMemoryRequirements(device, shadowImage, &memoryRequirements);

	vkAllocateMemory(device, &(VkMemoryAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &(VkMemoryDedicatedAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
			.image = shadowImage
		},
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = getMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0)
	}, NULL, &shadowImageMemory);

	vkBindImageMemory(device, shadowImage, shadowImageMemory, 0);

	vkCreateImageView(device, &(VkImageViewCreateInfo){
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = shadowImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_D16_UNORM,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.levelCount = 1,
			.layerCount = 1
		}
	}, NULL, &shadowImageView);

	vkCreateRenderPass(device, &(VkRenderPassCreateInfo){
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &(VkAttachmentDescription){
			.format = VK_FORMAT_D16_UNORM,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		},
		.subpassCount = 1,
		.pSubpasses = &(VkSubpassDescription){
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.pDepthStencilAttachment = &(VkAttachmentReference){
				.attachment = 0,
				.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			}
		},
		.dependencyCount = 1,
		.pDependencies = &(VkSubpassDependency){
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		}
	}, NULL, &renderPassShadow);

	vkCreateFramebuffer(device, &(VkFramebufferCreateInfo){
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = renderPassShadow,
		.attachmentCount = 1,
		.pAttachments = &shadowImageView,
		.width = SHADOW_MAP_RESOLUTION,
		.height = SHADOW_MAP_RESOLUTION,
		.layers = 1
	}, NULL, &shadowFramebuffer);

	vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &(VkPushConstantRange){
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = sizeof(struct PushConstants)
		}
	}, NULL, &pipelineLayout);

	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = (uint32_t*)&incbin_shaders_spv_start,
		.codeSize = (char*)&incbin_shaders_spv_end - (char*)&incbin_shaders_spv_start
	}, NULL, &shaderModule);

	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = triangle_vert,
		.codeSize = sizeof(triangle_vert)
	}, NULL, &triangleVert);

	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = triangle_frag,
		.codeSize = sizeof(triangle_frag)
	}, NULL, &triangleFrag);

	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = skybox_frag,
		.codeSize = sizeof(skybox_frag)
	}, NULL, &skyboxFrag);

	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = particle_vert,
		.codeSize = sizeof(particle_vert)
	}, NULL, &particleVert);

	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = particle_frag,
		.codeSize = sizeof(particle_frag)
	}, NULL, &particleFrag);

	vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = shadow_vert,
		.codeSize = sizeof(shadow_vert)
	}, NULL, &shadowVert);

	VkPipelineShaderStageCreateInfo shaderStagesTriangle[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = triangleVert,
			.pName = "main"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = triangleFrag,
			.pName = "main"
		}
	};

	VkPipelineShaderStageCreateInfo shaderStagesSkybox[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shaderModule,
			.pName = "skybox_vert"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = skyboxFrag,
			.pName = "main"
		}
	};

	VkPipelineShaderStageCreateInfo shaderStagesParticle[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = particleVert,
			.pName = "main"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = particleFrag,
			.pName = "main"
		}
	};

	VkPipelineShaderStageCreateInfo shaderStagesPortal[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shaderModule,
			.pName = "portal_vert"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = shaderModule,
			.pName = "empty_frag"
		}
	};

	VkPipelineShaderStageCreateInfo shaderStagesShadow[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shadowVert,
			.pName = "main"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = shaderModule,
			.pName = "empty_frag"
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateNone = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateTriangle = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 3,
		.pVertexBindingDescriptions = (VkVertexInputBindingDescription[]){
			{
				.binding = 0,
				.stride = sizeof(struct VertexPosition),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			}, {
				.binding = 1,
				.stride = sizeof(struct VertexAttributes),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			}, {
				.binding = 2,
				.stride = sizeof(struct Instance),
				.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
			}
		},
		.vertexAttributeDescriptionCount = 10,
		.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]){
			{
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R16G16B16_UNORM,
				.offset = offsetof(struct VertexPosition, x)
			}, {
				.location = 1,
				.binding = 1,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(struct VertexAttributes, nx)
			}, {
				.location = 2,
				.binding = 1,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = offsetof(struct VertexAttributes, tx)
			}, {
				.location = 3,
				.binding = 1,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(struct VertexAttributes, u)
			}, {
				.location = 4,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 0
			}, {
				.location = 5,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 4 * sizeof(float)
			}, {
				.location = 6,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 8 * sizeof(float)
			}, {
				.location = 7,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 12 * sizeof(float)
			}, {
				.location = 8,
				.binding = 2,
				.format = VK_FORMAT_R8G8B8A8_UNORM,
				.offset = offsetof(struct Instance, material.r)
			}, {
				.location = 9,
				.binding = 2,
				.format = VK_FORMAT_R16_UINT,
				.offset = offsetof(struct Instance, material.colorIndex)
			}
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateShadow = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 2,
		.pVertexBindingDescriptions = (VkVertexInputBindingDescription[]){
			{
				.binding = 0,
				.stride = sizeof(struct VertexPosition),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			}, {
				.binding = 2,
				.stride = sizeof(struct Instance),
				.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
			}
		},
		.vertexAttributeDescriptionCount = 5,
		.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]){
			{
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R16G16B16_UNORM,
				.offset = offsetof(struct VertexPosition, x)
			}, {
				.location = 4,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 0
			}, {
				.location = 5,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 4 * sizeof(float)
			}, {
				.location = 6,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 8 * sizeof(float)
			}, {
				.location = 7,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 12 * sizeof(float)
			}
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateParticle = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = (VkVertexInputBindingDescription[]){
			{
				.binding = 3,
				.stride = sizeof(float) * 3,
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			}
		},
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]){
			{
				.location = 0,
				.binding = 3,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = 0
			}
		}
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParticle = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST
	};

	VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	VkPipelineRasterizationStateCreateInfo rasterizationStateNone = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.lineWidth = 1.f
	};

	VkPipelineRasterizationStateCreateInfo rasterizationState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.lineWidth = 1.f,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
	};

	VkPipelineRasterizationStateCreateInfo rasterizationStatePortal = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = physicalDeviceFeatures.depthClamp,
		.lineWidth = 1.f
	};

	VkPipelineMultisampleStateCreateInfo multisampleState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStateTriangle = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
		.stencilTestEnable = VK_TRUE,
		.front = {
			.failOp = VK_STENCIL_OP_KEEP,
			.passOp = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF
		},
		.back = {
			.failOp = VK_STENCIL_OP_KEEP,
			.passOp = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF
		}
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStateShadow = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_GREATER
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStateSkybox = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_EQUAL,
		.stencilTestEnable = VK_TRUE,
		.front = {
			.failOp = VK_STENCIL_OP_KEEP,
			.passOp = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF
		},
		.back = {
			.failOp = VK_STENCIL_OP_KEEP,
			.passOp = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF
		}
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStatePortalStencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
		.stencilTestEnable = VK_TRUE,
		.front = {
			.failOp = VK_STENCIL_OP_KEEP,
			.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask = 0xFF
		},
		.back = {
			.failOp = VK_STENCIL_OP_KEEP,
			.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask = 0xFF
		}
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStatePortalDepthClear = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_ALWAYS,
		.depthWriteEnable = VK_TRUE,
		.stencilTestEnable = VK_TRUE,
		.front = {
			.failOp = VK_STENCIL_OP_KEEP,
			.passOp = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF
		},
		.back = {
			.failOp = VK_STENCIL_OP_KEEP,
			.passOp = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF
		}
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStatePortalDepth = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
		.stencilTestEnable = VK_TRUE,
		.front = {
			.failOp = VK_STENCIL_OP_KEEP,
			.passOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask = 0xFF
		},
		.back = {
			.failOp = VK_STENCIL_OP_KEEP,
			.passOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask = 0xFF
		}
	};

	VkPipelineColorBlendStateCreateInfo colorBlendState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &(VkPipelineColorBlendAttachmentState){
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		}
	};

	VkPipelineColorBlendStateCreateInfo colorBlendStatePortal = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &(VkPipelineColorBlendAttachmentState){ }
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = (VkDynamicState[]){
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		}
	};

	VkPipelineDynamicStateCreateInfo dynamicStateStencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 3,
		.pDynamicStates = (VkDynamicState[]){
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_STENCIL_REFERENCE
		}
	};

	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, sizeof(pipelines) / sizeof(VkPipeline), (VkGraphicsPipelineCreateInfo[]){
		{ // triangle
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesTriangle),
			.pStages = shaderStagesTriangle,
			.pVertexInputState = &vertexInputStateTriangle,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilStateTriangle,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicStateStencil,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, { // skybox
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesSkybox),
			.pStages = shaderStagesSkybox,
			.pVertexInputState = &vertexInputStateNone,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationStateNone,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilStateSkybox,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicStateStencil,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, { // particle
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesParticle),
			.pStages = shaderStagesParticle,
			.pVertexInputState = &vertexInputStateParticle,
			.pInputAssemblyState = &inputAssemblyStateParticle,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationStateNone,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilStateTriangle,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicStateStencil,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, { // portalStencil
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesPortal),
			.pStages = shaderStagesPortal,
			.pVertexInputState = &vertexInputStateNone,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationStatePortal,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilStatePortalStencil,
			.pColorBlendState = &colorBlendStatePortal,
			.pDynamicState = &dynamicStateStencil,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, { // portalDepthClear
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesPortal),
			.pStages = shaderStagesPortal,
			.pVertexInputState = &vertexInputStateNone,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationStatePortal,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilStatePortalDepthClear,
			.pColorBlendState = &colorBlendStatePortal,
			.pDynamicState = &dynamicStateStencil,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, { // portalDepthWrite
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesPortal),
			.pStages = shaderStagesPortal,
			.pVertexInputState = &vertexInputStateNone,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationStatePortal,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilStatePortalDepth,
			.pColorBlendState = &colorBlendStatePortal,
			.pDynamicState = &dynamicStateStencil,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, { // shadow
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesShadow),
			.pStages = shaderStagesShadow,
			.pVertexInputState = &vertexInputStateShadow,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilStateShadow,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
			.renderPass = renderPassShadow
		}
	}, NULL, (VkPipeline*)&pipelines);

	extern void abc(void);
	abc();

	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	ioctlsocket(sock, FIONBIO, &(u_long){ 1 });

	struct sockaddr_in servaddr = {
		.sin_family = AF_INET,
		.sin_port = htons(8080)
	};
	inet_pton(AF_INET, "78.141.219.44", &servaddr.sin_addr);

	CoInitializeEx(NULL, COINIT_SPEED_OVER_MEMORY);
	CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&enumerator);
	enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &audioDevice);

	WAVEFORMATEX waveFormat = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = 2,
		.nSamplesPerSec = 44100,
		.wBitsPerSample = sizeof(uint16_t) * 8,
	};
	waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

	audioDevice->lpVtbl->Activate(audioDevice, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audioClient);
	audioClient->lpVtbl->Initialize(audioClient, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, 10000000, 0, &waveFormat, NULL);
	audioClient->lpVtbl->GetService(audioClient, &IID_IAudioRenderClient, (void**)&audioRenderClient);
	audioClient->lpVtbl->GetBufferSize(audioClient, &audioBufferSize);
	audioClient->lpVtbl->Start(audioClient);

	sendto(sock, (char*)&(struct ClientMessageID){
		.clientID = -1,
		.header = CLIENT_MESSAGE_ID
	}, sizeof(struct ClientMessageID), 0, (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in));

	QueryPerformanceCounter(&start);

	for (;;) {
		SwitchToFiber(messageFiber);

		static char buffer[16 * 1024 * 1024];
		while (recv(sock, buffer, sizeof(buffer), 0) > 0) {
			uint16_t header = ((uint16_t*)buffer)[0];

			switch (header) {
				case SERVER_MESSAGE_ID: {
					struct ServerMessageID* message = (struct ServerMessageID*)buffer;

					clientID = message->clientID;
					entityID = message->entityID;
				} break;
				case SERVER_MESSAGE_UPDATE: {
					struct ServerMessageUpdate* message = (struct ServerMessageUpdate*)buffer;

					for (uint32_t i = 0; i < message->entityCount; i++) {
						struct ServerEntity* entity = &message->entities[i];

						if (entity->flags & ENTITY_CREATED) {
							entityMap[entity->entityID] = (struct Entity){
								.translation = { entity->x, entity->y, entity->z },
								.velocity = { entity->vx, entity->vy, entity->vz },
								.rotation = { entity->rx, entity->ry, entity->rz, entity->rw },
								.scale = { entity->sx, entity->sy, entity->sz },
								.speed = entity->speed,
								.mesh = entity->entityID == entityID ? MESH_NONE : entity->mesh
							};

							entities[entityCount++] = entity->entityID;
						} else if (entity->flags & ENTITY_DESTROYED) {
							for (uint32_t i = 0; i < entityCount; i++) {
								if (entities[i] == entity->entityID) {
									entities[i] = entities[--entityCount];
									break;
								}
							}
						} else {
							entityMap[entity->entityID].speed = entity->speed;

							if (entity->entityID == entityID)
								continue;

							entityMap[entity->entityID].translation = (Vec3){ entity->x, entity->y, entity->z };
							entityMap[entity->entityID].velocity = (Vec3){ entity->vx, entity->vy, entity->vz };
							entityMap[entity->entityID].rotation = (Quat){ entity->rx, entity->ry, entity->rz, entity->rw };
							entityMap[entity->entityID].scale = (Vec3){ entity->sx, entity->sy, entity->sz };
							entityMap[entity->entityID].mesh = entity->mesh;
						}
					}
				} break;
			}
		}

		while (({
			LARGE_INTEGER now;
			QueryPerformanceCounter(&now);

			uint64_t countsElapsed = now.QuadPart - start.QuadPart;
			uint64_t targetTicksElapsed = (countsElapsed * TICKS_PER_SECOND) / countsPerSecond.QuadPart;

			float countsPerTick = (float)countsPerSecond.QuadPart / TICKS_PER_SECOND;
			smoothFactor = (float)(countsElapsed - (targetTicksElapsed * countsPerTick)) / countsPerTick;

			ticksElapsed < targetTicksElapsed;
		})) {
			if (entityID != -1) {
				struct Entity* player = &entityMap[entityID];

				Vec3 forwardMovement = { 0 };
				Vec3 sidewaysMovement = { 0 };

				player->velocity = (Vec3){ 0.f };

				if ((input & INPUT_START_FORWARD) && !(input & INPUT_START_BACK))
					forwardMovement = -(Vec3){ camera.forward.x, 0.f, camera.forward.z };
				else if ((input & INPUT_START_BACK) && !(input & INPUT_START_FORWARD))
					forwardMovement = (Vec3){ camera.forward.x, 0.f, camera.forward.z };

				if ((input & INPUT_START_LEFT) && !(input & INPUT_START_RIGHT))
					sidewaysMovement = -(Vec3){ camera.right.x, 0.f, camera.right.z };
				else if ((input & INPUT_START_RIGHT) && !(input & INPUT_START_LEFT))
					sidewaysMovement = (Vec3){ camera.right.x, 0.f, camera.right.z };

				if ((input & INPUT_START_UP) && !(input & INPUT_START_DOWN))
					player->velocity.y = 0.09f;
				else if ((input & INPUT_START_DOWN) && !(input & INPUT_START_UP))
					player->velocity.y = -0.09f;

				if (input & INPUT_STOP_FORWARD) input &= ~(INPUT_START_FORWARD | INPUT_STOP_FORWARD);
				if (input & INPUT_STOP_BACK) input &= ~(INPUT_START_BACK | INPUT_STOP_BACK);
				if (input & INPUT_STOP_LEFT) input &= ~(INPUT_START_LEFT | INPUT_STOP_LEFT);
				if (input & INPUT_STOP_RIGHT) input &= ~(INPUT_START_RIGHT | INPUT_STOP_RIGHT);
				if (input & INPUT_STOP_DOWN) input &= ~(INPUT_START_DOWN | INPUT_STOP_DOWN);
				if (input & INPUT_STOP_UP) input &= ~(INPUT_START_UP | INPUT_STOP_UP);

				player->velocity += player->speed * forwardMovement;
				player->velocity += player->speed * sidewaysMovement;

				travelPortal(&player->translation, &player->velocity);
				player->translation += player->velocity;

				Quat pitchQuat = quatFromAxisAngle((Vec3){ 1.f, 0.f, 0.f }, -pitch);
				Quat yawQuat = quatFromAxisAngle((Vec3){ 0.f, 1.f, 0.f }, -yaw);
				player->rotation = quatMultiply(yawQuat, pitchQuat);

				sendto(sock, (char*)&(struct ClientMessageUpdate){
					.clientID = clientID,
					.header = CLIENT_MESSAGE_UPDATE,
					.x = player->translation.x,
					.y = player->translation.y,
					.z = player->translation.z,
					.rx = player->rotation.x,
					.ry = player->rotation.y,
					.rz = player->rotation.z,
					.rw = player->rotation.w,
					.sx = player->scale.x,
					.sy = player->scale.y,
					.sz = player->scale.z
				}, sizeof(struct ClientMessageUpdate), 0, (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in));
			}

			ticksElapsed++;
		}

		if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0) {
			Sleep(1);
			continue;
		}

		// Vec3 sunDirection = vec3Normalize((Vec3){ 0.36f, 0.8f, 0.48f });
		// Mat4 ortho = mat4Ortho(-50.f, 50.f, -50.f, 50.f, 1.f, 100.f);
		// portals[0].rotation = quatRotateY(portals[0].rotation, 0.001);
		// portals[1].rotation = quatRotateY(portals[1].rotation, -0.001);

		drawCount = 0;
		instanceCount = 0;
		instances = buffers.instance.data + BUFFER_OFFSET_INSTANCES + (frame * BUFFER_RANGE_INSTANCES);

		// struct Entity* tree[64];
		// uint32_t childIndices[64];
		// Mat4 transforms[64]; // todo: stack transforms

		for (uint32_t i = 0; i < entityCount; i++) {
			struct Entity* entity = &entityMap[entities[i]];

			// uint32_t depth = 0;

		// todo:
			if (entity->mesh != MESH_NONE) {
				for (uint32_t j = 0; j < meshes[entity->mesh].primitiveCount; j++) {
					struct Primitive* primitive = &meshes[entity->mesh].primitives[j];

					// todo: this code here doesn't work for rotated meshes idk why
					Vec3 min = primitive->min;
					Vec3 max = primitive->max;
					Vec3 scale = entity->scale * (max - min);
					Vec3 translation = entity->translation + (entity->scale * min);

					draws[drawCount].model = mat4FromRotationTranslationScale(entity->rotation, translation, scale);
					draws[drawCount++].primitive = primitive;
				}
			}

			// if (entity->childCount) {
			// 	tree[depth] = entity;
			// 	childIndices[depth] = 0;
			// 	depth++;
			// 	entity = entity->children[0];
			// } else {
			// 	while (depth) {
			// 		depth--;
			// 		entity = tree[depth];
			// 		childIndices[depth]++;
			// 		if (childIndices[depth] != entity->childCount) {
			// 			entity = entity->children[childIndices[depth++]];
			// 			break;
			// 		}
			// 	}
			// 	if (depth)
			// 		goto todo;
			// }
		}

		vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquireSemaphores[frame], VK_NULL_HANDLE, &swapchainImageIndex);

		vkWaitForFences(device, 1, &fences[frame], VK_TRUE, UINT64_MAX);

		vkResetFences(device, 1, &fences[frame]);

		vkResetCommandPool(device, commandPools[frame], 0);

		vkBeginCommandBuffer(commandBuffers[frame], &(VkCommandBufferBeginInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		});

		vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

		vkCmdBindIndexBuffer(commandBuffers[frame], buffers.index.buffer, 0, VK_INDEX_TYPE_UINT16);

		vkCmdBindVertexBuffers(commandBuffers[frame], 0, 4, (VkBuffer[]){
			buffers.vertex.buffer,
			buffers.vertex.buffer,
			buffers.instance.buffer,
			buffers.particles.buffer
		}, (VkDeviceSize[]){
			BUFFER_OFFSET_VERTEX_POSITIONS,
			BUFFER_OFFSET_VERTEX_ATTRIBUTES,
			BUFFER_OFFSET_INSTANCES + (frame * BUFFER_RANGE_INSTANCES),
			BUFFER_OFFSET_PARTICLES + (frame * BUFFER_RANGE_PARTICLES)
		});

		VkRect2D scissor = {
			.extent.width = SHADOW_MAP_RESOLUTION,
			.extent.height = SHADOW_MAP_RESOLUTION
		};

		VkViewport viewport = {
			.width = SHADOW_MAP_RESOLUTION,
			.height = SHADOW_MAP_RESOLUTION,
			.minDepth = 0.f,
			.maxDepth = 1.f
		};

		vkCmdSetScissor(commandBuffers[frame], 0, 1, &scissor);

		vkCmdSetViewport(commandBuffers[frame], 0, 1, &viewport);

		vkCmdBeginRenderPass(commandBuffers[frame], &(VkRenderPassBeginInfo){
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = renderPassShadow,
			.framebuffer = shadowFramebuffer,
			.renderArea = scissor,
			.clearValueCount = 1,
			.pClearValues = &(VkClearValue){ { 0 } }
		}, VK_SUBPASS_CONTENTS_INLINE);

		// vkCmdBindPipeline(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadow);
		// for (uint32_t i = 0; i < drawCount; i++) {
		// 	// todo: culling
		// 	instances[instanceCount].mvp = ortho * draws[i].model;
		// 	instances[instanceCount].material = materials[draws[i].primitive->material];
		// 	vkCmdDrawIndexed(commandBuffers[frame], draws[i].primitive->indexCount, 1, draws[i].primitive->firstIndex, draws[i].primitive->vertexOffset, instanceCount++);
		// }

		vkCmdEndRenderPass(commandBuffers[frame]);

		scissor.extent = surfaceCapabilities.currentExtent;
		viewport.width = (float)surfaceCapabilities.currentExtent.width;
		viewport.height = (float)surfaceCapabilities.currentExtent.height;

		vkCmdSetScissor(commandBuffers[frame], 0, 1, &scissor);

		vkCmdSetViewport(commandBuffers[frame], 0, 1, &viewport);

		vkCmdBeginRenderPass(commandBuffers[frame], &(VkRenderPassBeginInfo){
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = renderPass,
			.framebuffer = swapchainFramebuffers[swapchainImageIndex],
			.renderArea = scissor,
			.clearValueCount = 2,
			.pClearValues = (VkClearValue[]){ { 0 }, { 0 } }
		}, VK_SUBPASS_CONTENTS_INLINE);

		if (entityID != -1) {
			camera.position = entityMap[entityID].translation;
			Vec3 velocity = entityMap[entityID].velocity * smoothFactor;

			float oldYaw = yaw;
			travelPortal(&camera.position, &velocity);
			camera.position += velocity;
			yaw = oldYaw;
		}

		drawScene(commandBuffers[frame], camera, (Vec4){ 0 }, 0, -1);

		float sp = sinf(pitch);
		float cp = cosf(pitch);
		float sy = sinf(yaw);
		float cy = cosf(yaw);

		camera.right = (Vec3){ cy, 0, -sy };
		camera.up = (Vec3){ sy * sp, cp, cy * sp };
		camera.forward = (Vec3){ sy * cp, -sp, cp * cy };

		vkCmdEndRenderPass(commandBuffers[frame]);

		vkEndCommandBuffer(commandBuffers[frame]);

		vkQueueSubmit(queue, 1, &(VkSubmitInfo){
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &imageAcquireSemaphores[frame],
			.pWaitDstStageMask = &(VkPipelineStageFlags){ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffers[frame],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &semaphores[frame]
		}, fences[frame]);

		vkQueuePresentKHR(queue, &(VkPresentInfoKHR){
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &semaphores[frame],
			.swapchainCount = 1,
			.pSwapchains = &swapchain,
			.pImageIndices = &swapchainImageIndex
		});

		frame = (frame + 1) % FRAMES_IN_FLIGHT;
	}
}