#include <stdio.h>
#include <assert.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>


#define VK_CHECK(call) \
	do{ \
		VkResult result_ = call;\
		assert(result_ == VK_SUCCESS);\
	}while (0)

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

	};

	createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
	createInfo.ppEnabledExtensionNames = extensions;

	VkInstance instance = 0;
	VK_CHECK(vkCreateInstance(&createInfo, NULL, &instance));

	return instance;
}

VkPhysicalDevice pickPhysicalDevice(VkPhysicalDevice* physicalDevices, uint32_t physicalDeviceCount)
{
	for (uint32_t i = 0; i < physicalDeviceCount; i++)
	{
		VkPhysicalDeviceProperties properties = {};
		vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);

		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			printf("Picking discrete GPU %s\n", properties.deviceName);
			return physicalDevices[i];
		}
	}

	if (physicalDeviceCount > 0)
	{
		VkPhysicalDeviceProperties properties = {};
		vkGetPhysicalDeviceProperties(physicalDevices[0], &properties);

		printf("Picking fallback GPU %s\n", properties.deviceName);
		return physicalDevices[0];
	}

	printf("No GPU found\n");
	return VK_NULL_HANDLE;
}

VkDevice createDevice(float queueProperties[], const VkPhysicalDevice& physicalDevice, uint32_t* familyIndex)
{
	*familyIndex = 0;

	VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	queueInfo.queueFamilyIndex = *familyIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = queueProperties;

	const char* extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueInfo;
	deviceInfo.enabledExtensionCount = 1;
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

	VkSurfaceKHR surface = {};

	vkCreateWin32SurfaceKHR(instance, &createInfo, NULL, &surface);

	return surface;
#endif // VK_USE_PLATFORM_WIN32_KHR
}

int main() {

	int rc = glfwInit();
	assert(rc);

	VkInstance instance = createInstance();
	assert(instance);

	VkPhysicalDevice physicalDevices[16];
	uint32_t physicalDeviceCount = 16;
	VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

	VkPhysicalDevice physicalDevice = pickPhysicalDevice(physicalDevices, physicalDeviceCount);
	assert(physicalDevice);

	float queueProperties[] = { 1.0f };


	uint32_t queueCount;

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);

	VkQueueFamilyProperties* properties = new VkQueueFamilyProperties[queueCount];

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, properties);

	delete[] properties;

	uint32_t familyIndex = 0;
	VkDevice device = createDevice(queueProperties, physicalDevice, &familyIndex);

	GLFWwindow* window = glfwCreateWindow(1024, 764, "renderer", NULL, NULL);
	assert(window);

	VkSurfaceKHR surface = createSurface(instance, window);
	assert(surface);

	uint32_t surfaceForamatsCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceForamatsCount, NULL);

	VkSurfaceFormatKHR* formats = new VkSurfaceFormatKHR[surfaceForamatsCount];
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceForamatsCount, formats);

	VkSurfaceCapabilitiesKHR surfCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);

	VkBool32 supported;
	vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &supported);

	int windowWidth = 0, windowHeight = 0;
	glfwGetWindowSize(window, &windowWidth, &windowHeight);
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = surfCaps.minImageCount;
	swapchainCreateInfo.imageFormat = formats[0].format;
	swapchainCreateInfo.imageColorSpace = formats[0].colorSpace;
	swapchainCreateInfo.imageExtent.width = windowWidth;
	swapchainCreateInfo.imageExtent.height = windowHeight;
	swapchainCreateInfo.imageArrayLayers = 1;
	//swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.queueFamilyIndexCount = 1;
	swapchainCreateInfo.pQueueFamilyIndices = &familyIndex;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;

	//vkCreateDebugReportCallbackEXT(instance)
	VkSwapchainKHR swapchain = 0;
	VkResult res = vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}

	glfwDestroyWindow(window);

	vkDestroySurfaceKHR(instance, surface, NULL);

	vkDestroyDevice(device, NULL);

	vkDestroyInstance(instance, NULL);
}


