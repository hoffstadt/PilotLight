#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <assert.h>
#include "vulkan/vulkan.h"

#define S_BACKBUFFER_COUNT 2
#define S_FRAME_COUNT 3


//-----------------------------------------------------------------------------
// Constant Buffer Struct
//-----------------------------------------------------------------------------
struct ConstantBuffer
{
    float x_offset = 0.0f;
    float y_offset = 0.0f;
    float padding[2] = { 0.0f, 0.0f };
};

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------
static HWND           g_hwnd;
static const int      g_width = 1024;
static const int      g_height = 768;
static ConstantBuffer g_vertexOffset = { 0.0f, 0.0f };

static VkInstance                     instance;
static VkDescriptorPool               descriptorPool;
static VkPhysicalDevice               physicalDevice;
static VkDevice                       logicalDevice;
static VkQueue                        graphicsQueue;
static VkQueue                        presentQueue;
static VkCommandPool                  commandPool;
static unsigned                       graphicsQueueFamily;
// static std::vector<VkCommandBuffer>   commandBuffers;
static VkPhysicalDeviceProperties     deviceProperties;
static VkFormat                       swapChainImageFormat;
static VkSwapchainKHR                 swapChain;
// static std::vector<VkImage>           swapChainImages;
// static std::vector<VkImageView>       swapChainImageViews;
// static std::vector<VkFramebuffer>     swapChainFramebuffers;
static VkImage                        depthImage;
static VkDeviceMemory                 depthImageMemory;
static VkImageView                    depthImageView;
// static std::vector<VkSemaphore>       imageAvailableSemaphores; // syncronize rendering to image when already rendering to image
// static std::vector<VkSemaphore>       renderFinishedSemaphores; // syncronize render/present
// static std::vector<VkFence>           inFlightFences;
// static std::vector<VkFence>           imagesInFlight;
static VkRenderPass                   renderPass;
static VkExtent2D                     swapChainExtent;
static unsigned                       minImageCount;
static unsigned                       currentImageIndex;
static size_t                         currentFrame;
static VkSurfaceKHR                   surface;
static bool                           enableValidationLayers;
static VkDebugUtilsMessengerEXT       debugMessenger;

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
    2, 1, 0,
    3, 2, 0
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

int main()
{

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