#include <memory>
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;

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
	vk::raii::Context  context;
	vk::raii::Instance instance = nullptr;

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
	}

	void createInstance()
	{
		/* 1. step is to create an instance: a vulkan application instance.
		  The style of passing meta data to fopr the creation, is via predefined structs.
		  designated initalizers (C++20) are used, so that some of the fields be left default
		  GLFW is used to manage windows, it tells Vulkan which extensions to use to be able to
		  create and use the windows on this specific platform.
		*/
		
		constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "hello Triangle",
		                                      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		                                      .pEngineName        = "No Engine",
		                                      .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
		                                      .apiVersion         = vk::ApiVersion14};

		// Get the required instance extensions from GLFW.
		uint32_t glfwExtensionCount = 0;
		auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		// Check if the required GLFW extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		for (uint32_t i = 0; i < glfwExtensionCount; ++i)
		{
			if (std::ranges::none_of(extensionProperties,
			                         [glfwExtension = glfwExtensions[i]](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, glfwExtension) == 0; }))
			{
				throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtensions[i]));
			}
		}

		vk::InstanceCreateInfo createInfo{
		    .pApplicationInfo        = &appInfo,
		    .enabledExtensionCount   = glfwExtensionCount,
		    .ppEnabledExtensionNames = glfwExtensions};

		// now defined everything we need to create a vulkan instace:
		// custom allocator callbacks are ignored, so it will use the standard one
		instance = vk::raii::Instance(context, createInfo);
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
