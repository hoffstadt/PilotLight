#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <assert.h>

#define S_BACKBUFFER_COUNT 2
#define S_FRAME_COUNT 3
#define S_D3D12(x) assert(SUCCEEDED(x));

struct FrameContext
{
    ID3D12CommandAllocator* commandAllocator;
    size_t                  fenceValue;
};

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
static FrameContext                 g_frameContext[S_FRAME_COUNT];
static unsigned                     g_frameIndex;
static HWND                         g_hwnd;
static IDXGIFactory4*               g_dxgiFactory;
static ID3D12Device*                g_device;
static ID3D12DescriptorHeap*        g_rtvDescHeap;
static ID3D12DescriptorHeap*        g_dsvDescHeap;
static ID3D12DescriptorHeap*        g_srvDescHeap;
static ID3D12CommandQueue*          g_commandQueue;
static ID3D12GraphicsCommandList*   g_commandList;
static ID3D12Fence*                 g_fence;
static HANDLE                       g_fenceEvent;
static size_t                       g_fenceLastSignaledValue;
static D3D12_CPU_DESCRIPTOR_HANDLE  g_depthStencilView;
static ID3D12Resource*              g_mainRenderTargetResource[S_BACKBUFFER_COUNT];
static D3D12_CPU_DESCRIPTOR_HANDLE  g_mainRenderTargetDescriptor[S_BACKBUFFER_COUNT];
static IDXGISwapChain3*             g_swapChain;
static HANDLE                       g_swapChainWaitableObject;
static ID3D12Resource*              g_depthStencilBuffer;
static DXGI_FORMAT                  g_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
static size_t                       g_uploadHeapSize = 1024u*1024u*4u;
static ID3D12Resource*              g_uploadHeap;
static ID3D12Heap*                  g_uniformHeap;
static ID3DBlob*                    g_vsBlob;
static ID3DBlob*                    g_psBlob;
static ID3D12RootSignature*         g_rootSignature;
static ID3D12PipelineState*         g_pipelineState;
static ID3D12Resource*              g_vertexBuffer;
static ID3D12Resource*              g_indexBuffer;
static ID3D12Resource*              g_uniformBuffer;
static ID3D12Resource*              g_textureBuffer;
static D3D12_INDEX_BUFFER_VIEW      g_indexBufferView;
static D3D12_VERTEX_BUFFER_VIEW     g_vertexBufferView;
static D3D12_VIEWPORT               g_viewport;
static D3D12_RECT                   g_surfaceSize;
static unsigned char*               g_mapping;
static unsigned                     g_rtvDescriptorSize; // caching descriptor size 
static unsigned                     g_dsvDescriptorSize; // caching descriptor size
static unsigned                     g_cbvDescriptorSize; // caching descriptor size
static D3D12_CPU_DESCRIPTOR_HANDLE  g_textureHandle;
static const int                    g_width = 1024;
static const int                    g_height = 768;
static ConstantBuffer               g_vertexOffset = { 0.0f, 0.0f };
static unsigned                     g_rtvHeapSize = 10u; 
static unsigned                     g_dsvHeapSize = 10u;
static unsigned                     g_cbvHeapSize = 10u;
static unsigned                     g_srvHeapSize = 10u;

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
    // Create DXGI Factory
    //-----------------------------------------------------------------------------
    assert(::CreateDXGIFactory(IID_PPV_ARGS(&g_dxgiFactory)) == S_OK);

    //-----------------------------------------------------------------------------
    // Create Debug interface
    //-----------------------------------------------------------------------------
    ID3D12Debug* pdx12Debug = nullptr;
    if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
        pdx12Debug->EnableDebugLayer();

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    assert(::D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&g_device)) == S_OK);

    if (pdx12Debug != nullptr)
    {
        ID3D12InfoQueue* pInfoQueue = nullptr;
        g_device->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        pInfoQueue->Release();
        pdx12Debug->Release();
    }

    //-----------------------------------------------------------------------------
    // Create Descriptor Heaps
    //-----------------------------------------------------------------------------
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = g_rtvHeapSize;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        assert(g_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_rtvDescHeap)) == S_OK);

        unsigned rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (unsigned i = 0; i < S_BACKBUFFER_COUNT; i++)
        {
            g_mainRenderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = g_dsvHeapSize;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        assert(g_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_dsvDescHeap)) == S_OK);
        g_depthStencilView = g_dsvDescHeap->GetCPUDescriptorHandleForHeapStart();
    }

    {

        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = g_srvHeapSize;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        desc.NodeMask = 1;
        assert(g_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_srvDescHeap)) == S_OK);
    }

    //-----------------------------------------------------------------------------
    // Create Command Queue
    //-----------------------------------------------------------------------------
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        assert(g_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_commandQueue)) == S_OK);
    }

    //-----------------------------------------------------------------------------
    // Create Command Allocators
    //-----------------------------------------------------------------------------
    for (unsigned i = 0; i < S_FRAME_COUNT; i++)
    {
        assert(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].commandAllocator)) == S_OK);
    }

    //-----------------------------------------------------------------------------
    // Create Command Lists
    //-----------------------------------------------------------------------------
    assert(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].commandAllocator, nullptr, IID_PPV_ARGS(&g_commandList)) == S_OK);

    //-----------------------------------------------------------------------------
    // Create Fence & Fence Event
    //-----------------------------------------------------------------------------
    assert(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) == S_OK);
    g_fenceEvent = ::CreateEvent(nullptr, false, false, nullptr);
    assert(g_fenceEvent != nullptr);

    // cache descriptor sizes (hardware dependent)
    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    g_dsvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    g_cbvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


    //-----------------------------------------------------------------------------
    // Create Swapchain
    //-----------------------------------------------------------------------------
    {
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.BufferCount = S_BACKBUFFER_COUNT;
        sd.Width = 0;
        sd.Height = 0;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = S_BACKBUFFER_COUNT;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.Stereo = FALSE;
        
        IDXGISwapChain1* swapChain1 = nullptr;

        assert(g_dxgiFactory->CreateSwapChainForHwnd(g_commandQueue, g_hwnd, &sd, nullptr, nullptr, &swapChain1) == S_OK);
        assert(swapChain1->QueryInterface(IID_PPV_ARGS(&g_swapChain)) == S_OK);
        swapChain1->Release();
        g_swapChain->SetMaximumFrameLatency(S_BACKBUFFER_COUNT);
        g_swapChainWaitableObject = g_swapChain->GetFrameLatencyWaitableObject();
    }

    //-----------------------------------------------------------------------------
    // Create Render targets
    //-----------------------------------------------------------------------------
    for (unsigned i = 0; i < S_BACKBUFFER_COUNT; i++)
    {
        ID3D12Resource* pBackBuffer = nullptr;
        g_swapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        g_device->CreateRenderTargetView(pBackBuffer, nullptr, g_mainRenderTargetDescriptor[i]);
        g_mainRenderTargetResource[i] = pBackBuffer;
    }

    //-----------------------------------------------------------------------------
    // create depth/stencil buffer & view
    //-----------------------------------------------------------------------------
    {
        D3D12_RESOURCE_DESC depthStencilDesc{};
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment = 0;
        depthStencilDesc.Width = g_width;
        depthStencilDesc.Height = g_height;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;
        depthStencilDesc.Format = g_depthStencilFormat;
        depthStencilDesc.SampleDesc.Count = 1;
        depthStencilDesc.SampleDesc.Quality = 0;
        depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE optClear;
        optClear.Format = g_depthStencilFormat;
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0x00;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        assert(g_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthStencilDesc, D3D12_RESOURCE_STATE_COMMON, &optClear, IID_PPV_ARGS(&g_depthStencilBuffer)) == S_OK);

        // create descriptor to mip level 0
        g_device->CreateDepthStencilView(g_depthStencilBuffer, nullptr, g_depthStencilView);

        // transition resource from initial state to depth buffer
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = g_depthStencilBuffer;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        g_commandList->ResourceBarrier(1, &barrier);
    }

    g_commandList->Close();
    g_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_commandList);

    //-----------------------------------------------------------------------------
    // create upload & uniform heaps
    //-----------------------------------------------------------------------------
    {
        D3D12_HEAP_PROPERTIES uploadHeapProps{};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC stagingBufferResourceDesc{};
        stagingBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        stagingBufferResourceDesc.Width = g_uploadHeapSize;
        stagingBufferResourceDesc.Height = 1;
        stagingBufferResourceDesc.DepthOrArraySize = 1;
        stagingBufferResourceDesc.MipLevels = 1;
        stagingBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        stagingBufferResourceDesc.SampleDesc.Count = 1;
        stagingBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        stagingBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        S_D3D12(g_device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &stagingBufferResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_uploadHeap)));
    }

    {
        D3D12_HEAP_DESC desc;
        desc.SizeInBytes = 65536u*3;
        desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
        desc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
        desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        desc.Properties.CreationNodeMask = 1;
        desc.Properties.VisibleNodeMask = 1;

        S_D3D12(g_device->CreateHeap(&desc, IID_PPV_ARGS(&g_uniformHeap)));
    }

    //-----------------------------------------------------------------------------
    // Create Vertex Shader
    //-----------------------------------------------------------------------------
    {
        ID3DBlob* shaderCompileErrorsBlob;
        HRESULT hResult = ::D3DCompileFromFile(L"../shaders.hlsl", nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &g_vsBlob, &shaderCompileErrorsBlob);
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
    }

    //-----------------------------------------------------------------------------
    // Create Pixel Shader
    //-----------------------------------------------------------------------------
    {
        ID3DBlob* shaderCompileErrorsBlob;
        HRESULT hResult = ::D3DCompileFromFile(L"../shaders.hlsl", 
            nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &g_psBlob, &shaderCompileErrorsBlob);
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
    }

    //-----------------------------------------------------------------------------
    // Create Root signature
    //-----------------------------------------------------------------------------
    {
        // This is the highest version the sample supports. If
        // CheckFeatureSupport succeeds, the HighestVersion returned will not be
        // greater than this.
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(g_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        ID3DBlob* signature = nullptr;
        ID3DBlob* error = nullptr;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_DESCRIPTOR_RANGE1 ranges[2]{};
        ranges[0].BaseShaderRegister = 0;
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        ranges[0].NumDescriptors = 1;
        ranges[0].RegisterSpace = 0;
        ranges[0].OffsetInDescriptorsFromTableStart = 0;
        ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

        ranges[1].BaseShaderRegister = 0;
        ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[1].NumDescriptors = 1;
        ranges[1].RegisterSpace = 0;
        ranges[1].OffsetInDescriptorsFromTableStart =  D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

        D3D12_ROOT_PARAMETER1 rootParameters[2]{};
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[0].DescriptorTable.pDescriptorRanges = ranges;

        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[1].DescriptorTable.pDescriptorRanges = &ranges[1];

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        rootSignatureDesc.Desc_1_1.NumParameters = 2;
        rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
        rootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
        rootSignatureDesc.Desc_1_1.pStaticSamplers = &sampler;

        S_D3D12(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
        S_D3D12(g_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&g_rootSignature)));
    }

    //-----------------------------------------------------------------------------
    // Create Pipeline State Object
    //-----------------------------------------------------------------------------
    {
        D3D12_SHADER_BYTECODE vsBytecode;
        vsBytecode.pShaderBytecode = g_vsBlob->GetBufferPointer();
        vsBytecode.BytecodeLength = g_vsBlob->GetBufferSize();
        
        D3D12_SHADER_BYTECODE psBytecode;
        psBytecode.pShaderBytecode = g_psBlob->GetBufferPointer();
        psBytecode.BytecodeLength = g_psBlob->GetBufferSize();
        
        D3D12_RASTERIZER_DESC rasterDesc;
        rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
        rasterDesc.FrontCounterClockwise = FALSE;
        // rasterDesc.DepthBias = spec.depthBias;
        // rasterDesc.DepthBiasClamp = spec.clamp;
        // rasterDesc.SlopeScaledDepthBias = spec.slopeBias;
        rasterDesc.DepthClipEnable = TRUE;
        rasterDesc.MultisampleEnable = FALSE;
        rasterDesc.AntialiasedLineEnable = FALSE;
        rasterDesc.ForcedSampleCount = 0;
        rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_BLEND_DESC blendDesc;
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
            TRUE,
            FALSE,
            D3D12_BLEND_SRC_ALPHA,
            D3D12_BLEND_INV_SRC_ALPHA,
            D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE,
            D3D12_BLEND_INV_SRC_ALPHA,
            D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };
        for (unsigned i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        D3D12_INPUT_LAYOUT_DESC layoutDesc{};
        layoutDesc.NumElements = 2;
        D3D12_INPUT_ELEMENT_DESC inputelements[]=
        {
            {"Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        layoutDesc.pInputElementDescs = inputelements;
        psoDesc.InputLayout = layoutDesc;
        psoDesc.pRootSignature = g_rootSignature;
        psoDesc.PS = psBytecode;
        psoDesc.VS = vsBytecode;
        psoDesc.RasterizerState = rasterDesc;
        psoDesc.BlendState = blendDesc;
        psoDesc.DepthStencilState.DepthEnable = true;
        psoDesc.DepthStencilState.StencilEnable = false;

        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.DSVFormat = g_depthStencilFormat;

        S_D3D12(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelineState)));
    }

    //-----------------------------------------------------------------------------
    // Create Vertex Buffer
    //-----------------------------------------------------------------------------
    {

        assert(sizeof(g_vertexData) < g_uploadHeapSize && "Upload heap size is too small");

        //-----------------------------------------------------------------------------
        // Create final buffer
        //-----------------------------------------------------------------------------

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC vertexBufferResourceDesc{};
        vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        vertexBufferResourceDesc.Width = sizeof(g_vertexData);
        vertexBufferResourceDesc.Height = 1;
        vertexBufferResourceDesc.DepthOrArraySize = 1;
        vertexBufferResourceDesc.MipLevels = 1;
        vertexBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        vertexBufferResourceDesc.SampleDesc.Count = 1;
        vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        vertexBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        g_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertexBufferResourceDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&g_vertexBuffer));

        //-----------------------------------------------------------------------------
        // Upload data to staging buffer
        //-----------------------------------------------------------------------------

        // Copy the triangle data to the vertex buffer.
        unsigned char* pDataBegin = nullptr;

        // We do not intend to read from this resource on the CPU.
        D3D12_RANGE readRange{};
        readRange.Begin = 0;
        readRange.End = 0;

        S_D3D12(g_uploadHeap->Map(0, &readRange, (void**)&pDataBegin));
        memcpy(pDataBegin, g_vertexData, sizeof(g_vertexData));
        g_uploadHeap->Unmap(0, nullptr);

        //-----------------------------------------------------------------------------
        // Transfer data from staging buffer to final buffer
        //-----------------------------------------------------------------------------

        g_commandList->Reset(g_frameContext[g_frameIndex % S_FRAME_COUNT].commandAllocator, nullptr);
        g_commandList->CopyBufferRegion(g_vertexBuffer, 0u, g_uploadHeap, 0u, sizeof(g_vertexData));
        g_commandList->Close();
        g_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_commandList);

        size_t fenceValue = g_fence->GetCompletedValue();
        fenceValue++;
        g_commandQueue->Signal(g_fence, fenceValue);

        // wait to destroy staging buffer
        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        ::WaitForSingleObject(g_fenceEvent, INFINITE);

        g_vertexBufferView.BufferLocation = g_vertexBuffer->GetGPUVirtualAddress();
        g_vertexBufferView.SizeInBytes = sizeof(g_vertexData);
        g_vertexBufferView.StrideInBytes = sizeof(float)*4;
    }

    //-----------------------------------------------------------------------------
    // Create Index Buffer
    //-----------------------------------------------------------------------------
    {
        assert(sizeof(g_vertexData) < g_uploadHeapSize && "Upload heap size is too small");

        //-----------------------------------------------------------------------------
        // Create final buffer
        //-----------------------------------------------------------------------------

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC vertexBufferResourceDesc{};
        vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        vertexBufferResourceDesc.Width = UINT(sizeof(unsigned) * 6);
        vertexBufferResourceDesc.Height = 1;
        vertexBufferResourceDesc.DepthOrArraySize = 1;
        vertexBufferResourceDesc.MipLevels = 1;
        vertexBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        vertexBufferResourceDesc.SampleDesc.Count = 1;
        vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        vertexBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        g_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertexBufferResourceDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&g_indexBuffer));

        //-----------------------------------------------------------------------------
        // Upload data to staging buffer
        //-----------------------------------------------------------------------------

        // Copy the triangle data to the vertex buffer.
        unsigned char* pDataBegin = nullptr;

        // We do not intend to read from this resource on the CPU.
        D3D12_RANGE readRange{};
        readRange.Begin = 0;
        readRange.End = 0;

        S_D3D12(g_uploadHeap->Map(0, &readRange, (void**)&pDataBegin));
        memcpy(pDataBegin, g_indices, vertexBufferResourceDesc.Width);
        g_uploadHeap->Unmap(0, nullptr);

        //-----------------------------------------------------------------------------
        // Transfer data from staging buffer to final buffer
        //-----------------------------------------------------------------------------

        g_commandList->Reset(g_frameContext[g_frameIndex % S_FRAME_COUNT].commandAllocator, nullptr);
        g_commandList->CopyBufferRegion(g_indexBuffer, 0u, g_uploadHeap, 0u, vertexBufferResourceDesc.Width);
        g_commandList->Close();
        g_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_commandList);

        size_t fenceValue = g_fence->GetCompletedValue();
        fenceValue++;
        g_commandQueue->Signal(g_fence, fenceValue);

        // wait to destroy staging buffer
        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        ::WaitForSingleObject(g_fenceEvent, INFINITE);

        g_indexBufferView.BufferLocation = g_indexBuffer->GetGPUVirtualAddress();
        g_indexBufferView.SizeInBytes = sizeof(g_indices);
        g_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    }

    //-----------------------------------------------------------------------------
    // Create Uniform Buffer
    //-----------------------------------------------------------------------------
    {
        // temporary solution
        D3D12_RESOURCE_DESC uboResourceDesc{};
        uboResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uboResourceDesc.Alignment = 0;
        uboResourceDesc.Width = (sizeof(ConstantBuffer) + 255) & ~255;
        uboResourceDesc.Width*=S_FRAME_COUNT;
        uboResourceDesc.Height = 1;
        uboResourceDesc.DepthOrArraySize = 1;
        uboResourceDesc.MipLevels = 1;
        uboResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        uboResourceDesc.SampleDesc.Count = 1;
        uboResourceDesc.SampleDesc.Quality = 0;
        uboResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        uboResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        S_D3D12(g_device->CreatePlacedResource(g_uniformHeap, 0, &uboResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_uniformBuffer)));

        D3D12_RANGE readRange{};
        readRange.Begin = 0;
        readRange.End = 0;

        // leaving this mapped
        S_D3D12(g_uniformBuffer->Map(0, &readRange,(void**)(&g_mapping)));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = g_uniformBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = (sizeof(ConstantBuffer) + 255) & ~255; // CB size is required to be 256-byte aligned.

        D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle(g_srvDescHeap->GetCPUDescriptorHandleForHeapStart());
        cbvHandle.ptr = cbvHandle.ptr + g_cbvDescriptorSize * 0;

        g_device->CreateConstantBufferView(&cbvDesc, cbvHandle);
    }

    //-----------------------------------------------------------------------------
    // Create Texture
    //-----------------------------------------------------------------------------
    {
        //-----------------------------------------------------------------------------
        // Final resource info
        //-----------------------------------------------------------------------------

        D3D12_RESOURCE_DESC textureDesc{};
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = 2;
        textureDesc.Height = 2;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        textureDesc.Alignment = 0;

        D3D12_HEAP_PROPERTIES defaultProperties;
        defaultProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        defaultProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultProperties.CreationNodeMask = 0;
        defaultProperties.VisibleNodeMask = 0;

        g_device->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&g_textureBuffer));

        D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
        shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        shaderResourceViewDesc.Format = textureDesc.Format;
        shaderResourceViewDesc.Texture2D.MipLevels = textureDesc.MipLevels;

        g_textureHandle = g_srvDescHeap->GetCPUDescriptorHandleForHeapStart();
        g_textureHandle.ptr = g_textureHandle.ptr + g_cbvDescriptorSize * 1;
        g_device->CreateShaderResourceView(g_textureBuffer, &shaderResourceViewDesc, g_textureHandle); 

        size_t textureMemorySize = 0;
        UINT numRows[1];
        size_t rowSizesInBytes[1];
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[1];
        const size_t numSubResources = textureDesc.MipLevels * textureDesc.DepthOrArraySize; 
        g_device->GetCopyableFootprints(&textureDesc, 0, 1, 0, layouts, numRows, rowSizesInBytes, &textureMemorySize);

        //-----------------------------------------------------------------------------
        // Upload data to staging buffer
        //-----------------------------------------------------------------------------

        // Copy the triangle data to the vertex buffer.
        unsigned char* pDataBegin = nullptr;

        // We do not intend to read from this resource on the CPU.
        D3D12_RANGE readRange{};
        readRange.Begin = 0;
        readRange.End = 0;

        UINT uploadPitch = ( textureDesc.Width*4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

        S_D3D12(g_uploadHeap->Map(0, &readRange, (void**)&pDataBegin));
        
        {
            auto pdata = (unsigned char*)image;
            size_t ppitch = textureDesc.Width*4;
            for(int i = 0; i <  textureDesc.Height; i++)
            {
                memcpy(pDataBegin, pdata, ppitch);
                pDataBegin+=uploadPitch;
                pdata+=ppitch;
            }
        }
        //memcpy(pDataBegin, data, texture->size);
        g_uploadHeap->Unmap(0, nullptr);

        
        D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
        srcLocation.pResource = g_uploadHeap;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint.Footprint = layouts[0].Footprint;

        D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
        dstLocation.pResource = g_textureBuffer;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = 0;
        dstLocation.PlacedFootprint.Footprint.Width = textureDesc.Width;
        dstLocation.PlacedFootprint.Footprint.Height = textureDesc.Height;
        dstLocation.PlacedFootprint.Footprint.Format = textureDesc.Format;
        dstLocation.PlacedFootprint.Footprint.RowPitch = textureDesc.Width;

        g_commandList->Reset(g_frameContext[g_frameIndex].commandAllocator, nullptr);
        g_commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

        //-----------------------------------------------------------------------------
        // layout transition
        //-----------------------------------------------------------------------------

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = g_textureBuffer;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        g_commandList->ResourceBarrier(1, &barrier);

        g_commandList->Close();
        g_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_commandList);
        size_t fenceValue = g_fenceLastSignaledValue + 1;
        g_commandQueue->Signal(g_fence, fenceValue);
        g_fenceLastSignaledValue = fenceValue;
        g_frameContext[g_frameIndex].fenceValue = fenceValue;
        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        ::WaitForSingleObject(g_fenceEvent, INFINITE);   
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

        g_surfaceSize.right = g_width;
        g_surfaceSize.bottom = g_height;
        g_viewport.Width = g_width;
        g_viewport.Height = g_height;

        //-----------------------------------------------------------------------------
        // wait for next frame
        //-----------------------------------------------------------------------------
        unsigned nextFrameIndex = g_frameIndex + 1;
        g_frameIndex = nextFrameIndex;

        HANDLE waitableObjects[] = { g_swapChainWaitableObject, nullptr };
        DWORD numWaitableObjects = 1;

        FrameContext* currentFrameCtx = &g_frameContext[nextFrameIndex % S_FRAME_COUNT];
        size_t fenceValue = currentFrameCtx->fenceValue;
        if (fenceValue != 0) // means no fence was signaled
        {
            currentFrameCtx->fenceValue = 0;
            g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
            waitableObjects[1] = g_fenceEvent;
            numWaitableObjects = 2;
        }

        ::WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);  
        currentFrameCtx->commandAllocator->Reset();
        g_commandList->Reset(currentFrameCtx->commandAllocator, nullptr);

        //-----------------------------------------------------------------------------
        // begin main pass
        //-----------------------------------------------------------------------------
        static float clear_color[4] = { 60.0f/255.0f, 60.0f/255.0f, 60.0f/255.0f, 1.0f };
        unsigned backBufferIdx = g_swapChain->GetCurrentBackBufferIndex();  

        unsigned char* dst = g_mapping;
        dst+=(g_frameIndex % S_FRAME_COUNT)*(sizeof(ConstantBuffer) + 255) & ~255;
        memcpy(dst, &g_vertexOffset, sizeof(ConstantBuffer)); 

        // transition to render target
        D3D12_RESOURCE_BARRIER toTargetBarrier = {};
        toTargetBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toTargetBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        toTargetBarrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx];
        toTargetBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toTargetBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        toTargetBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_commandList->ResourceBarrier(1, &toTargetBarrier);
        g_commandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, &g_depthStencilView);
        g_commandList->ClearRenderTargetView(g_mainRenderTargetDescriptor[backBufferIdx], clear_color, 0, nullptr);
        g_commandList->ClearDepthStencilView(g_depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0x00, 0, nullptr);
        g_commandList->RSSetViewports(1, &g_viewport);
        g_commandList->RSSetScissorRects(1, &g_surfaceSize);
        g_commandList->SetDescriptorHeaps(1, &g_srvDescHeap);

        g_commandList->SetPipelineState(g_pipelineState);
        g_commandList->SetGraphicsRootSignature(g_rootSignature);
        g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        {
            D3D12_GPU_DESCRIPTOR_HANDLE srvHandle(g_srvDescHeap->GetGPUDescriptorHandleForHeapStart());
            srvHandle.ptr = srvHandle.ptr + g_cbvDescriptorSize * 0;
            g_commandList->SetGraphicsRootDescriptorTable(0, srvHandle);  
        }
        {
            D3D12_GPU_DESCRIPTOR_HANDLE srvHandle(g_srvDescHeap->GetGPUDescriptorHandleForHeapStart());
            srvHandle.ptr = srvHandle.ptr + g_cbvDescriptorSize * 1;
            g_commandList->SetGraphicsRootDescriptorTable(1, srvHandle);  
        }

        g_commandList->IASetVertexBuffers(0, 1, &g_vertexBufferView);
        g_commandList->IASetIndexBuffer(&g_indexBufferView);
        g_commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

        //-----------------------------------------------------------------------------
        // end main pass
        //-----------------------------------------------------------------------------

        // transition to present
        D3D12_RESOURCE_BARRIER toPresentBarrier = {};
        toPresentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toPresentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        toPresentBarrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx];
        toPresentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toPresentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toPresentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_commandList->ResourceBarrier(1, &toPresentBarrier);

        // close and submit command list to gpu
        g_commandList->Close();
        g_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_commandList);

        //-----------------------------------------------------------------------------
        // present
        //-----------------------------------------------------------------------------
        static UINT presentFlags = 0;
        if (g_swapChain->Present(1, presentFlags) == DXGI_STATUS_OCCLUDED)
        {
            presentFlags = DXGI_PRESENT_TEST;
            Sleep(20);
        }
        else presentFlags = 0;
        size_t nfenceValue = g_fenceLastSignaledValue + 1;
        g_commandQueue->Signal(g_fence, nfenceValue);
        g_fenceLastSignaledValue = nfenceValue;
        currentFrameCtx->fenceValue = nfenceValue;   

    }

    //-----------------------------------------------------------------------------
    // cleanup
    //-----------------------------------------------------------------------------

    // g_swapChain->Release();
    // g_frameBuffer->Release();
    // g_frameBufferView->Release();
    // g_vsBlob->Release();
    // g_vertexShader->Release();
    // g_pixelShader->Release();
    // g_inputLayout->Release();
    // g_vertexBuffer->Release();
    // g_indexBuffer->Release();
    // g_textureView->Release();
    // g_sampler->Release();
    // g_constantBuffer->Release();
    // g_context->Release();
    // g_device->Release();

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