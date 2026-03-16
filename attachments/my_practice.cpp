#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <unordered_set>

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
		
		std::unordered_set<std::string> all_supported_layers;
		auto layerProperties    = context.enumerateInstanceLayerProperties(); //expected in ~20 layer structs
		for(auto const& layer: layerProperties){
			all_supported_layers.insert(layer.layerName);
		}
		for(auto required_layer: requiredLayers){
			std::string required_layer_str(required_layer);
			if(!all_supported_layers.contains(required_layer)){
				throw std::runtime_error(" A required layer is not supported:" + required_layer_str);
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

		// now defined everything we need to create a vulkan instace:
		// custom allocator callbacks are ignored, use the standard one
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
