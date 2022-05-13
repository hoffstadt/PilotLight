// PilotLight, v0.1 WIP
//   * Jonathan Hoffstadt

// Resources:
// - Website               https://www.vulkan.org/
// - Github                https://github.com/hoffstadt/PilotLight
// - Vulkan Tutorial       https://vulkan-tutorial.com/
// - Vulkan Dev            https://vkguide.dev/

// Implemented features:
//  [X] Platform: Windows
//  [X] Platform: Linux
//  [X] Index Buffers
//  [X] Vertex Buffers
//  [X] Depth Buffer
//  [X] Textures
//  [X] Multiple Frames in Flight
// Missing features:
//  [ ] Platform: MacOs
//  [ ] Constant Buffers
//  [ ] Resizing
//  [ ] Multiple draw calls
//  [ ] Multiple render targets
// Important:
//  - Requires Vulkan SDK and a driver that supports Vulkan 1.2 at least

/*
Index of this file:
// [SECTION] header mess
// [SECTION] platform specific variables
// [SECTION] general variables
// [SECTION] example specific variables
// [SECTION] example specific data
// [SECTION] general setup function declarations
// [SECTION] example specific setup function declarations
// [SECTION] general per-frame function declarations
// [SECTION] example specific per-frame function declarations
// [SECTION] entry point
// [SECTION] misc.
// [SECTION] helper functions
// [SECTION] general setup function implementations
// [SECTION] example specific setup function implementations
// [SECTION] general per-frame function implementations
// [SECTION] example per-frame function implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#elif defined(__APPLE__)
#else // linux
#include <xcb/xcb.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>  // sudo apt-get install libx11-dev
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>  // sudo apt-get install libxkbcommon-x11-dev libx11-xcb-dev
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <set> // temporary

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#else // linux
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "vulkan/vulkan.h"

#ifndef MV_ASSERT
#include <assert.h>
#define MV_VULKAN(x) assert(x == VK_SUCCESS)
#endif

//#define MV_ENABLE_VALIDATION_LAYERS

//-----------------------------------------------------------------------------
// [SECTION] platform specific variables
//-----------------------------------------------------------------------------
#ifdef _WIN32
static HWND g_hwnd;
#elif defined(__APPLE__)
#else // linux
static Display*          g_display;
static xcb_connection_t* g_connection;
static xcb_window_t      g_window;
static xcb_screen_t*     g_screen;
static xcb_atom_t        g_wm_protocols;
static xcb_atom_t        g_wm_delete_win;
#endif

//-----------------------------------------------------------------------------
// [SECTION] general variables
//-----------------------------------------------------------------------------
static const char*                      validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
static const char*                      extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
static const int                        g_width = 1024;
static const int                        g_height = 768;
static VkInstance                       g_instance;
static VkSurfaceKHR                     g_surface;
static VkDebugUtilsMessengerEXT         g_debugMessenger;
static VkPhysicalDeviceProperties       g_deviceProperties;
static VkPhysicalDeviceMemoryProperties g_memoryProperties;
static VkPhysicalDevice                 g_physicalDevice;
static unsigned                         g_graphicsQueueFamily;
static VkDevice                         g_logicalDevice;
static VkQueue                          g_graphicsQueue;
static VkQueue                          g_presentQueue;
static unsigned                         g_minImageCount;
static unsigned                         g_framesInFlight;
static VkSwapchainKHR                   g_swapChain;
static VkImage*                         g_swapChainImages;
static VkImageView*                     g_swapChainImageViews;
static VkFormat                         g_swapChainImageFormat;
static VkExtent2D                       g_swapChainExtent;
static VkCommandPool                    g_commandPool;
static VkCommandBuffer*                 g_commandBuffers;
static VkDescriptorPool                 g_descriptorPool;
static VkRenderPass                     g_renderPass;
static VkImage                          g_depthImage;
static VkDeviceMemory                   g_depthImageMemory;
static VkImageView                      g_depthImageView;
static VkFramebuffer*                   g_swapChainFramebuffers;
static VkSemaphore*                     g_imageAvailableSemaphores; // syncronize rendering to image when already rendering to image
static VkSemaphore*                     g_renderFinishedSemaphores; // syncronize render/present
static VkFence*                         g_inFlightFences;
static VkFence*                         g_imagesInFlight;
static unsigned                         g_currentImageIndex = 0;
static size_t                           g_currentFrame = 0;
static VkViewport                       g_viewport;
static bool                             g_running=true;

//-----------------------------------------------------------------------------
// [SECTION] example specific variables
//-----------------------------------------------------------------------------
static VkBuffer                          g_indexBuffer;
static VkBuffer                          g_vertexBuffer;
static VkDeviceMemory                    g_indexDeviceMemory;
static VkDeviceMemory                    g_vertexDeviceMemory;
static VkDeviceMemory                    g_textureImageMemory;
static VkPipelineLayout                  g_pipelineLayout;
static VkPipeline                        g_pipeline;
static VkVertexInputAttributeDescription g_attributeDescriptions[2];
static VkVertexInputBindingDescription   g_bindingDescriptions[1];
static VkShaderModule                    g_vertexShaderModule;
static VkShaderModule                    g_pixelShaderModule;
static VkImage                           g_textureImage;
static VkDescriptorImageInfo             g_imageInfo;
static VkDescriptorSetLayout             g_descriptorSetLayout;
static VkDescriptorSet*                  g_descriptorSets;
static VkWriteDescriptorSet              g_descriptor;

//-----------------------------------------------------------------------------
// [SECTION] example specific data
//-----------------------------------------------------------------------------
struct ConstantBuffer
{
    float x_offset = 0.0f;
    float y_offset = 0.0f;
    float padding[2] = { 0.0f, 0.0f };
};

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

static unsigned char g_image[] = {
    255,   0,   0, 255,
      0, 255,   0, 255,
      0,   0, 255, 255,
    255,   0, 255, 255
};

static ConstantBuffer g_vertexOffset = { 0.0f, 0.0f };

//-----------------------------------------------------------------------------
// [SECTION] general setup function declarations
//-----------------------------------------------------------------------------
static void create_window();
static void create_vulkan_instance();
static void enable_validation_layers();
static void create_surface();
static void select_physical_device();
static void create_logical_device();
static void create_swapchain();
static void create_command_pool();
static void create_main_command_buffers();
static void create_descriptor_pool();
static void create_render_pass();
static void create_depth_resources();
static void create_frame_buffers();
static void create_syncronization_primitives();
static void process_events();
static void cleanup();

//-----------------------------------------------------------------------------
// [SECTION] example specific setup function declarations
//-----------------------------------------------------------------------------
static void create_vertex_layout();
static void create_descriptor_set_layout();
static void create_descriptor_set();
static void create_pipeline_layout();
static void create_pipeline();
static void create_vertex_buffer();
static void create_index_buffer();
static void create_texture();

//-----------------------------------------------------------------------------
// [SECTION] general per-frame function declarations
//-----------------------------------------------------------------------------
static void begin_frame(); // wait for fences and acquire next image
static void begin_recording();
static void begin_render_pass();
static void set_viewport_settings();
static void end_render_pass();
static void end_recording();
static void submit_command_buffers_then_present();

//-----------------------------------------------------------------------------
// [SECTION] example specific per-frame function declarations
//-----------------------------------------------------------------------------
static void update_descriptor_sets();
static void setup_pipeline_state();
static void draw();

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------
int main()
{

    // general setup
    create_window();
    create_vulkan_instance();
    enable_validation_layers();
    create_surface();
    select_physical_device();
    create_logical_device();
    create_swapchain();
    create_command_pool();
    create_main_command_buffers();
    create_descriptor_pool();
    create_render_pass();
    create_depth_resources();
    create_frame_buffers();
    create_syncronization_primitives();

    // example specific setup
    create_vertex_layout();
    create_descriptor_set_layout();
    create_descriptor_set();
    create_pipeline_layout();
    create_pipeline();
    create_vertex_buffer();
    create_index_buffer();
    create_texture();

    // main loop
    while (g_running)
    {
        process_events();
        begin_frame(); // wait for fences and acquire next image
        begin_recording();
        update_descriptor_sets();
        begin_render_pass();
        set_viewport_settings();
        setup_pipeline_state();
        draw();
        end_render_pass();
        end_recording();
        submit_command_buffers_then_present();
    }

    cleanup();
}

//-----------------------------------------------------------------------------
// [SECTION] misc.
//-----------------------------------------------------------------------------

struct QueueFamilyIndices
{
    int graphicsFamily = -1;
    int presentFamily = -1;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
{
    printf("validation layer: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

#ifdef _WIN32
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
#else
#endif

//-----------------------------------------------------------------------------
// [SECTION] helper functions
//-----------------------------------------------------------------------------

inline unsigned get_max(unsigned a, unsigned b) { return a > b ? a : b;}
inline unsigned get_min(unsigned a, unsigned b) { return a < b ? a : b;}

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
    char* data = (char*)malloc(sizeof(char)*size);

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

static VkCommandBuffer
begin_command_buffer()
{
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
    return commandBuffer; 
}

static void
submit_command_buffer(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkDeviceWaitIdle(g_logicalDevice);
    vkFreeCommandBuffers(g_logicalDevice, g_commandPool, 1, &commandBuffer);
}

static void
copy_buffer_to_image(VkBuffer srcBuffer, VkImage dstImage, unsigned width, unsigned height, unsigned layers=1u)
{
    VkCommandBuffer commandBuffer = begin_command_buffer();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = layers;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        srcBuffer,
        dstImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    submit_command_buffer(commandBuffer);
}

static void 
transition_image_layout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
    //VkCommandBuffer commandBuffer = mvBeginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = subresourceRange;

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (oldLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // Image layout is undefined (or does not matter)
        // Only valid as initial layout
        // No flags required, listed only for completeness
        barrier.srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        // Image is preinitialized
        // Only valid as initial layout for linear images, preserves memory contents
        // Make sure host writes have been finished
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image is a color attachment
        // Make sure any writes to the color buffer have been finished
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image is a depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image is a transfer source
        // Make sure any reads from the image have been finished
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image is a transfer destination
        // Make sure any writes to the image have been finished
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image is read by a shader
        // Make sure any shader reads from the image have been finished
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (newLayout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image will be used as a transfer destination
        // Make sure any writes to the image have been finished
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image will be used as a transfer source
        // Make sure any reads from the image have been finished
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image will be used as a color attachment
        // Make sure any writes to the color buffer have been finished
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image layout will be used as a depth/stencil attachment
        // Make sure any writes to depth/stencil buffer have been finished
        barrier.dstAccessMask = barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image will be read in a shader (sampler, input attachment)
        // Make sure any writes to the image have been finished
        if (barrier.srcAccessMask == 0)
        {
            barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    //mvEndSingleTimeCommands(commandBuffer);
}

//-----------------------------------------------------------------------------
// [SECTION] general setup functions implementation
//-----------------------------------------------------------------------------
static void
create_window()
{
#ifdef _WIN32
    WNDCLASSEXW winClass = {};
    winClass.cbSize = sizeof(WNDCLASSEXW);
    winClass.style = CS_HREDRAW | CS_VREDRAW;
    winClass.lpfnWndProc = &WndProc;
    winClass.hInstance = ::GetModuleHandle(nullptr);
    winClass.hIcon = ::LoadIconW(0, IDI_APPLICATION);
    winClass.hCursor = ::LoadCursorW(0, IDC_ARROW);
    winClass.lpszClassName = L"MyWindowClass";
    winClass.hIconSm = ::LoadIconW(0, IDI_APPLICATION);

    if (!::RegisterClassExW(&winClass)) 
    {
        ::MessageBoxA(0, "RegisterClassEx failed", "Fatal Error", MB_OK);
        assert(false);
        return;
    }

    RECT initialRect = { 0, 0, g_width, g_height };
    ::AdjustWindowRectEx(&initialRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);
    LONG initialWidth = initialRect.right - initialRect.left;
    LONG initialHeight = initialRect.bottom - initialRect.top;

    g_hwnd = ::CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
        winClass.lpszClassName,
        L"Vulkan",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        initialWidth,
        initialHeight,
        0, 0, ::GetModuleHandle(nullptr), 0);

    if (!g_hwnd) 
    {
        ::MessageBoxA(0, "CreateWindowEx failed", "Fatal Error", MB_OK);
        assert(false);
        return;
    }
#elif defined(__APPLE__)
#else
    
    g_display = XOpenDisplay(nullptr);           // connect to x
    XAutoRepeatOff(g_display);                   // turn off key repeats  
    g_connection = XGetXCBConnection(g_display); // retrieve connection from display
    if(xcb_connection_has_error(g_connection))
    {
        assert(false && "Failed to connect to X server via XCB.");
        return;
    }

    // get data from x server
    const xcb_setup_t* setup = xcb_get_setup(g_connection);

    // Loop through screens using iterator
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    int screen_p = 0;
    for (int s = screen_p; s > 0; s--) 
    {
        xcb_screen_next(&it);
    }

    // After screens have been looped through, assign it.
    g_screen = it.data;

    // Allocate a XID for the window to be created.
    g_window = xcb_generate_id(g_connection);

    // Register event types.
    // XCB_CW_BACK_PIXEL = filling then window bg with a single colour
    // XCB_CW_EVENT_MASK is required.
    unsigned int event_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

    // Listen for keyboard and mouse buttons
    unsigned int  event_values = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                    XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_POINTER_MOTION |
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    // Values to be sent over XCB (bg colour, events)
    unsigned int  value_list[] = {g_screen->black_pixel, event_values};

    // Create the window
    xcb_create_window(
        g_connection,
        XCB_COPY_FROM_PARENT,  // depth
        g_window,
        g_screen->root,        // parent
        200,                              //x
        200,                              //y
        g_width,                          //width
        g_height,                         //height
        0,                              // No border
        XCB_WINDOW_CLASS_INPUT_OUTPUT,  //class
        g_screen->root_visual,
        event_mask,
        value_list);

    // Change the title
    xcb_change_property(
        g_connection,
        XCB_PROP_MODE_REPLACE,
        g_window,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,  // data should be viewed 8 bits at a time
        strlen("Vulkan"),
        "Vulkan");

    // Tell the server to notify when the window manager
    // attempts to destroy the window.
    xcb_intern_atom_cookie_t wm_delete_cookie = xcb_intern_atom(
        g_connection,
        0,
        strlen("WM_DELETE_WINDOW"),
        "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
        g_connection,
        0,
        strlen("WM_PROTOCOLS"),
        "WM_PROTOCOLS");
    xcb_intern_atom_reply_t* wm_delete_reply = xcb_intern_atom_reply(
        g_connection,
        wm_delete_cookie,
        NULL);
    xcb_intern_atom_reply_t* wm_protocols_reply = xcb_intern_atom_reply(
        g_connection,
        wm_protocols_cookie,
        NULL);
    g_wm_delete_win = wm_delete_reply->atom;
    g_wm_protocols = wm_protocols_reply->atom;

    xcb_change_property(
        g_connection,
        XCB_PROP_MODE_REPLACE,
        g_window,
        wm_protocols_reply->atom,
        4,
        32,
        1,
        &wm_delete_reply->atom);

    // Map the window to the screen
    xcb_map_window(g_connection, g_window);

    // Flush the stream
    int stream_result = xcb_flush(g_connection);
    if (stream_result <= 0) 
    {
        assert(false && "An error occurred when flusing the stream");
        return;
    }
#endif
}

static  void
create_vulkan_instance()
{
#ifdef _WIN32  
    char* layerPath = getenv("VK_LAYER_PATH");
    int result = putenv(layerPath);
#endif

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
#ifdef _WIN32
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
#else
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
#ifdef MV_ENABLE_VALIDATION_LAYERS
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
    };
#ifdef MV_ENABLE_VALIDATION_LAYERS
    createInfo.enabledExtensionCount = 3u;
#else
    createInfo.enabledExtensionCount = 2u;
#endif
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
#else
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.pNext = VK_NULL_HANDLE;
#endif

    MV_VULKAN(vkCreateInstance(&createInfo, nullptr, &g_instance));
}

static void
enable_validation_layers()
{
#ifdef MV_ENABLE_VALIDATION_LAYERS
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pNext = VK_NULL_HANDLE;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_instance, "vkCreateDebugUtilsMessengerEXT");
    assert(func != nullptr && "failed to set up debug messenger!");
    MV_VULKAN(func(g_instance, &createInfo, nullptr, &g_debugMessenger));
#endif
}

static void
create_surface()
{
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = NULL;
    surfaceCreateInfo.flags = 0;
    surfaceCreateInfo.hinstance = ::GetModuleHandle(NULL);
    surfaceCreateInfo.hwnd = g_hwnd;
    MV_VULKAN(vkCreateWin32SurfaceKHR(g_instance, &surfaceCreateInfo, nullptr, &g_surface));
#elif defined(__APPLE__)
#else
    VkXlibSurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = NULL;
    surfaceCreateInfo.flags = 0;
    surfaceCreateInfo.dpy = g_display;
    surfaceCreateInfo.window = g_window;
    vkCreateXlibSurfaceKHR(g_instance, &surfaceCreateInfo, nullptr, &g_surface);
#endif
}

static void
select_physical_device()
{
    unsigned deviceCount = 0u;
    MV_VULKAN(vkEnumeratePhysicalDevices(g_instance, &deviceCount, nullptr));
    assert(deviceCount > 0 && "failed to find GPUs with Vulkan support!");

    //-----------------------------------------------------------------------------
    // check if device is suitable
    //-----------------------------------------------------------------------------
    auto devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice)*deviceCount);
    MV_VULKAN(vkEnumeratePhysicalDevices(g_instance, &deviceCount, devices));

    // prefer discrete, then memory size
    int bestDeviceIndex = 0;
    bool discreteGPUFound = false;
    VkDeviceSize maxLocalMemorySize = 0u;
    for(int i = 0; i < deviceCount; i++)
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

        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
        vkGetPhysicalDeviceMemoryProperties(devices[i], &memoryProperties);

        if (extensionsSupported && swapChainAdequate)
        {
            for(int j = 0; j < memoryProperties.memoryHeapCount; j++)
            {
                if(memoryProperties.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT && memoryProperties.memoryHeaps[j].size > maxLocalMemorySize)
                {
                    maxLocalMemorySize = memoryProperties.memoryHeaps[j].size;
                    bestDeviceIndex = i;
                }
            }

            if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && !discreteGPUFound)
            {
                bestDeviceIndex = i;
                discreteGPUFound = true;
            }
        }
    }

    g_physicalDevice = devices[bestDeviceIndex];
    vkGetPhysicalDeviceProperties(devices[bestDeviceIndex], &g_deviceProperties);
    vkGetPhysicalDeviceMemoryProperties(devices[bestDeviceIndex], &g_memoryProperties);
    printf("Physical Device Selection\n");
    printf("-------------------------\n");
    printf("Device ID: %u\n", g_deviceProperties.deviceID);
    printf("Vendor ID: %u\n", g_deviceProperties.vendorID);
    printf("API Version: %u\n", g_deviceProperties.apiVersion);
    printf("Driver Version: %u\n", g_deviceProperties.driverVersion);
    static const char* deviceTypeName[] = {"Other", "Integrated", "Discrete", "Virtual", "CPU"};
    printf("Device Type: %s\n", deviceTypeName[g_deviceProperties.deviceType]);
    printf("Device Name: %s\n", g_deviceProperties.deviceName);
    printf("Device Local Memory: %I64u\n", maxLocalMemorySize);
    

    assert(g_physicalDevice != VK_NULL_HANDLE && "failed to find a suitable GPU!");
}

static void
create_logical_device()
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
        g_framesInFlight = g_minImageCount-1;
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

static void
create_command_pool()
{
    QueueFamilyIndices queueFamilyIndices = find_queue_families(g_physicalDevice);

    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    MV_VULKAN(vkCreateCommandPool(g_logicalDevice, &commandPoolInfo, nullptr, &g_commandPool));
}

static void
create_main_command_buffers()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = g_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (unsigned)(g_minImageCount);

    g_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*g_minImageCount);

    MV_VULKAN(vkAllocateCommandBuffers(g_logicalDevice, &allocInfo, g_commandBuffers));
}

static void
create_descriptor_pool()
{
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
create_depth_resources()
{
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    create_image(g_swapChainExtent.width, g_swapChainExtent.height, depthFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        g_depthImage, g_depthImageMemory);

    g_depthImageView = create_image_view(g_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

static void
create_frame_buffers()
{
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
}

static void
create_syncronization_primitives()
{
    g_imagesInFlight = (VkFence*)malloc(sizeof(VkFence)*g_minImageCount);
    g_inFlightFences = (VkFence*)malloc(sizeof(VkFence)*g_minImageCount);
    g_imageAvailableSemaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore)*g_minImageCount);
    g_renderFinishedSemaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore)*g_minImageCount);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (unsigned i = 0; i < g_minImageCount; i++)
    {
        g_imagesInFlight[i] = VK_NULL_HANDLE;
        MV_VULKAN(vkCreateSemaphore(g_logicalDevice, &semaphoreInfo, nullptr, &g_imageAvailableSemaphores[i]));
        MV_VULKAN(vkCreateSemaphore(g_logicalDevice, &semaphoreInfo, nullptr, &g_renderFinishedSemaphores[i]));
        MV_VULKAN(vkCreateFence(g_logicalDevice, &fenceInfo, nullptr, &g_inFlightFences[i]));
    }
}

//-----------------------------------------------------------------------------
// [SECTION] example specific setup function implementations
//-----------------------------------------------------------------------------

static void
create_vertex_layout()
{
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
}

static void
create_descriptor_set_layout()
{
    VkDescriptorSetLayout descriptorSetLayouts[1] = {g_descriptorSetLayout};
    VkDescriptorSetLayoutBinding bindings[1];

    bindings[0].binding = 0u;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = bindings;

    MV_VULKAN(vkCreateDescriptorSetLayout(g_logicalDevice, &layoutInfo, nullptr, &g_descriptorSetLayout));
}

static void
create_descriptor_set()
{
    // allocate descriptor sets
    g_descriptorSets = (VkDescriptorSet*)malloc(g_minImageCount*sizeof(VkDescriptorSet));
    auto layouts = (VkDescriptorSetLayout*)malloc(g_minImageCount*sizeof(VkDescriptorSetLayout));
    for(int i = 0; i < g_minImageCount; i++)
        layouts[i] = g_descriptorSetLayout;
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = g_descriptorPool;
    allocInfo.descriptorSetCount = g_minImageCount;
    allocInfo.pSetLayouts = layouts;

    MV_VULKAN(vkAllocateDescriptorSets(g_logicalDevice, &allocInfo, g_descriptorSets));
}

static void
create_pipeline_layout()
{
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_descriptorSetLayout;

    MV_VULKAN(vkCreatePipelineLayout(g_logicalDevice, &pipelineLayoutInfo, nullptr, &g_pipelineLayout));
}

static void
create_pipeline()
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

static void
create_vertex_buffer()
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
    VkCommandBuffer commandBuffer = begin_command_buffer();

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

static void
create_index_buffer()
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
    VkCommandBuffer commandBuffer = begin_command_buffer();

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

static void
create_texture()
{
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferDeviceMemory;
    g_imageInfo = VkDescriptorImageInfo{};
    g_imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    { // create staging buffer

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(unsigned char)*16;
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
        memcpy(mapping, g_image, bufferInfo.size);
        vkUnmapMemory(g_logicalDevice, stagingBufferDeviceMemory);
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = 2;
    imageInfo.extent.height = 2;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;
    MV_VULKAN(vkCreateImage(g_logicalDevice, &imageInfo, nullptr, &g_textureImage));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(g_logicalDevice, g_textureImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    MV_VULKAN(vkAllocateMemory(g_logicalDevice, &allocInfo, nullptr, &g_textureImageMemory));
    MV_VULKAN(vkBindImageMemory(g_logicalDevice, g_textureImage, g_textureImageMemory, 0));

    //-----------------------------------------------------------------------------
    // final image
    //-----------------------------------------------------------------------------

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = imageInfo.mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    VkCommandBuffer commandBuffer = begin_command_buffer();
    transition_image_layout(commandBuffer, g_textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
    submit_command_buffer(commandBuffer);
    copy_buffer_to_image(stagingBuffer, g_textureImage, (unsigned)2, (unsigned)2);
    commandBuffer = begin_command_buffer();
    transition_image_layout(commandBuffer, g_textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
    submit_command_buffer(commandBuffer);
    vkDestroyBuffer(g_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(g_logicalDevice, stagingBufferDeviceMemory, nullptr);

    //mvGenerateMipmaps(graphics, texture.textureImage, VK_FORMAT_R8G8B8A8_UNORM, 2, 2, imageInfo.mipLevels);

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(g_physicalDevice, &properties);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = g_textureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    MV_VULKAN(vkCreateImageView(g_logicalDevice, &viewInfo, nullptr, &g_imageInfo.imageView));

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;

    MV_VULKAN(vkCreateSampler(g_logicalDevice, &samplerInfo, nullptr, &g_imageInfo.sampler));
}

static void
process_events()
{

#ifdef _WIN32
    MSG msg = {};
    while (::PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            g_running = false;
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
#elif defined(__APPLE__)
#else // linux

    xcb_generic_event_t* event;
    xcb_client_message_event_t* cm;

    // Poll for events until null is returned.
    while (event = xcb_poll_for_event(g_connection)) 
    {
        switch (event->response_type & ~0x80) 
        {

            case XCB_KEY_PRESS:
            case XCB_KEY_RELEASE: 
            {
                xcb_key_press_event_t* kb_event = (xcb_key_press_event_t*)event;
                bool pressed = event->response_type == XCB_KEY_PRESS;
                xcb_keycode_t code = kb_event->detail;
                KeySym key_sym = XkbKeycodeToKeysym(g_display,(KeyCode)code, 0, code & ShiftMask ? 1 : 0);
                if(key_sym == XK_W) g_vertexOffset.y_offset += 0.01f;
                if(key_sym == XK_S) g_vertexOffset.y_offset -= 0.01f;
                if(key_sym == XK_A) g_vertexOffset.x_offset -= 0.01f;
                if(key_sym == XK_D) g_vertexOffset.x_offset += 0.01f;
                break;
            }

            case XCB_CLIENT_MESSAGE: 
            {
                cm = (xcb_client_message_event_t*)event;

                // Window close
                if (cm->data.data32[0] == g_wm_delete_win) 
                {
                    isRunning = false;
                }
                break;
            } 
            default: break;
        }
        free(event);
    }
#endif
}

static void
cleanup()
{
#ifdef _WIN32
#elif defined(__APPLE__)
#else // linux
    // Turn key repeats back on since this is global for the OS... just... wow.
    XAutoRepeatOn(g_display);
    xcb_destroy_window(g_connection, g_window);
#endif   
}

//-----------------------------------------------------------------------------
// [SECTION] general per-frame function implementations
//-----------------------------------------------------------------------------

static void
begin_frame()
{
    MV_VULKAN(vkWaitForFences(g_logicalDevice, 1, &g_inFlightFences[g_currentFrame], VK_TRUE, UINT64_MAX));
    MV_VULKAN(vkAcquireNextImageKHR(g_logicalDevice, g_swapChain, UINT64_MAX, g_imageAvailableSemaphores[g_currentFrame],VK_NULL_HANDLE, &g_currentImageIndex));
    if (g_imagesInFlight[g_currentImageIndex] != VK_NULL_HANDLE)
        MV_VULKAN(vkWaitForFences(g_logicalDevice, 1, &g_imagesInFlight[g_currentImageIndex], VK_TRUE, UINT64_MAX));

    // just in case the acquired image is out of order
    g_imagesInFlight[g_currentImageIndex] = g_inFlightFences[g_currentFrame];
}

static void
begin_recording()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    MV_VULKAN(vkBeginCommandBuffer(g_commandBuffers[g_currentImageIndex], &beginInfo));
}

static void
begin_render_pass()
{
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

    vkCmdBeginRenderPass(g_commandBuffers[g_currentImageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

static void
set_viewport_settings()
{
    VkRect2D scissor{};
    scissor.extent = g_swapChainExtent;

    g_viewport.x = 0.0f;
    g_viewport.y = g_swapChainExtent.height;
    g_viewport.width = g_swapChainExtent.width;
    g_viewport.height = -(int)g_swapChainExtent.height;

    vkCmdSetViewport(g_commandBuffers[g_currentImageIndex], 0, 1, &g_viewport);
    vkCmdSetScissor(g_commandBuffers[g_currentImageIndex], 0, 1, &scissor);  
}

static void
end_render_pass()
{
    vkCmdEndRenderPass(g_commandBuffers[g_currentImageIndex]);
}

static void
end_recording()
{
    MV_VULKAN(vkEndCommandBuffer(g_commandBuffers[g_currentImageIndex]));
}

static void
submit_command_buffers_then_present()
{
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

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapChains[] = { g_swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &g_currentImageIndex;
    VkResult result = vkQueuePresentKHR(g_presentQueue, &presentInfo);
    g_currentFrame = (g_currentFrame + 1) % g_framesInFlight;
}

//-----------------------------------------------------------------------------
// [SECTION] example specific per-frame function implementations
//-----------------------------------------------------------------------------

static void
update_descriptor_sets()
{
    VkWriteDescriptorSet descriptorWrites[1];
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstBinding = 0u;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].dstSet = g_descriptorSets[g_currentImageIndex];
    descriptorWrites[0].pImageInfo = &g_imageInfo; 
    descriptorWrites[0].pNext = nullptr;
    vkUpdateDescriptorSets(g_logicalDevice, 1, descriptorWrites, 0, nullptr);
}

static void
setup_pipeline_state()
{
    static VkDeviceSize offsets = { 0 };
    vkCmdSetDepthBias(g_commandBuffers[g_currentImageIndex], 0.0f, 0.0f, 0.0f);
    vkCmdBindPipeline(g_commandBuffers[g_currentImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline);
    vkCmdBindDescriptorSets(g_commandBuffers[g_currentImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipelineLayout, 0, 1, &g_descriptorSets[g_currentImageIndex], 0u, nullptr);
    vkCmdBindIndexBuffer(g_commandBuffers[g_currentImageIndex], g_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindVertexBuffers(g_commandBuffers[g_currentImageIndex], 0, 1, &g_vertexBuffer, &offsets);
}

static void
draw()
{
    vkCmdDrawIndexed(g_commandBuffers[g_currentImageIndex], 6, 1, 0, 0, 0);
}