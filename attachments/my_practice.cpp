#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <unordered_set>
#include <limits>
#include <fstream>
#include <array>
#include <assert.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;
constexpr int      MAX_FRAMES_IN_FLIGHT = 2;

// "whislist" of debug validation layers
const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

struct Vertex
{
	glm::vec2 pos;
	glm::vec3 color;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
	{
		return {
		    vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)),
		    vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))};
	}
};

const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};
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
	uint32_t queueIndex = std::numeric_limits<uint32_t>::max();
	vk::raii::Queue queue = nullptr;
	vk::raii::SurfaceKHR surface = nullptr;
	vk::raii::SwapchainKHR swapChain = nullptr;
	std::vector<vk::Image> swapChainImages;
	vk::SurfaceFormatKHR swapChainSurfaceFormat;
	vk::Extent2D swapChainExtent;
	std::vector<vk::raii::ImageView> swapChainImageViews;
	vk::raii::PipelineLayout pipelineLayout = nullptr;
	vk::raii::Pipeline       graphicsPipeline = nullptr;

	vk::raii::CommandPool    commandPool      = nullptr;
	std::vector<vk::raii::CommandBuffer>  commandBuffers;

	std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
	std::vector<vk::raii::Fence>     inFlightFences;
	uint32_t                         frameIndex = 0;

	vk::raii::Buffer       vertexBuffer       = nullptr;
	vk::raii::DeviceMemory vertexBufferMemory = nullptr;

	bool framebufferResized = false; //warn us when pixel extent change (usually window resize)

	std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	/*
	* Register this callback so that when OS will tell GLFW if window has been resized, this callback allows us
	* to set a variable true, so we can handle swapchain re-create
	*/
	static void framebufferResizeCallback(GLFWwindow *window, int width, int height)
	{
		auto app = reinterpret_cast<HelloTriangleApplication *>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
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
		createGraphicsPipeline();
		createCommandPool();
		createVertexBuffer();
		createCommandBuffers();
		createSyncObjects();
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
		        .template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
		                                features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
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
		// query for required features (Vulkan 1.1 and 1.3)
		vk::StructureChain<	vk::PhysicalDeviceFeatures2, 
							vk::PhysicalDeviceVulkan11Features, 
							vk::PhysicalDeviceVulkan13Features, 
							vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
		    {},                                                          // vk::PhysicalDeviceFeatures2
		    {.shaderDrawParameters = true},                              // vk::PhysicalDeviceVulkan11Features
		    {.synchronization2 = true, .dynamicRendering = true},        // vk::PhysicalDeviceVulkan13Features
		    {.extendedDynamicState = true}                               // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
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

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const
	{
    	vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t *>(code.data())};
    	vk::raii::ShaderModule     shaderModule{device, createInfo};
    	return shaderModule;
	}

	static std::vector<char> readFile(const std::string &filename)
	{	
		//open at end of to get size
    	std::ifstream file(filename, std::ios::ate | std::ios::binary);
    	if (!file.is_open())
    	{
        	throw std::runtime_error("failed to open file!");
    	}
    	std::vector<char> buffer(file.tellg()); //use cursor end size to make correct size
    	file.seekg(0, std::ios::beg);
    	file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    	file.close();
    	return buffer;
	}

	void createGraphicsPipeline()
	{

		//1. Vertex buffer (with handwritten values)
    	vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/slang.spv"));

    	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"};
    	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"};
    	vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		//2. Input Assembler

		auto bindingDescription    = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
												.vertexBindingDescriptionCount = 1, 
												.pVertexBindingDescriptions = &bindingDescription, 
												.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
												.pVertexAttributeDescriptions = attributeDescriptions.data()};
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};

		//not mapped to a stage:
		//cordinate transform between rasterization and framebuffer
		vk::PipelineViewportStateCreateInfo      viewportState{.viewportCount = 1, .scissorCount = 1};
		
		//3. Rasterizer
		vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False,
															.rasterizerDiscardEnable = vk::False,
															.polygonMode = vk::PolygonMode::eFill,	//solid triangles
															.cullMode = vk::CullModeFlagBits::eBack,
															.frontFace = vk::FrontFace::eClockwise,
															.depthBiasEnable = vk::False,
															.depthBiasSlopeFactor = 1.0f,
															.lineWidth = 1.0f};
		
		//4. Fragment shader
		vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};
		
		//5. Color Blending
		vk::PipelineColorBlendAttachmentState colorBlendAttachment{.blendEnable    = vk::False,
		                                                           .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
		vk::PipelineColorBlendStateCreateInfo colorBlending{.logicOpEnable = vk::False,
															.logicOp = vk::LogicOp::eCopy,
															.attachmentCount = 1,
															.pAttachments = &colorBlendAttachment};
		
		//dynamic sizing of swapchain: the viewport and sciccor rectangle (discard everything outside of it) will be decided at drawing time
		std::vector dynamicStates = { 	vk::DynamicState::eViewport,
										vk::DynamicState::eScissor}; 
		vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};
		
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0}; 
		pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);
		
		//chain is because dynamic rendering api change, but Vulkan didnt want to break old struct definitioons of previous versions
		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
		    {.stageCount          = 2,
		     .pStages             = shaderStages,
		     .pVertexInputState   = &vertexInputInfo,
		     .pInputAssemblyState = &inputAssembly,
		     .pViewportState      = &viewportState,
		     .pRasterizationState = &rasterizer,
		     .pMultisampleState   = &multisampling,
		     .pColorBlendState    = &colorBlending,
		     .pDynamicState       = &dynamicState,
		     .layout              = pipelineLayout,
		     .renderPass          = nullptr},
		    {.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

		graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
	}

	void createCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo{.flags= vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		                                   .queueFamilyIndex = queueIndex};
		commandPool = vk::raii::CommandPool(device, poolInfo);
	}


	void createVertexBuffer()
	{
		vk::BufferCreateInfo bufferInfo{.size = sizeof(vertices[0]) * vertices.size(),
										.usage = vk::BufferUsageFlagBits::eVertexBuffer,
										.sharingMode = vk::SharingMode::eExclusive};
		vertexBuffer = vk::raii::Buffer(device, bufferInfo);

		vk::MemoryRequirements memRequirements = vertexBuffer.getMemoryRequirements();
		vk::MemoryAllocateInfo memoryAllocateInfo{	.allocationSize = memRequirements.size,
													.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
													vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)};
		vertexBufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);

		vertexBuffer.bindMemory(*vertexBufferMemory, 0);

		void *data = vertexBufferMemory.mapMemory(0, bufferInfo.size);
		memcpy(data, vertices.data(), bufferInfo.size);
		vertexBufferMemory.unmapMemory();
	}

	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
	{
		//find the exact type of GPU memory we want
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		throw std::runtime_error("failed to find suitable memory type!");
	}


	void createCommandBuffers()
	{
		commandBuffers.clear();
		vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
		commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
	}

	void transition_image_layout(
	    uint32_t                imageIndex,
	    vk::ImageLayout         old_layout,
	    vk::ImageLayout         new_layout,
	    vk::AccessFlags2        src_access_mask,
	    vk::AccessFlags2        dst_access_mask,
	    vk::PipelineStageFlags2 src_stage_mask,
	    vk::PipelineStageFlags2 dst_stage_mask)
	{
		vk::ImageMemoryBarrier2 barrier = {
		    .srcStageMask        = src_stage_mask,
		    .srcAccessMask       = src_access_mask,
		    .dstStageMask        = dst_stage_mask,
		    .dstAccessMask       = dst_access_mask,
		    .oldLayout           = old_layout,
		    .newLayout           = new_layout,
		    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .image               = swapChainImages[imageIndex],
		    .subresourceRange    = {
		           .aspectMask     = vk::ImageAspectFlagBits::eColor,
		           .baseMipLevel   = 0,
		           .levelCount     = 1,
		           .baseArrayLayer = 0,
		           .layerCount     = 1}};
		vk::DependencyInfo dependency_info = {
		    .dependencyFlags         = {},
		    .imageMemoryBarrierCount = 1,
		    .pImageMemoryBarriers    = &barrier};
		//sync step: make sure we wait until transition completes, we should not start drawing on image before transition to this memory state
		commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
	}

	void recordCommandBuffer(uint32_t imageIndex)
	{
		auto &commandBuffer = commandBuffers[frameIndex];
		commandBuffer.begin({});
		// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
		// The memory layout in the gpu can be fast to write, or fast to read
		// Transition the swap chain recent image from any mode (eUndefined) to 
		// fast to write mode = eColorAttachmentOptimal
		// as we want to write this image
		transition_image_layout(
		    imageIndex,
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eColorAttachmentOptimal,
		    {},                                                        // srcAccessMask (no need to wait for previous operations)
		    vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput         // dstStage
		);

		//set the color to black(RGB: 000), solid color
		vk::ClearValue              clearColor     = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		//load operation: clear the image to black, basically we reseting the canvas to black before painting on it
		vk::RenderingAttachmentInfo attachmentInfo = {
		    .imageView   = swapChainImageViews[imageIndex],
		    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		    .loadOp      = vk::AttachmentLoadOp::eClear,
		    .storeOp     = vk::AttachmentStoreOp::eStore,
		    .clearValue  = clearColor};
		vk::RenderingInfo renderingInfo = {
		    .renderArea           = {.offset = {0, 0}, .extent = swapChainExtent},
		    .layerCount           = 1,
		    .colorAttachmentCount = 1,
		    .pColorAttachments    = &attachmentInfo};
		
		//bind the graphics pipeline to this command buffer: "here is the tool to work with",
		//set area to draw on, draw 3 vertexes
		commandBuffer.beginRendering(renderingInfo);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
		commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
		commandBuffer.draw(3, 1, 0, 0);
		commandBuffer.endRendering();
		// After rendering, transition the swapchain image to PRESENT_SRC - to fast to read memory for screen presentation
		transition_image_layout(
		    imageIndex,
		    vk::ImageLayout::eColorAttachmentOptimal,
		    vk::ImageLayout::ePresentSrcKHR,
		    vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
		    {},                                                        // dstAccessMask
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		    vk::PipelineStageFlagBits2::eBottomOfPipe                  // dstStage
		);
		commandBuffer.end();
	}



	void drawFrame()
	{
		// Note: inFlightFences, presentCompleteSemaphores, and commandBuffers are indexed by frameIndex,
		//       while renderFinishedSemaphores is indexed by imageIndex
		// main sync reason: dont render until image is ready to write into (finished presenting)
		// 					dont present image until rendering finished 
		auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!");
		}

		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

		// Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
		// here and does not need to be caught by an exception.
		if (result == vk::Result::eErrorOutOfDateKHR)
		{
			recreateSwapChain();
			return;
		}
		// On other success codes than eSuccess and eSuboptimalKHR we just throw an exception.
		// On any error code, aquireNextImage already threw an exception.
		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		{
			assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		// Only reset the fence if we are submitting work
		device.resetFences(*inFlightFences[frameIndex]);

		commandBuffers[frameIndex].reset();
		recordCommandBuffer(imageIndex);

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = 1,
		                                  .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
		                                  .pWaitDstStageMask    = &waitDestinationStageMask,
		                                  .commandBufferCount   = 1,
		                                  .pCommandBuffers      = &*commandBuffers[frameIndex],
		                                  .signalSemaphoreCount = 1,
		                                  .pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex]};
		queue.submit(submitInfo, *inFlightFences[frameIndex]);

		const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
		                                        .pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
		                                        .swapchainCount     = 1,
		                                        .pSwapchains        = &*swapChain,
		                                        .pImageIndices      = &imageIndex};
		result = queue.presentKHR(presentInfoKHR);
		// Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
		// here and does not need to be caught by an exception.
		if ((result == vk::Result::eSuboptimalKHR) or (result == vk::Result::eErrorOutOfDateKHR) or framebufferResized)
		{
			framebufferResized = false; // value set by GLFW window callback
			recreateSwapChain();
		}
		else
		{
			// There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
			assert(result == vk::Result::eSuccess);
		}
		frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
		//cpu is going in round on fences, as soon as the previous is done, we can go to next one
		// max frames in flight is less then the swapchain.
		// we always wait for fence inflight[frame] to be done presenting (gpu signals) before we can start recording next image
		// in flight 2 is usual setting, the larger the number the higher the latency of user input ->change on screen.
	}

	void createSyncObjects()
	{
		assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
		}
	}

	void cleanupSwapChain()
	{
		swapChainImageViews.clear();
		swapChain = nullptr;
	}

	void recreateSwapChain()
	{
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		device.waitIdle(); // finish current rendering jobs already issues

		cleanupSwapChain();
		createSwapChain();
		createImageViews();
	}


	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			drawFrame();
		}
			device.waitIdle();        // wait for device to finish operations before destroying resources
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
