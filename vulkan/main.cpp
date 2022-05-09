#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <set> // temporary

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include "vulkan/vulkan.h"

#define S_BACKBUFFER_COUNT 2
#define S_FRAME_COUNT 3

#ifndef MV_ASSERT
#include <assert.h>
#define MV_VULKAN(x) assert(x == VK_SUCCESS)
#endif

#define MV_ENABLE_VALIDATION_LAYERS

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
{
    printf("validation layer: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}


//-----------------------------------------------------------------------------
// Constant Buffer Struct
//-----------------------------------------------------------------------------
struct ConstantBuffer
{
    float x_offset = 0.0f;
    float y_offset = 0.0f;
    float padding[2] = { 0.0f, 0.0f };
};

struct QueueFamilyIndices
{
    int graphicsFamily = -1;
    int presentFamily = -1;
};

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------
static HWND           g_hwnd;
static const int      g_width = 1024;
static const int      g_height = 768;
static ConstantBuffer g_vertexOffset = { 0.0f, 0.0f };

static const char*                validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
static const char*                extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

static VkInstance                 g_instance;
static VkSurfaceKHR               g_surface;
static VkDebugUtilsMessengerEXT   g_debugMessenger;
static VkPhysicalDeviceProperties g_deviceProperties;
static VkPhysicalDevice           g_physicalDevice;
static unsigned                   g_graphicsQueueFamily;
static VkDevice                   g_logicalDevice;
static VkQueue                    g_graphicsQueue;
static VkQueue                    g_presentQueue;
static unsigned                   g_minImageCount;
static VkSwapchainKHR             g_swapChain;
static VkImage*                   g_swapChainImages;
static VkImageView*               g_swapChainImageViews;
static VkFormat                   g_swapChainImageFormat;
static VkExtent2D                 g_swapChainExtent;
static VkCommandPool              g_commandPool;
static VkCommandBuffer*           g_commandBuffers;
static VkDescriptorPool           g_descriptorPool;
static VkRenderPass               g_renderPass;
static VkImage                    g_depthImage;
static VkDeviceMemory             g_depthImageMemory;
static VkImageView                g_depthImageView;
static VkFramebuffer*             g_swapChainFramebuffers;
static VkSemaphore                g_imageAvailableSemaphores[S_FRAME_COUNT]; // syncronize rendering to image when already rendering to image
static VkSemaphore                g_renderFinishedSemaphores[S_FRAME_COUNT]; // syncronize render/present
static VkFence                    g_inFlightFences[S_FRAME_COUNT];
static VkFence                    g_imagesInFlight[S_FRAME_COUNT];
static unsigned                   g_currentImageIndex = 0;
static size_t                     g_currentFrame = 0;
static VkViewport                 g_viewport;

static VkPipelineLayout g_pipelineLayout;
static VkPipeline g_pipeline;

static VkBuffer g_indexBuffer;
static VkDeviceMemory g_indexDeviceMemory;

static VkBuffer g_vertexBuffer;
static VkDeviceMemory g_vertexDeviceMemory;

static VkVertexInputAttributeDescription g_attributeDescriptions[2];
static VkVertexInputBindingDescription   g_bindingDescriptions[1];
static VkShaderModule g_vertexShaderModule;
static VkShaderModule g_pixelShaderModule;


//-----------------------------------------------------------------------------
// Vertex Data and Index Data
//-----------------------------------------------------------------------------
static const float g_vertexData[] = { // x, y, u, v
     -0.5f,  0.5f, 0.0f, 0.0f,
     -0.5f, -0.5f, 0.0f, 1.0f,
      0.5f, -0.5f, 1.0f, 1.0f,
      0.5f,  0.5f, 1.0f, 0.0f,
};

static const unsigned g_indices[] = { 
    0, 1, 2,
    0, 2, 3
};

//-----------------------------------------------------------------------------
// Image Data
//-----------------------------------------------------------------------------
unsigned char image[] = {
    255,   0,   0, 255,
      0, 255,   0, 255,
      0,   0, 255, 255,
    255,   0, 255, 255
};

//-----------------------------------------------------------------------------
// Windows Procedure
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
static void               create_swapchain();
static VkImageView        create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
inline unsigned           get_max(unsigned a, unsigned b) { return a > b ? a : b;}
inline unsigned           get_min(unsigned a, unsigned b) { return a < b ? a : b;}
static void               create_render_pass();
static unsigned           find_memory_type(unsigned typeFilter, VkMemoryPropertyFlags properties);
static void               create_image(unsigned width, unsigned height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
static char*              read_file(const char* file, unsigned& size, const char* mode);

int main()
{
    //char layerPath[1024];
    char* layerPath = getenv("VK_LAYER_PATH");
    int result = putenv(layerPath);
    printf(layerPath);
    //-----------------------------------------------------------------------------
    // Create and Open Window
    //-----------------------------------------------------------------------------
    {
        WNDCLASSEXW winClass = {};
        winClass.cbSize = sizeof(WNDCLASSEXW);
        winClass.style = CS_HREDRAW | CS_VREDRAW;
        winClass.lpfnWndProc = &WndProc;
        winClass.hInstance = ::GetModuleHandle(nullptr);
        winClass.hIcon = ::LoadIconW(0, IDI_APPLICATION);
        winClass.hCursor = ::LoadCursorW(0, IDC_ARROW);
        winClass.lpszClassName = L"MyWindowClass";
        winClass.hIconSm = ::LoadIconW(0, IDI_APPLICATION);

        if (!::RegisterClassExW(&winClass)) {
            ::MessageBoxA(0, "RegisterClassEx failed", "Fatal Error", MB_OK);
            return ::GetLastError();
        }

        RECT initialRect = { 0, 0, g_width, g_height };
        ::AdjustWindowRectEx(&initialRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);
        LONG initialWidth = initialRect.right - initialRect.left;
        LONG initialHeight = initialRect.bottom - initialRect.top;

        g_hwnd = ::CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
            winClass.lpszClassName,
            L"Directx 11",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT,
            initialWidth,
            initialHeight,
            0, 0, ::GetModuleHandle(nullptr), 0);

        if (!g_hwnd) 
        {
            ::MessageBoxA(0, "CreateWindowEx failed", "Fatal Error", MB_OK);
            return ::GetLastError();
        }
    }

    //-----------------------------------------------------------------------------
    // create vulkan instance
    //-----------------------------------------------------------------------------
    {
#ifdef MV_ENABLE_VALIDATION_LAYERS
        unsigned layerCount = 0u;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        auto availableLayers = (VkLayerProperties*)malloc(sizeof(VkLayerProperties)*layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

        bool validationLayersFound = false;
        for(int i = 0; i < layerCount; i++)
        {
            auto blah = availableLayers[i].layerName;
            if(strcmp(availableLayers[i].layerName, "VK_LAYER_KHRONOS_validation") == 0)
            {
                validationLayersFound = true;
                break;
            }
        }
        assert(validationLayersFound);
        free(availableLayers);
#endif

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        const char* enabledExtensions[] = { 
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#ifdef MV_ENABLE_VALIDATION_LAYERS
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
        };

        createInfo.enabledExtensionCount = 3u;
        createInfo.ppEnabledExtensionNames = enabledExtensions;

        // Setup debug messenger for vulkan instance
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
#ifdef MV_ENABLE_VALIDATION_LAYERS
        createInfo.enabledLayerCount = 1u;
        createInfo.ppEnabledLayerNames = validationLayers;
        createInfo.pNext = VK_NULL_HANDLE;

        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        debugCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#elif
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = VK_NULL_HANDLE;
#endif

        MV_VULKAN(vkCreateInstance(&createInfo, nullptr, &g_instance));
    }

    //-----------------------------------------------------------------------------
    // setup debug messenger
    //-----------------------------------------------------------------------------
#ifdef MV_ENABLE_VALIDATION_LAYERS
    {

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = VK_NULL_HANDLE;

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_instance, "vkCreateDebugUtilsMessengerEXT");
        assert(func != nullptr && "failed to set up debug messenger!");
        MV_VULKAN(func(g_instance, &createInfo, nullptr, &g_debugMessenger));
    }
#endif

    //-----------------------------------------------------------------------------
    // create surface
    //-----------------------------------------------------------------------------
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = NULL;
    surfaceCreateInfo.flags = 0;
    surfaceCreateInfo.hinstance = ::GetModuleHandle(NULL);
    surfaceCreateInfo.hwnd = g_hwnd;
    MV_VULKAN(vkCreateWin32SurfaceKHR(g_instance, &surfaceCreateInfo, nullptr, &g_surface));

    //-----------------------------------------------------------------------------
    // create physical device
    //-----------------------------------------------------------------------------
    {
        unsigned deviceCount = 0u;
        MV_VULKAN(vkEnumeratePhysicalDevices(g_instance, &deviceCount, nullptr));
        assert(deviceCount > 0 && "failed to find GPUs with Vulkan support!");

        //-----------------------------------------------------------------------------
        // check if device is suitable
        //-----------------------------------------------------------------------------
        auto devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice)*deviceCount);
        MV_VULKAN(vkEnumeratePhysicalDevices(g_instance, &deviceCount, devices));

        //for(int i = 0; i < deviceCount; i++)
        for(int i = 0; i < 1; i++)
        {
            QueueFamilyIndices indices = find_queue_families(devices[i]);

            //-----------------------------------------------------------------------------
            // check if device supports extensions
            //-----------------------------------------------------------------------------
            bool extensionsSupported = false;
            {
                unsigned extensionCount;
                MV_VULKAN(vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extensionCount, nullptr));

                auto availableExtensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*extensionCount);
                MV_VULKAN(vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extensionCount, availableExtensions));

                // TODO: ensure required extensions are found
                extensionsSupported = true;
            }


            bool swapChainAdequate = false;
            if (extensionsSupported)
            {
                swapChainAdequate = true;
                //SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
                //swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }

            vkGetPhysicalDeviceProperties(devices[i], &g_deviceProperties);

            if (extensionsSupported && swapChainAdequate && g_deviceProperties.limits.maxPushConstantsSize >= 256)
            {
                g_physicalDevice = devices[0];
                // TODO: add logic to pick best device (not the last device)
            }
        }

        assert(g_physicalDevice != VK_NULL_HANDLE && "failed to find a suitable GPU!");
    }

    //-----------------------------------------------------------------------------
    // create logical device
    //-----------------------------------------------------------------------------
    {
        QueueFamilyIndices indices = find_queue_families(g_physicalDevice);
        g_graphicsQueueFamily = indices.graphicsFamily;

        VkDeviceQueueCreateInfo queueCreateInfos[2];
        std::set<unsigned> uniqueQueueFamilies = { (unsigned)indices.graphicsFamily, (unsigned)indices.presentFamily };

        float queuePriority = 1.0f;
        for(int i = 0; i < uniqueQueueFamilies.size(); i++)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = i;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos[i] = queueCreateInfo;
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        {
            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

            createInfo.queueCreateInfoCount = 1u;
            createInfo.pQueueCreateInfos = queueCreateInfos;
            createInfo.pEnabledFeatures = &deviceFeatures;
            createInfo.enabledExtensionCount = 1u;
            createInfo.ppEnabledExtensionNames = extensions;
            createInfo.enabledLayerCount = 0;
#ifdef MV_ENABLE_VALIDATION_LAYERS
            createInfo.enabledLayerCount = 2u;
            createInfo.ppEnabledLayerNames = validationLayers;
#endif
            MV_VULKAN(vkCreateDevice(g_physicalDevice, &createInfo, nullptr, &g_logicalDevice));
        }

        vkGetDeviceQueue(g_logicalDevice, indices.graphicsFamily, 0, &g_graphicsQueue);
        vkGetDeviceQueue(g_logicalDevice, indices.presentFamily, 0, &g_presentQueue);
    }

    //-----------------------------------------------------------------------------
    // create swapchain
    //-----------------------------------------------------------------------------
    create_swapchain();

    //-----------------------------------------------------------------------------
    // create command pool and command buffers
    //-----------------------------------------------------------------------------
    QueueFamilyIndices queueFamilyIndices = find_queue_families(g_physicalDevice);

    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    MV_VULKAN(vkCreateCommandPool(g_logicalDevice, &commandPoolInfo, nullptr, &g_commandPool));

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = g_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (unsigned)(g_minImageCount);

    g_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*g_minImageCount);

    MV_VULKAN(vkAllocateCommandBuffers(g_logicalDevice, &allocInfo, g_commandBuffers));

    //-----------------------------------------------------------------------------
    // create descriptor pool
    //-----------------------------------------------------------------------------
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000 }
    };
    VkDescriptorPoolCreateInfo descPoolInfo = {};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descPoolInfo.maxSets = 1000 * 11;
    descPoolInfo.poolSizeCount = 11u;
    descPoolInfo.pPoolSizes = poolSizes;

    MV_VULKAN(vkCreateDescriptorPool(g_logicalDevice, &descPoolInfo, nullptr, &g_descriptorPool));

    //-----------------------------------------------------------------------------
    // create render pass
    //-----------------------------------------------------------------------------
    create_render_pass();

    //-----------------------------------------------------------------------------
    // create depth resources
    //-----------------------------------------------------------------------------
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    create_image(g_swapChainExtent.width, g_swapChainExtent.height, depthFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        g_depthImage, g_depthImageMemory);

    g_depthImageView = create_image_view(g_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    //-----------------------------------------------------------------------------
    // create frame buffers
    //-----------------------------------------------------------------------------
    g_swapChainFramebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*g_minImageCount);
    for (unsigned i = 0; i < g_minImageCount; i++)
    {
        VkImageView imageViews[] = { g_swapChainImageViews[i], g_depthImageView };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = g_renderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = imageViews;
        framebufferInfo.width = g_swapChainExtent.width;
        framebufferInfo.height = g_swapChainExtent.height;
        framebufferInfo.layers = 1;
        MV_VULKAN(vkCreateFramebuffer(g_logicalDevice, &framebufferInfo, nullptr, &g_swapChainFramebuffers[i]));
    }

    //-----------------------------------------------------------------------------
    // create syncronization primitives
    //-----------------------------------------------------------------------------
    for(int i = 0; i < S_FRAME_COUNT; i++)
        g_imagesInFlight[i] = VK_NULL_HANDLE;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (unsigned i = 0; i < S_FRAME_COUNT; i++)
    {
        MV_VULKAN(vkCreateSemaphore(g_logicalDevice, &semaphoreInfo, nullptr, &g_imageAvailableSemaphores[i]));
        MV_VULKAN(vkCreateSemaphore(g_logicalDevice, &semaphoreInfo, nullptr, &g_renderFinishedSemaphores[i]));
        MV_VULKAN(vkCreateFence(g_logicalDevice, &fenceInfo, nullptr, &g_inFlightFences[i]));
    }

    //-----------------------------------------------------------------------------
    // create vertex layout
    //-----------------------------------------------------------------------------
    g_bindingDescriptions[0].binding = 0;
    g_bindingDescriptions[0].stride = sizeof(float)*4;
    g_bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    g_attributeDescriptions[0].binding = 0;
    g_attributeDescriptions[0].location = 0;
    g_attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    g_attributeDescriptions[0].offset = 0u;

    g_attributeDescriptions[1].binding = 0;
    g_attributeDescriptions[1].location = 1;
    g_attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    g_attributeDescriptions[1].offset = sizeof(float)*2;

    //-----------------------------------------------------------------------------
    // create pipeline layout
    //-----------------------------------------------------------------------------
    
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;

    MV_VULKAN(vkCreatePipelineLayout(g_logicalDevice, &pipelineLayoutInfo, nullptr, &g_pipelineLayout));

    //-----------------------------------------------------------------------------
    // create pipeline
    //-----------------------------------------------------------------------------
    {
        unsigned vertexFileSize = 0u;
        unsigned pixelFileSize = 0u;
        auto vertexShaderCode = read_file("simple.vert.spv", vertexFileSize, "rb");
        auto pixelShaderCode = read_file("simple.frag.spv", pixelFileSize, "rb");

        {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = vertexFileSize;
            createInfo.pCode = (const uint32_t*)(vertexShaderCode);
            assert(vkCreateShaderModule(g_logicalDevice, &createInfo, nullptr, &g_vertexShaderModule) == VK_SUCCESS);
        }

        {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = pixelFileSize;
            createInfo.pCode = (const uint32_t*)(pixelShaderCode);
            assert(vkCreateShaderModule(g_logicalDevice, &createInfo, nullptr, &g_pixelShaderModule) == VK_SUCCESS);
        }

        //---------------------------------------------------------------------
        // input assembler stage
        //---------------------------------------------------------------------
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1u;
        vertexInputInfo.vertexAttributeDescriptionCount = 2u;
        vertexInputInfo.pVertexBindingDescriptions = g_bindingDescriptions;
        vertexInputInfo.pVertexAttributeDescriptions = g_attributeDescriptions;

        //---------------------------------------------------------------------
        // vertex shader stage
        //---------------------------------------------------------------------
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = g_vertexShaderModule;
        vertShaderStageInfo.pName = "main";

        //---------------------------------------------------------------------
        // tesselation stage
        //---------------------------------------------------------------------

        //---------------------------------------------------------------------
        // geometry shader stage
        //---------------------------------------------------------------------

        //---------------------------------------------------------------------
        // rasterization stage
        //---------------------------------------------------------------------

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = g_height;
        viewport.width = g_width;
        viewport.height = -g_height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent.width = (unsigned)viewport.width;
        scissor.extent.height = (unsigned)viewport.y;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        //---------------------------------------------------------------------
        // fragment shader stage
        //---------------------------------------------------------------------
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = g_pixelShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back = {}; // Optional

        //---------------------------------------------------------------------
        // color blending stage
        //---------------------------------------------------------------------
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        //---------------------------------------------------------------------
        // Create Pipeline
        //---------------------------------------------------------------------
        VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertShaderStageInfo,
            fragShaderStageInfo
        };


        VkDynamicState dynamicStateEnables[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 3;
        dynamicState.pDynamicStates = dynamicStateEnables;
        
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2u;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = g_pipelineLayout;
        pipelineInfo.renderPass = g_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.pDepthStencilState = &depthStencil;

        MV_VULKAN(vkCreateGraphicsPipelines(g_logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_pipeline));

        // no longer need these
        vkDestroyShaderModule(g_logicalDevice, g_vertexShaderModule, nullptr);
        vkDestroyShaderModule(g_logicalDevice, g_pixelShaderModule, nullptr);
        g_vertexShaderModule = VK_NULL_HANDLE;
        g_pixelShaderModule = VK_NULL_HANDLE;
    }

    //-----------------------------------------------------------------------------
    // create vertex buffer
    //-----------------------------------------------------------------------------
    {

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferDeviceMemory;

        { // create staging buffer

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(float)*16;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            MV_VULKAN(vkCreateBuffer(g_logicalDevice, &bufferInfo, nullptr, &stagingBuffer));

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(g_logicalDevice, stagingBuffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            MV_VULKAN(vkAllocateMemory(g_logicalDevice, &allocInfo, nullptr, &stagingBufferDeviceMemory));
            MV_VULKAN(vkBindBufferMemory(g_logicalDevice, stagingBuffer, stagingBufferDeviceMemory, 0));

            void* mapping;
            MV_VULKAN(vkMapMemory(g_logicalDevice, stagingBufferDeviceMemory, 0, bufferInfo.size, 0, &mapping));
            memcpy(mapping, g_vertexData, bufferInfo.size);
            vkUnmapMemory(g_logicalDevice, stagingBufferDeviceMemory);
        }

        { // create final buffer

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(float)*16;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            MV_VULKAN(vkCreateBuffer(g_logicalDevice, &bufferInfo, nullptr, &g_vertexBuffer));

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(g_logicalDevice, g_vertexBuffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            MV_VULKAN(vkAllocateMemory(g_logicalDevice, &allocInfo, nullptr, &g_vertexDeviceMemory));
            MV_VULKAN(vkBindBufferMemory(g_logicalDevice, g_vertexBuffer, g_vertexDeviceMemory, 0));
        }

        // copy buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = g_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(g_logicalDevice, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = sizeof(float)*16;
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, g_vertexBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkDeviceWaitIdle(g_logicalDevice);

        vkFreeCommandBuffers(g_logicalDevice, g_commandPool, 1, &commandBuffer);
    }

    //-----------------------------------------------------------------------------
    // create index buffer
    //-----------------------------------------------------------------------------
    {

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferDeviceMemory;

        { // create staging buffer

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(unsigned)*6;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            MV_VULKAN(vkCreateBuffer(g_logicalDevice, &bufferInfo, nullptr, &stagingBuffer));

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(g_logicalDevice, stagingBuffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            MV_VULKAN(vkAllocateMemory(g_logicalDevice, &allocInfo, nullptr, &stagingBufferDeviceMemory));
            MV_VULKAN(vkBindBufferMemory(g_logicalDevice, stagingBuffer, stagingBufferDeviceMemory, 0));

            void* mapping;
            MV_VULKAN(vkMapMemory(g_logicalDevice, stagingBufferDeviceMemory, 0, bufferInfo.size, 0, &mapping));
            memcpy(mapping, g_indices, bufferInfo.size);
            vkUnmapMemory(g_logicalDevice, stagingBufferDeviceMemory);
        }

        { // create final buffer

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(unsigned)*6;
            bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            MV_VULKAN(vkCreateBuffer(g_logicalDevice, &bufferInfo, nullptr, &g_indexBuffer));

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(g_logicalDevice, g_indexBuffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            MV_VULKAN(vkAllocateMemory(g_logicalDevice, &allocInfo, nullptr, &g_indexDeviceMemory));
            MV_VULKAN(vkBindBufferMemory(g_logicalDevice, g_indexBuffer, g_indexDeviceMemory, 0));
        }

        // copy buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = g_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(g_logicalDevice, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = sizeof(unsigned)*6;
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, g_indexBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkDeviceWaitIdle(g_logicalDevice);

        vkFreeCommandBuffers(g_logicalDevice, g_commandPool, 1, &commandBuffer);
    }

    //-----------------------------------------------------------------------------
    // Main loop
    //-----------------------------------------------------------------------------
    bool isRunning = true;
    while (isRunning)
    {
        // poll for and process events
        MSG msg = {};
        while (::PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                isRunning = false;
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }

        //-----------------------------------------------------------------------------
        // begin frame (wait for fences and acquire next image)
        //-----------------------------------------------------------------------------

        MV_VULKAN(vkWaitForFences(g_logicalDevice, 1, &g_inFlightFences[g_currentFrame], VK_TRUE, UINT64_MAX));

        MV_VULKAN(vkAcquireNextImageKHR(g_logicalDevice, g_swapChain, UINT64_MAX, g_imageAvailableSemaphores[g_currentFrame],
            VK_NULL_HANDLE, &g_currentImageIndex));

        if (g_imagesInFlight[g_currentImageIndex] != VK_NULL_HANDLE)
            MV_VULKAN(vkWaitForFences(g_logicalDevice, 1, &g_imagesInFlight[g_currentImageIndex], VK_TRUE, UINT64_MAX));

        // just in case the acquired image is out of order
        g_imagesInFlight[g_currentImageIndex] = g_inFlightFences[g_currentFrame];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        MV_VULKAN(vkBeginCommandBuffer(g_commandBuffers[g_currentImageIndex], &beginInfo));

        //---------------------------------------------------------------------
        // begin pass
        //---------------------------------------------------------------------
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = g_renderPass;
        renderPassInfo.framebuffer = g_swapChainFramebuffers[g_currentImageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = g_swapChainExtent;

        VkClearValue clearValues[2];
        clearValues[0].color.float32[0] = 0.0f;
        clearValues[0].color.float32[1] = 0.0f;
        clearValues[0].color.float32[2] = 0.0f;
        clearValues[0].color.float32[3] = 0.0f;
        clearValues[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = 2;
        renderPassInfo.pClearValues = clearValues;

        VkRect2D scissor{};
        scissor.extent = g_swapChainExtent;

        g_viewport.x = 0.0f;
        g_viewport.y = g_swapChainExtent.height;
        g_viewport.width = g_swapChainExtent.width;
        g_viewport.height = -(int)g_swapChainExtent.height;

        vkCmdBeginRenderPass(g_commandBuffers[g_currentImageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(g_commandBuffers[g_currentImageIndex], 0, 1, &g_viewport);
        vkCmdSetScissor(g_commandBuffers[g_currentImageIndex], 0, 1, &scissor);
        vkCmdSetDepthBias(g_commandBuffers[g_currentImageIndex], 0.0f, 0.0f, 0.0f);
        vkCmdBindPipeline(g_commandBuffers[g_currentImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline);

        static VkDeviceSize offsets = { 0 };
        VkCommandBuffer commandBuffer = g_commandBuffers[g_currentImageIndex];
        vkCmdBindIndexBuffer(g_commandBuffers[g_currentImageIndex], g_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(g_commandBuffers[g_currentImageIndex], 0, 1, &g_vertexBuffer, &offsets);
        vkCmdDrawIndexed(g_commandBuffers[g_currentImageIndex], 6, 1, 0, 0, 0);


        vkCmdEndRenderPass(g_commandBuffers[g_currentImageIndex]);

        //---------------------------------------------------------------------
        // submit command buffers & present
        //---------------------------------------------------------------------
        MV_VULKAN(vkEndCommandBuffer(g_commandBuffers[g_currentImageIndex]));

        VkSemaphore waitSemaphores[] = { g_imageAvailableSemaphores[g_currentFrame] };
        VkSemaphore signalSemaphores[] = { g_renderFinishedSemaphores[g_currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &g_commandBuffers[g_currentImageIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        MV_VULKAN(vkResetFences(g_logicalDevice, 1, &g_inFlightFences[g_currentFrame]));
        MV_VULKAN(vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, g_inFlightFences[g_currentFrame]));

        // present
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = { g_swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &g_currentImageIndex;
        VkResult result = vkQueuePresentKHR(g_presentQueue, &presentInfo);
        g_currentFrame = (g_currentFrame + 1) % S_FRAME_COUNT;

    }

}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    switch (msg)
    {
    case WM_KEYDOWN:
    {
        if (wparam == VK_ESCAPE)
            ::DestroyWindow(hwnd);
        if (wparam == VK_UP)
            g_vertexOffset.y_offset += 0.01f;
        if (wparam == VK_DOWN)
            g_vertexOffset.y_offset -= 0.01f;
        if (wparam == VK_LEFT)
            g_vertexOffset.x_offset -= 0.01f;
        if (wparam == VK_RIGHT)
            g_vertexOffset.x_offset += 0.01f;
        break;
    }
    case WM_DESTROY:
    {
        ::PostQuitMessage(0);
        break;
    }
    default:
        result = ::DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return result;
}

static QueueFamilyIndices 
find_queue_families(VkPhysicalDevice device)
{

    QueueFamilyIndices indices;

    unsigned queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    auto queueFamilies = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties)*queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

    for(int i = 0; i < queueFamilyCount; i++)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 presentSupport = false;
        MV_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, g_surface, &presentSupport));

        if (presentSupport)
            indices.presentFamily = i;

        if (indices.graphicsFamily > -1 && indices.presentFamily > -1) // complete
            break;

        i++;
    }
    free(queueFamilies);
    return indices;
}

static void 
create_swapchain()
{

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR*      formats;
        VkPresentModeKHR*        presentModes;
    };
   
    //-----------------------------------------------------------------------------
    // query swapchain support
    //-----------------------------------------------------------------------------
    SwapChainSupportDetails swapChainSupport;
    MV_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physicalDevice, g_surface, &swapChainSupport.capabilities));

    unsigned formatCount;
    MV_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, nullptr));

    // todo: put in appropriate spot
    VkBool32 presentSupport = false;
    MV_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(g_physicalDevice, 0, g_surface, &presentSupport));
    assert(formatCount > 0);
    swapChainSupport.formats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR)*formatCount);
    MV_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, swapChainSupport.formats));

    unsigned presentModeCount = 0u;
    MV_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_surface, &presentModeCount, nullptr));
    assert(presentModeCount > 0);
    swapChainSupport.presentModes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR)*presentModeCount);
    MV_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_surface, &presentModeCount, swapChainSupport.presentModes));

    // choose swap surface Format
    VkSurfaceFormatKHR surfaceFormat = swapChainSupport.formats[0];
    for(int i = 0 ; i < formatCount; i++)
    {
        const auto& availableFormat = swapChainSupport.formats[i];
        if (availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surfaceFormat = availableFormat;
            break;
        }
    }

    // chose swap present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for(int i = 0 ; i < presentModeCount; i++)
    {
        const auto& availablePresentMode = swapChainSupport.presentModes[i];
        if (availablePresentMode == VK_PRESENT_MODE_FIFO_KHR)
        {
            presentMode = availablePresentMode;
            break;
        }
    }

    // chose swap extent
    VkExtent2D extent;
    if (swapChainSupport.capabilities.currentExtent.width != UINT32_MAX)
        extent = swapChainSupport.capabilities.currentExtent;
    else
    {
        VkExtent2D actualExtent = {
            (unsigned)g_width,
            (unsigned)g_height
        };

        actualExtent.width = get_max(swapChainSupport.capabilities.minImageExtent.width, get_min(swapChainSupport.capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = get_max(swapChainSupport.capabilities.minImageExtent.height, get_min(swapChainSupport.capabilities.maxImageExtent.height, actualExtent.height));

        extent = actualExtent;
    }

    g_minImageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && g_minImageCount > swapChainSupport.capabilities.maxImageCount)
        g_minImageCount = swapChainSupport.capabilities.maxImageCount;

    {
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = g_surface;
        createInfo.minImageCount = g_minImageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = find_queue_families(g_physicalDevice);
        unsigned queueFamilyIndices[] = { (unsigned)indices.graphicsFamily, (unsigned)indices.presentFamily};

        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = VK_NULL_HANDLE;

        MV_VULKAN(vkCreateSwapchainKHR(g_logicalDevice, &createInfo, nullptr, &g_swapChain));
    }

    vkGetSwapchainImagesKHR(g_logicalDevice, g_swapChain, &g_minImageCount, nullptr);
    g_swapChainImages = (VkImage*)malloc(sizeof(VkImage)*g_minImageCount);

    vkGetSwapchainImagesKHR(g_logicalDevice, g_swapChain, &g_minImageCount, g_swapChainImages);

    g_swapChainImageFormat = surfaceFormat.format;
    g_swapChainExtent = extent;

    // creating image views
    g_swapChainImageViews = (VkImageView*)malloc(sizeof(VkImageView)*g_minImageCount);
    for (unsigned i = 0; i < g_minImageCount; i++)
        g_swapChainImageViews[i] = create_image_view(g_swapChainImages[i], g_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}

static VkImageView
create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.aspectMask = aspectFlags;

    VkImageView imageView;
    MV_VULKAN(vkCreateImageView(g_logicalDevice, &viewInfo, nullptr, &imageView));
    return imageView;
}

static void 
create_render_pass()
{
    VkAttachmentDescription attachments[2];

    // color attachment
    attachments[0].flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    attachments[0].format = g_swapChainImageFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // depth attachment
    attachments[1].flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference references[] = 
    {
        {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}
    };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = references;
    subpass.pDepthStencilAttachment = &references[1];

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2u;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 0;
    renderPassInfo.pDependencies = VK_NULL_HANDLE;

    MV_VULKAN(vkCreateRenderPass(g_logicalDevice, &renderPassInfo, nullptr, &g_renderPass));
}

static void
create_image(unsigned width, unsigned height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0; // Optional

    MV_VULKAN(vkCreateImage(g_logicalDevice, &imageInfo, nullptr, &image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(g_logicalDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    MV_VULKAN(vkAllocateMemory(g_logicalDevice, &allocInfo, nullptr, &imageMemory));
    MV_VULKAN(vkBindImageMemory(g_logicalDevice, image, imageMemory, 0));
}

unsigned
find_memory_type(unsigned typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(g_physicalDevice, &memProperties);

    for (unsigned i = 0; i < memProperties.memoryTypeCount; i++) 
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) 
        {
            return i;
        }
    }
    assert(false && "failed to find suitable memory type!");
    return 0u;
}

static char*
read_file(const char* file, unsigned& size, const char* mode)
{
    FILE* dataFile = fopen(file, mode);

    if (dataFile == nullptr)
    {
        assert(false && "File not found.");
        return nullptr;
    }

    // obtain file size:
    fseek(dataFile, 0, SEEK_END);
    size = ftell(dataFile);
    fseek(dataFile, 0, SEEK_SET);

    // allocate memory to contain the whole file:
    char* data = new char[size];

    // copy the file into the buffer:
    size_t result = fread(data, sizeof(char), size, dataFile);
    if (result != size)
    {
        if (feof(dataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(dataFile)) {
            perror("Error reading test.bin");
        }
        assert(false && "File not read.");
    }

    fclose(dataFile);

    return data;
}