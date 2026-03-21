#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <unordered_set>
#include <limits>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

// "whislist" of debug validation layers
const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication
{
  public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	GLFWwindow        *window = nullptr;
	vk::raii::Context context;
	vk::raii::Instance instance = nullptr;
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::PhysicalDevice physicalDevice = nullptr;
	vk::raii::Device         device         = nullptr;
	vk::raii::Queue queue = nullptr;
	vk::raii::SurfaceKHR surface = nullptr;
	vk::raii::SwapchainKHR swapChain = nullptr;
	std::vector<vk::Image> swapChainImages;
	vk::SurfaceFormatKHR swapChainSurfaceFormat;
	vk::Extent2D swapChainExtent;
	std::vector<vk::raii::ImageView> swapChainImageViews;


	std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		// resize takes special care, disable for now
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void initVulkan()
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
	}

	void createInstance()
	{
		/* 1. step is to create an instance: a vulkan application instance.
		  The style of passing meta data to fopr the creation, is via predefined structs.
		  designated initalizers (C++20) are used, so that some of the fields be left default
		  GLFW is used to manage windows, it tells Vulkan which extensions to use to be able to
		  create and use the windows on this specific platform.

		  2. Validation layers: Our "whislist" of validation layers,
		  are copied into a local vector (if debug build flag), and checked if these are supported. 
		  Check is just string basedlookup from the list of all supported layers.
		  If supported, the whislist is just passed into the instannce creation instance.
		  Validation layer itself will be a function pointer before the real call:
		  our call -> fp -> validation layer with same func name and signature -> real vulkan call
		  "Chain of responsibility" pattern.
		*/
		
		constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "hello Triangle",
		                                      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		                                      .pEngineName        = "No Engine",
		                                      .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
		                                      .apiVersion         = vk::ApiVersion14};

		// Get the required layers and check they are supported.
		std::vector<char const *> requiredLayers;
		if (enableValidationLayers){
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}
		
		std::unordered_set<std::string> allSupportedLayers;
		auto layerProperties    = context.enumerateInstanceLayerProperties(); //expected in ~20 layer structs
		for(auto const& layer: layerProperties){
			allSupportedLayers.insert(layer.layerName);
		}
		for(auto requiredLayer: requiredLayers){
			std::string requiredLayerStr(requiredLayer);
			if(!allSupportedLayers.contains(requiredLayer)){
				throw std::runtime_error(" A required validation layer is not supported:" + requiredLayerStr);
			}
		}

		// Get the required extensions (GLFW + debug utils if validation enabled) and check they are supported.
		auto requiredExtensions      = getRequiredInstanceExtensions();
		auto extensionProperties     = context.enumerateInstanceExtensionProperties();
		auto unsupportedPropertyIt   = std::ranges::find_if(requiredExtensions,
		                                                  [&extensionProperties](auto const &requiredExtension) {
			                                                  return std::ranges::none_of(extensionProperties,
			                                                                              [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
		                                                  });
		if (unsupportedPropertyIt != requiredExtensions.end())
		{
			throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
		}

		vk::InstanceCreateInfo createInfo{.pApplicationInfo        = &appInfo,
		                                  .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
		                                  .ppEnabledLayerNames     = requiredLayers.data(),
		                                  .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
		                                  .ppEnabledExtensionNames = requiredExtensions.data()};

		instance = vk::raii::Instance(context, createInfo);
	}

	void setupDebugMessenger()
	{
		if(!enableValidationLayers){return;}
		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		                                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		                                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		                                                   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		                                                   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation); 
		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
		                                                                      .messageType     = messageTypeFlags,
		                                                                      .pfnUserCallback = &debugCallback};
		debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
	}

	std::vector<const char *> getRequiredInstanceExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enableValidationLayers)
		{
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
	{
		if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{
			std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
		}

		return vk::False;
	}

	bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice)
	{
		// Check if the physicalDevice supports the Vulkan 1.3 API version
		bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

		// Check if any of the queue families support graphics operations
		auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
		bool supportsGraphics = false;
		for (auto const& qfp : queueFamilies) {
    		if (qfp.queueFlags & vk::QueueFlagBits::eGraphics) {
        		supportsGraphics = true;
				break;
    		}
		}

		// Check if all required physicalDevice extensions are available
		std::vector<vk::ExtensionProperties> availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
		
		// can we find all requied in the available ones?
		bool supportsAllRequiredExtensions = true;
		std::unordered_set<std::string> availableExtensions;
		for(auto const& extension: availableDeviceExtensions){
			availableExtensions.insert(extension.extensionName);
		}
		for(std::string requiredExtension: requiredDeviceExtension){
			if(!availableExtensions.contains(requiredExtension)){
				supportsAllRequiredExtensions=false;
				break;
			}
		}

		// Check if the physicalDevice supports the required features
		auto features =
		    physicalDevice
		        .template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
		                                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		// Return true if the physicalDevice meets all the criteria
		return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
	}

	void createSurface(){
		VkSurfaceKHR _surface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
		{
			throw std::runtime_error("failed to create window surface!");
		}
		surface = vk::raii::SurfaceKHR(instance, _surface);
	}

	void pickPhysicalDevice()
	{
		std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();

		for(auto const& candidate: physicalDevices){
			//we just pick the first suitable, but could also score devices, then pick the highest score
			if(isDeviceSuitable(candidate)){
				physicalDevice = candidate;
				break;
			}
		}
		
		if (physicalDevice==nullptr){throw std::runtime_error("failed to find a suitable GPU!");}
	}
	
	void createLogicalDevice()
	{
		// find the index of the first queue family that supports graphics
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

		// get the first index into queueFamilyProperties which supports graphics
		uint32_t queueIndex = std::numeric_limits<uint32_t>::max();
		for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i){
    		if (
				(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) and  //check graphics computation
				(physicalDevice.getSurfaceSupportKHR(i, *surface))  //check if we can rpesent result to a surface
				){
        		queueIndex = i;
        		break;
    		}
		}
		if (queueIndex == std::numeric_limits<uint32_t>::max()){
			throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
		}

		// Vulkan introduces chained structs (like a linked list) in order to extend with new info on existing api struct
		// Struct Chain is just chaining together the structs inside the template
		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
		    {},                                   // vk::PhysicalDeviceFeatures2
		    {.dynamicRendering = true},           // vk::PhysicalDeviceVulkan13Features
		    {.extendedDynamicState = true}        // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		};

		// create a Device
		float queuePriority = 0.5f;  //float 32 bit
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
														.queueFamilyIndex = queueIndex, 
														.queueCount = 1, 
														.pQueuePriorities = &queuePriority
													};

		vk::DeviceCreateInfo deviceCreateInfo{	.pNext  				 = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		                                        .queueCreateInfoCount    = 1,
		                                        .pQueueCreateInfos       = &deviceQueueCreateInfo,
		                                        .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()), 
		                                        .ppEnabledExtensionNames = requiredDeviceExtension.data() //swapchain enable
											};

		device        = vk::raii::Device(physicalDevice, deviceCreateInfo);
		queue = vk::raii::Queue(device, queueIndex, 0);
	}

	/*
		Return the resolution of swap chain images 
	*/
	vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
	{
		//in case the width+height is a sentiunel value, it signals that out gpu kbnows from the os,
		//that we have a high dpi display, where logical screen cordinates is != framebuffer
		//in this case screen coordinates !=  to pixels
		//this is bascially because OS is abstracting away actual screen size, so that ui elements have consistent size,
		//regardless of high vs low dpi
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		return {
			//clamp: reduce/increase width/height to be fitting between capabilities
		    std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		    std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
	}

	static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
	{
		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if ((0 < surfaceCapabilities.maxImageCount) and (surfaceCapabilities.maxImageCount < minImageCount))
		{
			minImageCount = surfaceCapabilities.maxImageCount;
		}
		return minImageCount;
	}

	static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats)
	{
		assert(!availableFormats.empty());

		for (auto const &format : availableFormats) {
			//try to find prefered pixel format and color space (sRGB)
			//(B8G8R8A8 = blue, green, red, alpha, 8 bits each)
        	if (format.format == vk::Format::eB8G8R8A8Srgb and format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            	return format;
        	}
    	}
    	return availableFormats[0];  // fallback: first available
	}

	static vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
	{
		assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
		return std::ranges::any_of(availablePresentModes,
		                           [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
		           vk::PresentModeKHR::eMailbox :
		           vk::PresentModeKHR::eFifo;
	}

	void createSwapChain(){
		//we query the gpu(physical dev) for info that it holds (some indirect from os)
		//and call for a function to select prefered 
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
		swapChainExtent = chooseSwapExtent(surfaceCapabilities); //(width * height)
		uint32_t minImageCount  = chooseSwapMinImageCount(surfaceCapabilities);

		std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
		swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

		std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
		vk::PresentModeKHR              presentMode           = chooseSwapPresentMode(availablePresentModes);
		//everything collected, can fill out struct and call raii creation
		vk::SwapchainCreateInfoKHR swapChainCreateInfo{.surface          = *surface,
		                                               .minImageCount    = minImageCount,
		                                               .imageFormat      = swapChainSurfaceFormat.format,
		                                               .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
		                                               .imageExtent      = swapChainExtent,
		                                               .imageArrayLayers = 1,
		                                               .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
		                                               .imageSharingMode = vk::SharingMode::eExclusive,
		                                               .preTransform     = surfaceCapabilities.currentTransform,
		                                               .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		                                               .presentMode      = presentMode,
		                                               .clipped          = true};

		swapChain       = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
		swapChainImages = swapChain.getImages();		
	}
	
	/*
	Populate the swap chain images views vector.
	Images are raw gpu memory, imageviews wrap it with additional info so that we can use it as render targets
	layer (/VR?) , mip level, color aspect, size
	*/
	void createImageViews()
	{
		assert(swapChainImageViews.empty());

		//make info struct once, and reuse it for every image
		vk::ImageViewCreateInfo imageViewCreateInfo{.viewType         = vk::ImageViewType::e2D,
		                                            .format           = swapChainSurfaceFormat.format,
		                                            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
		for (auto &image : swapChainImages)
		{
			imageViewCreateInfo.image = image;
			swapChainImageViews.emplace_back(device, imageViewCreateInfo);
		}
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
		glfwDestroyWindow(window);

		glfwTerminate();
	}
};

int main()
{
	try
	{
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
