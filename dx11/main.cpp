#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <assert.h>

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
static HWND                      g_hwnd;
static ID3D11Device*             g_device;
static ID3D11DeviceContext*      g_context;
static IDXGISwapChain*           g_swapChain;
static ID3D11Texture2D*          g_frameBuffer;
static ID3D11RenderTargetView*   g_frameBufferView;
static ID3DBlob*                 g_vsBlob;
static ID3D11VertexShader*       g_vertexShader;
static ID3D11PixelShader*        g_pixelShader;
static ID3D11InputLayout*        g_inputLayout;
static ID3D11Buffer*             g_vertexBuffer;
static ID3D11Buffer*             g_indexBuffer;
static ID3D11ShaderResourceView* g_textureView;
static ID3D11SamplerState*       g_sampler;
static ID3D11Buffer*             g_constantBuffer;
static UINT                      g_numVerts;
static UINT                      g_stride;
static UINT                      g_offset;
static const int                 g_width = 1024;
static const int                 g_height = 768;
static ConstantBuffer            g_vertexOffset = { 0.0f, 0.0f };

//-----------------------------------------------------------------------------
// Vertex Data and Index Data
//-----------------------------------------------------------------------------
static const float g_vertexData[] = { // x, y, u, v
     -0.5f,  0.5f, 0.0f, 0.0f,
     -0.5f, -0.5f, 0.0f, 1.0f,
      0.5f, -0.5f, 1.0f, 1.0f,
      0.5f,  0.5f, 1.0f, 0.0f,
};

static const unsigned short g_indices[] = { 
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
    // Create D3D11 Device and Context and Swap Chain
    //-----------------------------------------------------------------------------
    {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferDesc.Width = g_width;
        sd.BufferDesc.Height = g_height;
        sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 0;
        sd.BufferDesc.RefreshRate.Denominator = 0;
        sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.OutputWindow = g_hwnd;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; //DXGI_SWAP_EFFECT_DISCARD;
        sd.Flags = 0;

        // create device and front/back buffers, and swap chain and rendering context
        HRESULT hResult = ::D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_DEBUG,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &sd,
            &g_swapChain,
            &g_device,
            nullptr,
            &g_context
        );
        assert(SUCCEEDED(hResult));
    }

    //-----------------------------------------------------------------------------
    // Create Framebuffer Render Target
    //-----------------------------------------------------------------------------
    {
        HRESULT hResult = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&g_frameBuffer);
        assert(SUCCEEDED(hResult));

        hResult = g_device->CreateRenderTargetView(g_frameBuffer, 0, &g_frameBufferView);
        assert(SUCCEEDED(hResult));
    }

    //-----------------------------------------------------------------------------
    // Create Vertex Shader
    //-----------------------------------------------------------------------------
    {
        ID3DBlob* shaderCompileErrorsBlob;
        HRESULT hResult = ::D3DCompileFromFile(L"../shaders.hlsl", 
            nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &g_vsBlob, &shaderCompileErrorsBlob);
        if (FAILED(hResult))
        {
            const char* errorString = nullptr;
            if (hResult == ::HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                errorString = "Could not compile shader; file not found";
            else if (shaderCompileErrorsBlob)
                errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();
            MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
            return 1;
        }

        hResult = g_device->CreateVertexShader(g_vsBlob->GetBufferPointer(), 
            g_vsBlob->GetBufferSize(), nullptr, &g_vertexShader);
        assert(SUCCEEDED(hResult));
        if(shaderCompileErrorsBlob)
            shaderCompileErrorsBlob->Release();
    }

    //-----------------------------------------------------------------------------
    // Create Pixel Shader
    //-----------------------------------------------------------------------------
    {
        ID3DBlob* psBlob;
        ID3DBlob* shaderCompileErrorsBlob;
        HRESULT hResult = ::D3DCompileFromFile(L"../shaders.hlsl", 
            nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &psBlob, &shaderCompileErrorsBlob);
        if (FAILED(hResult))
        {
            const char* errorString = nullptr;
            if (hResult == ::HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                errorString = "Could not compile shader; file not found";
            else if (shaderCompileErrorsBlob)
                errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();

            ::MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
            return 1;
        }

        hResult = g_device->CreatePixelShader(psBlob->GetBufferPointer(), 
            psBlob->GetBufferSize(), nullptr, &g_pixelShader);
        assert(SUCCEEDED(hResult));

        psBlob->Release();
        if(shaderCompileErrorsBlob)
            shaderCompileErrorsBlob->Release();
    }

    //-----------------------------------------------------------------------------
    // Create Input Layout
    //-----------------------------------------------------------------------------
    {
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        HRESULT hResult = g_device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc), 
            g_vsBlob->GetBufferPointer(), g_vsBlob->GetBufferSize(), &g_inputLayout);
        assert(SUCCEEDED(hResult));
    }

    //-----------------------------------------------------------------------------
    // Create Vertex Buffer
    //-----------------------------------------------------------------------------
    {
        g_stride = 4 * sizeof(float);
        g_numVerts = sizeof(g_vertexData) / g_stride;
        g_offset = 0;

        D3D11_BUFFER_DESC vertexBufferDesc = {};
        vertexBufferDesc.ByteWidth = sizeof(g_vertexData);
        vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexSubresourceData = { g_vertexData };

        HRESULT hResult = g_device->CreateBuffer(&vertexBufferDesc, 
            &vertexSubresourceData, &g_vertexBuffer);
        assert(SUCCEEDED(hResult));
    }

    //-----------------------------------------------------------------------------
    // Create Index Buffer
    //-----------------------------------------------------------------------------
    {
        // Fill in a buffer description.
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = UINT(sizeof(unsigned short) * 6);
        bufferDesc.StructureByteStride = sizeof(unsigned short);
        bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bufferDesc.CPUAccessFlags = 0u;
        bufferDesc.MiscFlags = 0u;

        // Define the resource data.
        D3D11_SUBRESOURCE_DATA InitData = {};
        InitData.pSysMem = g_indices;

        // Create the buffer with the device.
        g_device->CreateBuffer(&bufferDesc, &InitData, &g_indexBuffer);
    }

    //-----------------------------------------------------------------------------
    // Create Sampler
    //-----------------------------------------------------------------------------
    {
        D3D11_SAMPLER_DESC samplerDesc = CD3D11_SAMPLER_DESC{ CD3D11_DEFAULT{} };
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;


        g_device->CreateSamplerState(&samplerDesc, &g_sampler);
    }

    //-----------------------------------------------------------------------------
    // Create Texture
    //-----------------------------------------------------------------------------
    {
        
        ID3D11Texture2D* texture;

        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = 2;
        textureDesc.Height = 2;
        textureDesc.MipLevels = 0;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

        g_device->CreateTexture2D(&textureDesc, nullptr, &texture);

        g_context->UpdateSubresource(texture, 0u, nullptr, image, 8, 0u);

        // create the resource view on the texture
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        g_device->CreateShaderResourceView(texture, &srvDesc, &g_textureView);
        g_context->GenerateMips(g_textureView);
        texture->Release();
    }

    //-----------------------------------------------------------------------------
    // Create Constant Buffer
    //-----------------------------------------------------------------------------
    {
        D3D11_BUFFER_DESC cbd;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbd.MiscFlags = 0u;
        cbd.ByteWidth = sizeof(ConstantBuffer);
        cbd.StructureByteStride = 0u;

        D3D11_SUBRESOURCE_DATA csd = {};
        csd.pSysMem = &g_vertexOffset;
        g_device->CreateBuffer(&cbd, &csd, &g_constantBuffer);

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

        // update constant buffer (every frame)
        D3D11_MAPPED_SUBRESOURCE mappedSubresource;
        g_context->Map(g_constantBuffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mappedSubresource);
        memcpy(mappedSubresource.pData, &g_vertexOffset, sizeof(ConstantBuffer));
        g_context->Unmap(g_constantBuffer, 0u);

        // clear render target
        float backgroundColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        g_context->ClearRenderTargetView(g_frameBufferView, backgroundColor);

        // set viewport
        RECT winRect;
        ::GetClientRect(g_hwnd, &winRect);
        D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)(winRect.right - winRect.left), (FLOAT)(winRect.bottom - winRect.top), 0.0f, 1.0f };
        g_context->RSSetViewports(1, &viewport);

        // bind frame buffer
        g_context->OMSetRenderTargets(1, &g_frameBufferView, nullptr);

        // bind constant buffer
        g_context->VSSetConstantBuffers(0u, 1u, &g_constantBuffer);

        // bind primitive topology
        g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // bind vertex layout
        g_context->IASetInputLayout(g_inputLayout);

        // bind shaders
        g_context->VSSetShader(g_vertexShader, nullptr, 0);
        g_context->PSSetShader(g_pixelShader, nullptr, 0);

        // bind index and vertex buffers
        g_context->IASetIndexBuffer(g_indexBuffer, DXGI_FORMAT_R16_UINT, 0u);
        g_context->IASetVertexBuffers(0, 1, &g_vertexBuffer, &g_stride, &g_offset);

        // bind sampler and texture
        g_context->PSSetSamplers(0u, 1, &g_sampler);
        g_context->PSSetShaderResources(0u, 1, &g_textureView);

        // draw
        g_context->DrawIndexed(6, 0u, 0u);

        // swap front and back buffers
        g_swapChain->Present(1, 0);
    }

    //-----------------------------------------------------------------------------
    // cleanup
    //-----------------------------------------------------------------------------

    g_swapChain->Release();
    g_frameBuffer->Release();
    g_frameBufferView->Release();
    g_vsBlob->Release();
    g_vertexShader->Release();
    g_pixelShader->Release();
    g_inputLayout->Release();
    g_vertexBuffer->Release();
    g_indexBuffer->Release();
    g_textureView->Release();
    g_sampler->Release();
    g_constantBuffer->Release();
    g_context->Release();
    g_device->Release();

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