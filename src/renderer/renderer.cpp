#include <stdio.h>
#include <assert.h>

#include <vector>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <volk.h>

#include<meshoptimizer.h>
#include"objparser.h"


#define VK_CHECK(call) \
	do{ \
		VkResult result_ = call;\
		assert(result_ == VK_SUCCESS);\
	}while (0)

#ifndef ARRAYSIZE
#define ARRAYSIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

VkInstance createInstance()
{
	VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	appInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	createInfo.pApplicationInfo = &appInfo;

#ifdef _DEBUG
	const char* debugLayers[] = { "VK_LAYER_KHRONOS_validation" };

	createInfo.enabledLayerCount = sizeof(debugLayers) / sizeof(debugLayers[0]);
	createInfo.ppEnabledLayerNames = debugLayers;
#endif

	const char* extensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif // DEBUG
#ifdef _DEBUG 
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif 
	};

	createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
	createInfo.ppEnabledExtensionNames = extensions;

	VkInstance instance = 0;
	VK_CHECK(vkCreateInstance(&createInfo, NULL, &instance));

	return instance;
}

uint32_t getGraphicsQueueFamily(VkPhysicalDevice physicalDevice)
{
	uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, VK_NULL_HANDLE);

	std::vector<VkQueueFamilyProperties> queues(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queues.data());

	for (int i = 0; i < queueCount; i++)
	{
		if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			return i;
		}
	}

	return VK_QUEUE_FAMILY_IGNORED;
}

bool supportsPresentation(VkPhysicalDevice physicalDevice, uint32_t familyIndex)
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
	return vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, familyIndex);
#else
	return true;
#endif
}

VkPhysicalDevice pickPhysicalDevice(VkPhysicalDevice* physicalDevices, uint32_t physicalDeviceCount)
{
	VkPhysicalDevice discrete = 0;
	VkPhysicalDevice fallback = 0;

	for (uint32_t i = 0; i < physicalDeviceCount; ++i)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

		printf("GPU[%d]: %s\n", i, props.deviceName);

		uint32_t familyIndex = getGraphicsQueueFamily(physicalDevices[i]);

		if (familyIndex == VK_QUEUE_FAMILY_IGNORED)
			continue;

		if (!supportsPresentation(physicalDevices[i], familyIndex))
			continue;

		if (!discrete && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			discrete = physicalDevices[i];


		if (!fallback) fallback = physicalDevices[i];
	}

	if (!fallback)
		printf("No GPU available!");
	else {
		VkPhysicalDevice result = discrete ? discrete : fallback;

		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(result, &props);

		printf("Picking GPU: %s\n", props.deviceName);
	}

	return discrete ? discrete : fallback;
}

VkDevice createDevice(float queueProperties[], const VkPhysicalDevice& physicalDevice, uint32_t familyIndex)
{
	VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	queueInfo.queueFamilyIndex = familyIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = queueProperties;

	const char* extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueInfo;

	deviceInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);;
	deviceInfo.ppEnabledExtensionNames = extensions;

	VkDevice device = 0;
	VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, VK_NULL_HANDLE, &device));

	return device;
}

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window)
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
	VkWin32SurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	createInfo.hinstance = GetModuleHandle(0);
	createInfo.hwnd = glfwGetWin32Window(window);

	VkSurfaceKHR surface = 0;

	vkCreateWin32SurfaceKHR(instance, &createInfo, VK_NULL_HANDLE, &surface);

	return surface;
#endif // VK_USE_PLATFORM_WIN32_KHR
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(VkSurfaceFormatKHR* formats, uint32_t formatCount) {
	for (int i = 0; i < formatCount; i++) {
		if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return formats[i];
		}
	}

	return formats[0];
}

VkSurfaceFormatKHR getSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t familyIndex) {
	uint32_t surfaceForamatsCount;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceForamatsCount, NULL));

	std::vector<VkSurfaceFormatKHR> formats(surfaceForamatsCount);
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceForamatsCount, formats.data()));

	auto format = chooseSwapSurfaceFormat(formats.data(), surfaceForamatsCount);

	return format;
}

VkSwapchainKHR createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t familyIndex, VkSurfaceFormatKHR format, VkSwapchainKHR oldSwapchain)
{
	VkSurfaceCapabilitiesKHR surfaceCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps);

	VkCompositeAlphaFlagBitsKHR surfaceComposite =
		(surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
		: (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
		: (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
		: VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

	VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	createInfo.surface = surface;
	createInfo.minImageCount = surfaceCaps.minImageCount + 1;

	createInfo.imageFormat = format.format;
	createInfo.imageColorSpace = format.colorSpace;
	createInfo.imageExtent.width = surfaceCaps.currentExtent.width;
	createInfo.imageExtent.height = surfaceCaps.currentExtent.height;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 1;
	createInfo.pQueueFamilyIndices = &familyIndex;
	createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	createInfo.compositeAlpha = surfaceComposite;
	createInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = oldSwapchain;

	VkSwapchainKHR swapchain = 0;
	VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, NULL, &swapchain));

	return swapchain;
}

VkSemaphore createSemaphore(VkDevice device) {
	VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkSemaphore semaphore;
	VK_CHECK(vkCreateSemaphore(device, &createInfo, NULL, &semaphore));

	return semaphore;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex) {
	VkCommandPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	createInfo.queueFamilyIndex = familyIndex;

	VkCommandPool commandPool = 0;
	VK_CHECK(vkCreateCommandPool(device, &createInfo, NULL, &commandPool));

	return commandPool;
}

VkRenderPass createRenderPass(VkDevice device, VkFormat format)
{
	VkAttachmentDescription attachments[1] = {};
	attachments[0].format = format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachments = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachments;

	VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;
	createInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
	createInfo.pAttachments = attachments;

	VkRenderPass renderPass = 0;
	VK_CHECK(vkCreateRenderPass(device, &createInfo, VK_NULL_HANDLE, &renderPass));

	return renderPass;
}

VkFramebuffer createFramebuffer(VkDevice device, VkRenderPass renderPass, VkImageView imageView, uint32_t width, uint32_t height)
{
	VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	createInfo.renderPass = renderPass;
	createInfo.attachmentCount = 1;
	createInfo.pAttachments = &imageView;
	createInfo.width = width;
	createInfo.height = height;
	createInfo.layers = 1;

	VkFramebuffer framebuffer = 0;
	VK_CHECK(vkCreateFramebuffer(device, &createInfo, 0, &framebuffer));

	return framebuffer;
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format)
{
	VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	createInfo.image = image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.subresourceRange.layerCount = 1;
	createInfo.subresourceRange.levelCount = 1;

	VkImageView view = 0;
	VK_CHECK(vkCreateImageView(device, &createInfo, VK_NULL_HANDLE, &view));

	return view;
}

VkShaderModule loadShader(VkDevice device, const char* path)
{
	FILE* file = fopen(path, "rb");
	assert(file);

	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* buffer = new char[length];
	size_t rc = fread(buffer, 1, length, file);
	assert(rc == size_t(length));

	fclose(file);

	VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	createInfo.codeSize = length;
	createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer);

	VkShaderModule shaderModule = 0;
	VK_CHECK(vkCreateShaderModule(device, &createInfo, VK_NULL_HANDLE, &shaderModule));

	return shaderModule;
}

VkPipelineLayout createPipelineLayout(VkDevice device)
{
	VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

	VkPipelineLayout layout = 0;
	VK_CHECK(vkCreatePipelineLayout(device, &createInfo, 0, &layout));

	return layout;
}

VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass, VkShaderModule vs, VkShaderModule fs, VkPipelineLayout layout)
{

	VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vs;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fs;
	stages[1].pName = "main";

	createInfo.stageCount = sizeof(stages) / sizeof(stages[0]);
	createInfo.pStages = stages;

	VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	createInfo.pVertexInputState = &vertexInput;

	VkVertexInputBindingDescription stream = { 0,32,VK_VERTEX_INPUT_RATE_VERTEX };
	VkVertexInputAttributeDescription attrs[3] = {};

	attrs[0].location = 0;
	attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrs[0].offset = 0;

	attrs[1].location = 1;
	attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrs[1].offset = 12;

	attrs[2].location = 2;
	attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrs[2].offset = 24;

	vertexInput.vertexAttributeDescriptionCount = 3;
	vertexInput.pVertexAttributeDescriptions = attrs;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &stream;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	createInfo.pInputAssemblyState = &inputAssembly;

	VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;
	createInfo.pViewportState = &viewportState;

	VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizationState.lineWidth = 1.f;
	createInfo.pRasterizationState = &rasterizationState;

	VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	createInfo.pMultisampleState = &multisampleState;

	VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	createInfo.pDepthStencilState = &depthStencilState;

	VkPipelineColorBlendAttachmentState colorAttachmentState = {};
	colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorAttachmentState;
	createInfo.pColorBlendState = &colorBlendState;

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;
	createInfo.pDynamicState = &dynamicState;

	createInfo.layout = layout;
	createInfo.renderPass = renderPass;

	VkPipeline pipeline = 0;
	VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &createInfo, 0, &pipeline));

	return pipeline;
}

VkBool32 debugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	const char* type = (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ? "ERROR " : "WARNING ";

	char message[4096];
	snprintf(message, ARRAYSIZE(message), "%s%s", type, pMessage);

	printf("%s", message);

#ifdef WIN32
	OutputDebugStringA(message);
#endif

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		assert(!"asdasd");
	}

	return false;
}

VkDebugReportCallbackEXT registerDebugCallback(VkInstance instance)
{
	VkDebugReportCallbackCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
	createInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
	createInfo.pfnCallback = debugReportCallback;

	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

	VkDebugReportCallbackEXT callback = 0;
	VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &createInfo, VK_NULL_HANDLE, &callback));

	return callback;
}

VkImageMemoryBarrier imageBarrier(VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

	barrier.srcAccessMask = srcAccessMask;
	barrier.oldLayout = oldLayout;
	barrier.dstAccessMask = dstAccessMask;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return barrier;
}

struct Swapchain {
	VkSwapchainKHR swapchain;
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	std::vector<VkFramebuffer> framebuffers;
	uint32_t width, height;
	uint32_t imageCount;
};

void destroySwapchain(Swapchain& swapchain, VkDevice device)
{
	for (int i = 0; i < swapchain.imageCount; i++)
	{
		vkDestroyFramebuffer(device, swapchain.framebuffers[i], VK_NULL_HANDLE);
	}

	for (int i = 0; i < swapchain.imageCount; i++)
	{
		vkDestroyImageView(device, swapchain.imageViews[i], VK_NULL_HANDLE);
	}

	vkDestroySwapchainKHR(device, swapchain.swapchain, VK_NULL_HANDLE);
}

void createSwapchain(Swapchain& result, VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t familyIndex, VkSurfaceFormatKHR format, VkRenderPass renderPass, VkSwapchainKHR oldSwapchain = 0) {
	VkSurfaceCapabilitiesKHR surfaceCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps);

	VkSwapchainKHR swapchain = createSwapchain(device, physicalDevice, surface, familyIndex, format, oldSwapchain);

	uint32_t imageCount = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, VK_NULL_HANDLE));

	std::vector<VkImage> swapchainImages(imageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));

	std::vector<VkImageView> swapchainImageViews(imageCount);
	for (int i = 0; i < imageCount; i++)
	{
		swapchainImageViews[i] = createImageView(device, swapchainImages[i], format.format);
		assert(swapchainImageViews[i]);
	}

	std::vector<VkFramebuffer> swapchainFramebuffers(imageCount);
	for (int i = 0; i < imageCount; i++)
	{
		swapchainFramebuffers[i] = createFramebuffer(device, renderPass, swapchainImageViews[i], surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height);
		assert(swapchainFramebuffers[i]);
	}

	result.swapchain = swapchain;

	result.imageCount = imageCount;

	result.images = swapchainImages;
	result.imageViews = swapchainImageViews;
	result.framebuffers = swapchainFramebuffers;

	result.width = surfaceCaps.currentExtent.width;
	result.height = surfaceCaps.currentExtent.height;
}


void resizeSwapchain(Swapchain& result, VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t familyIndex, VkSurfaceFormatKHR format, VkRenderPass renderPass)
{
	VkSurfaceCapabilitiesKHR surfaceCaps;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

	uint32_t newWidth = surfaceCaps.currentExtent.width;
	uint32_t newHeight = surfaceCaps.currentExtent.height;

	if (result.width == newWidth && result.height == newHeight) return;

	VkSwapchainKHR oldSwapchain = result.swapchain;

	Swapchain old = result;

	createSwapchain(result, device, physicalDevice, surface, familyIndex, format, renderPass, oldSwapchain);

	VK_CHECK(vkDeviceWaitIdle(device));

	destroySwapchain(old, device);
}

struct Vertex {
	float vx, vy, vz;
	float nx, ny, nz;
	float tu, tv;
};

struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

bool loadMesh(Mesh& result, const char* path)
{
	ObjFile file;
	if (!objParseFile(file, path))
		return false;

	size_t index_count = file.f_size / 3;

	std::vector<Vertex> vertices(index_count);

	for (size_t i = 0; i < index_count; i++) {
		Vertex& v = vertices[i];

		int vi = file.f[i * 3 + 0];
		int vti = file.f[i * 3 + 1];
		int vni = file.f[i * 3 + 2];

		v.vx = file.v[vi * 3 + 0];
		v.vy = file.v[vi * 3 + 1];
		v.vz = file.v[vi * 3 + 2];

		v.nx = vni < 0 ? 0.f : file.vn[vni * 3 + 0];
		v.ny = vni < 0 ? 0.f : file.vn[vni * 3 + 1];
		v.nz = vni < 0 ? 1.f : file.vn[vni * 3 + 2];

		v.tu = vti < 0 ? 0.f : file.vt[vti * 3 + 0];
		v.tv = vti < 0 ? 0.f : file.vt[vti * 3 + 1];
	}

	std::vector<uint32_t> remap(index_count);

	size_t vertex_count = meshopt_generateVertexRemap(remap.data(), 0, index_count, vertices.data(), index_count, sizeof(Vertex));

	result.vertices.resize(vertex_count);
	result.indices.resize(index_count);

	meshopt_remapVertexBuffer(result.vertices.data(), vertices.data(), index_count, sizeof(Vertex), remap.data());
	meshopt_remapIndexBuffer(result.indices.data(), 0, index_count, remap.data());

	return true;
}

struct Buffer
{
	VkBuffer buffer;
	VkDeviceMemory memory;
	void* data;
	size_t size;
};

uint32_t selectMemoryType(VkPhysicalDeviceMemoryProperties memoryProperties, uint32_t memoryTypeBits, VkMemoryPropertyFlags flags)
{
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if ((memoryTypeBits & (1 << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)
		{
			return i;
		}
	}

	assert(!"No compatible memory type found");
	return ~0u;
}

void createBuffer(Buffer& result, VkDevice device, VkPhysicalDeviceMemoryProperties memoryProperties, size_t size, VkBufferUsageFlags usage)
{
VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	createInfo.size = size;
	createInfo.usage = usage;

	VkBuffer buffer = 0;
	VK_CHECK(vkCreateBuffer(device, &createInfo, 0, &buffer));

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

	uint32_t memoryTypeIndex = selectMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	assert(memoryTypeIndex != ~0u);

	VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = memoryTypeIndex;

	VkDeviceMemory memory = 0;
	VK_CHECK(vkAllocateMemory(device, &allocateInfo, 0, &memory));

	VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

	void* data = 0;
	VK_CHECK(vkMapMemory(device, memory, 0, size, 0, &data));

	result.buffer = buffer;
	result.memory = memory;
	result.data = data;
	result.size = size;
}

void destroyBuffer(Buffer& buffer, VkDevice device)
{
	vkFreeMemory(device, buffer.memory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, buffer.buffer, VK_NULL_HANDLE);
}

int main(int argc, const char** argv)
{
	if (argc < 2)
	{
		return 1;
	}

	int rc = glfwInit();
	assert(rc);

	VK_CHECK(volkInitialize());

	VkInstance instance = createInstance();
	assert(instance);

	volkLoadInstance(instance);

	VkDebugReportCallbackEXT debugCallback = registerDebugCallback(instance);

	VkPhysicalDevice physicalDevices[16];
	uint32_t physicalDeviceCount = sizeof(physicalDevices) / sizeof(physicalDevices[0]);
	VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

	VkPhysicalDevice physicalDevice = pickPhysicalDevice(physicalDevices, physicalDeviceCount);
	assert(physicalDevice);

	float queueProperties[] = { 1.0f };

	uint32_t familyIndex = getGraphicsQueueFamily(physicalDevice);

	assert(familyIndex != VK_QUEUE_FAMILY_IGNORED);

	VkDevice device = createDevice(queueProperties, physicalDevice, familyIndex);

	volkLoadDevice(device);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1024, 768, "renderer", NULL, NULL);
	assert(window);

	VkSurfaceKHR surface = createSurface(instance, window);
	assert(surface);

	VkBool32 supported;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &supported));
	assert(supported);

	auto swapchainFormat = getSwapchainFormat(physicalDevice, surface, familyIndex);

	VkSemaphore aquireSemaphore = createSemaphore(device);
	assert(aquireSemaphore);

	VkSemaphore releaseSemaphore = createSemaphore(device);
	assert(releaseSemaphore);

	VkQueue queue = 0;
	vkGetDeviceQueue(device, familyIndex, 0, &queue);

	VkRenderPass renderPass = createRenderPass(device, swapchainFormat.format);
	assert(renderPass);

	VkShaderModule triangleVS = loadShader(device, "shaders/triangle.vert.spv");
	assert(triangleVS);

	VkShaderModule triangleFS = loadShader(device, "shaders/triangle.frag.spv");
	assert(triangleFS);

	VkPipelineLayout triangleLayout = createPipelineLayout(device);
	assert(triangleLayout);

	Swapchain swapchain;
	createSwapchain(swapchain, device, physicalDevice, surface, familyIndex, swapchainFormat, renderPass);

	VkPipelineCache pipelineCache = 0;
	VkPipeline trianglePipeline = createGraphicsPipeline(device, pipelineCache, renderPass, triangleVS, triangleFS, triangleLayout);
	assert(trianglePipeline);

	VkCommandPool commandPool = createCommandPool(device, familyIndex);
	assert(commandPool);

	VkCommandBufferAllocateInfo commandBufferInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	commandBufferInfo.commandPool = commandPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = 0;
	VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferInfo, &commandBuffer));

	VkPhysicalDeviceMemoryProperties memoryProps;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProps);

	Mesh mesh;
	bool rcm = loadMesh(mesh, argv[1]);

	Buffer vb = {};
	createBuffer(vb, device, memoryProps, 128 * 1024 * 1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	Buffer ib = {};
	createBuffer(ib, device, memoryProps, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	assert(vb.size >= mesh.vertices.size() * sizeof(Vertex));
	memcpy(vb.data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

	assert(ib.size >= mesh.indices.size() * sizeof(uint32_t));
	memcpy(ib.data, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		resizeSwapchain(swapchain, device, physicalDevice, surface, familyIndex, swapchainFormat, renderPass);


		uint32_t imageIndex = 0;
		VK_CHECK(vkAcquireNextImageKHR(device, swapchain.swapchain, ~0ull, aquireSemaphore, VK_NULL_HANDLE, &imageIndex));

		VK_CHECK(vkResetCommandPool(device, commandPool, 0));

		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		VkImageMemoryBarrier renderBeginBarrier = imageBarrier(swapchain.images[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);

		VkClearColorValue color = { 48.f / 255.f, 10.f / 255.f, 36.f / 255.f, 1 };
		VkClearValue clearColor = { color };

		VkRenderPassBeginInfo passBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		passBeginInfo.renderPass = renderPass;
		passBeginInfo.framebuffer = swapchain.framebuffers[imageIndex];
		passBeginInfo.renderArea.extent.width = swapchain.width;
		passBeginInfo.renderArea.extent.height = swapchain.height;
		passBeginInfo.clearValueCount = 1;
		passBeginInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = { 0, float(swapchain.height), float(swapchain.width), -float(swapchain.height), 0, 1 };
		VkRect2D scissor = { {0, 0}, {uint32_t(swapchain.width), uint32_t(swapchain.height)} };

		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb.buffer, &offset);
		vkCmdBindIndexBuffer(commandBuffer, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, mesh.indices.size(), 1, 0, 0, 0);

		//vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);

		VkImageMemoryBarrier renderEndBarrier = imageBarrier(swapchain.images[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);

		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &aquireSemaphore;
		submitInfo.pWaitDstStageMask = &submitStageMask;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &releaseSemaphore;

		VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, NULL));

		VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &releaseSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain.swapchain;
		presentInfo.pImageIndices = &imageIndex;

		VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

		VK_CHECK(vkDeviceWaitIdle(device));
	}

	VK_CHECK(vkDeviceWaitIdle(device));

	destroyBuffer(vb, device);
	destroyBuffer(ib, device);

	glfwDestroyWindow(window);
	vkDestroyCommandPool(device, commandPool, NULL);

	destroySwapchain(swapchain, device);

	PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	vkDestroyDebugReportCallbackEXT(instance, debugCallback, VK_NULL_HANDLE);

	vkDestroySemaphore(device, releaseSemaphore, NULL);
	vkDestroySemaphore(device, aquireSemaphore, NULL);

	vkDestroyShaderModule(device, triangleVS, VK_NULL_HANDLE);
	vkDestroyShaderModule(device, triangleFS, VK_NULL_HANDLE);

	vkDestroyPipelineLayout(device, triangleLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(device, trianglePipeline, VK_NULL_HANDLE);

	vkDestroyRenderPass(device, renderPass, VK_NULL_HANDLE);

	vkDestroySurfaceKHR(instance, surface, NULL);

	vkDestroyDevice(device, NULL);

	vkDestroyInstance(instance, NULL);
}