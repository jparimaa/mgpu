#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_4.h>
#include <comdef.h>
#include <wrl/client.h>
#include <iostream>
#include <vector>

using Microsoft::WRL::ComPtr;

#define CHECK(f)                                                                                      \
    do                                                                                                \
    {                                                                                                 \
        if (!(f))                                                                                     \
        {                                                                                             \
            std::cerr << "Terminate. " << #f << " failed at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::terminate();                                                                         \
        }                                                                                             \
    } while (false)

#define CHECK_HR(f)                                                                                   \
    do                                                                                                \
    {                                                                                                 \
        HRESULT hr = f;                                                                               \
        if (FAILED((hr)))                                                                             \
        {                                                                                             \
            std::cerr << "Terminate. " << #f << " failed at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::cerr << _com_error(hr).ErrorMessage() << std::endl;                                  \
            std::terminate();                                                                         \
        }                                                                                             \
    } while (false)

const int c_width = 7680;
const int c_height = 3744;
const UINT c_swapChainFrameCount = 3;
DXGI_FORMAT c_format = DXGI_FORMAT_R8G8B8A8_UNORM;
const int c_gpuCount = 2;

const CD3DX12_RESOURCE_DESC c_textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    c_format,
    c_width,
    c_height,
    1u,
    1u,
    1,
    1,
    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
    D3D12_TEXTURE_LAYOUT_UNKNOWN,
    0u);

UINT align(UINT size, UINT alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

void enableConsole()
{
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
    freopen_s(&dummy, "CONIN$", "r", stdin);

    std::cout.clear();
    std::clog.clear();
    std::cerr.clear();
    std::cin.clear();
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

HWND createRenderWindow(HINSTANCE hInstance, int nCmdShow)
{
    const LPCSTR windowClassName = "DX11WindowClass";
    const LPCSTR windowTitle = "DirectX 11 Window";

    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = windowClassName;

    CHECK(RegisterClassEx(&wc));

    RECT rect = {0, 0, c_width, c_height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowEx(
        0, // Optional window styles
        windowClassName, // Window class name
        windowTitle, // Window title
        WS_OVERLAPPEDWINDOW, // Window style
        CW_USEDEFAULT,
        CW_USEDEFAULT, // Position
        rect.right - rect.left, // Width
        rect.bottom - rect.top, // Height
        nullptr, // Parent window
        nullptr, // Menu
        hInstance, // Instance handle
        nullptr // Additional application data
    );

    CHECK(hwnd);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    return hwnd;
}

ComPtr<IDXGIFactory4> createFactory()
{
    UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
    { // Enable the debug layer (requires the Graphics Tools "optional feature").
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    CHECK_HR(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
    return factory;
}

std::vector<ComPtr<IDXGIAdapter>> getAdapters(IDXGIFactory4* factory)
{
    UINT i = 0;
    IDXGIAdapter* adapter;
    std::vector<ComPtr<IDXGIAdapter>> adapters;
    while (factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        adapters.push_back(adapter);
        ++i;
    }
    return adapters;
}

void printAdapters(std::vector<ComPtr<IDXGIAdapter>>& adapters)
{
    int i = 0;
    for (ComPtr<IDXGIAdapter> adapter : adapters)
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        std::wcout << "Adapter " << i << ": " << desc.Description << "\n";
        ++i;
    }
}

ComPtr<ID3D12Device> createDevice(ComPtr<IDXGIAdapter> adapter)
{
    ComPtr<ID3D12Device> device;
    CHECK_HR(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
    return device;
}

bool isCrossAdapterSupported(ComPtr<ID3D12Device> device)
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
    CHECK_HR(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, reinterpret_cast<void*>(&options), sizeof(options)));
    return options.CrossAdapterRowMajorTextureSupported;
}

ComPtr<ID3D12CommandQueue> createCommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type)
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = type;

    ComPtr<ID3D12CommandQueue> queue;

    CHECK_HR(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)));
    return queue;
}

ComPtr<IDXGISwapChain3> createSwapChain(ComPtr<IDXGIFactory4> factory, ComPtr<ID3D12CommandQueue> queue, HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = c_swapChainFrameCount;
    swapChainDesc.Width = c_width;
    swapChainDesc.Height = c_height;
    swapChainDesc.Format = c_format;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    CHECK_HR(factory->CreateSwapChainForHwnd(
        queue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain));

    ComPtr<IDXGISwapChain3> swapChain3;
    CHECK_HR(swapChain.As(&swapChain3));
    return swapChain3;
}

std::vector<ComPtr<ID3D12Resource>> getBackBuffers(ComPtr<IDXGISwapChain3> swapChain)
{
    std::vector<ComPtr<ID3D12Resource>> backBuffers(c_swapChainFrameCount);
    for (int i = 0; i < c_swapChainFrameCount; ++i)
    {
        CHECK_HR(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])));
    }
    return backBuffers;
}

ComPtr<ID3D12DescriptorHeap> createRtvHeap(ComPtr<ID3D12Device> device)
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.NumDescriptors = c_swapChainFrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ComPtr<ID3D12DescriptorHeap> heap;
    CHECK_HR(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&heap)));
    return heap;
}

std::vector<ComPtr<ID3D12Resource>> createTextures(ComPtr<ID3D12Device> device)
{
    const float clearColor[4] = {0.0f, 0.2f, 0.3f, 1.0f};
    const CD3DX12_CLEAR_VALUE clearValue(c_format, clearColor);

    std::vector<ComPtr<ID3D12Resource>> textures(c_swapChainFrameCount);
    for (int i = 0; i < c_swapChainFrameCount; ++i)
    {
        CHECK_HR(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &c_textureDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &clearValue,
            IID_PPV_ARGS(&textures[i])));
    }
    return textures;
}

void createRtvs(ComPtr<ID3D12Device> device, ComPtr<ID3D12DescriptorHeap> heap, const std::vector<ComPtr<ID3D12Resource>>& textures)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(heap->GetCPUDescriptorHandleForHeapStart());
    const UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (int i = 0; i < c_swapChainFrameCount; ++i)
    {
        device->CreateRenderTargetView(textures[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, rtvDescriptorSize);
    }
}

std::vector<ComPtr<ID3D12CommandAllocator>> createCommandAllocators(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type)
{
    std::vector<ComPtr<ID3D12CommandAllocator>> allocators(c_swapChainFrameCount);
    for (int i = 0; i < c_swapChainFrameCount; ++i)
    {
        CHECK_HR(device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocators[i])));
    }
    return allocators;
}

ComPtr<ID3D12GraphicsCommandList> createCommandList(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator> allocator)
{
    ComPtr<ID3D12GraphicsCommandList> list;
    CHECK_HR(device->CreateCommandList(0, type, allocator.Get(), nullptr, IID_PPV_ARGS(&list)));
    return list;
}

ComPtr<ID3D12Heap> createSharedHeap(ComPtr<ID3D12Device> device)
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    device->GetCopyableFootprints(&c_textureDesc, 0, 1, 0, &layout, nullptr, nullptr, nullptr);
    UINT textureSize = align(layout.Footprint.RowPitch * layout.Footprint.Height);

    CD3DX12_HEAP_DESC heapDesc(
        textureSize * c_swapChainFrameCount,
        D3D12_HEAP_TYPE_DEFAULT,
        0,
        D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);

    ComPtr<ID3D12Heap> heap;
    CHECK_HR(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)));
    return heap;
}

HANDLE createSharedHeapHandle(ComPtr<ID3D12Device> device, ComPtr<ID3D12Heap> heap)
{
    HANDLE heapHandle = nullptr;
    CHECK_HR(device->CreateSharedHandle(
        heap.Get(),
        nullptr,
        GENERIC_ALL,
        nullptr,
        &heapHandle));

    return heapHandle;
}

ComPtr<ID3D12Heap> openSharedHeapHandle(ComPtr<ID3D12Device> device, HANDLE handle)
{
    ComPtr<ID3D12Heap> sharedHeap;
    CHECK_HR(device->OpenSharedHandle(handle, IID_PPV_ARGS(&sharedHeap)));
    return sharedHeap;
}

std::vector<ComPtr<ID3D12Resource>> createSharedHeapTexture(ComPtr<ID3D12Device> device, ComPtr<ID3D12Heap> heap, D3D12_RESOURCE_STATES states)
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    device->GetCopyableFootprints(&c_textureDesc, 0, 1, 0, &layout, nullptr, nullptr, nullptr);
    UINT textureSize = align(layout.Footprint.RowPitch * layout.Footprint.Height);
    D3D12_RESOURCE_DESC crossAdapterDesc = CD3DX12_RESOURCE_DESC::Buffer(textureSize, D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    std::vector<ComPtr<ID3D12Resource>> resources(c_swapChainFrameCount);

    for (int i = 0; i < c_swapChainFrameCount; ++i)
    {
        CHECK_HR(device->CreatePlacedResource(
            heap.Get(),
            textureSize * i,
            &crossAdapterDesc,
            states,
            nullptr,
            IID_PPV_ARGS(&resources[i])));
    }

    return resources;
}

ComPtr<ID3D12Fence> createFence(ComPtr<ID3D12Device> device, D3D12_FENCE_FLAGS flags)
{
    ComPtr<ID3D12Fence> fence;
    CHECK_HR(device->CreateFence(1, flags, IID_PPV_ARGS(&fence)));
    return fence;
}

HANDLE createSharedFenceHandle(ComPtr<ID3D12Device> device, ComPtr<ID3D12Fence> fence)
{
    HANDLE fenceHandle = nullptr;
    CHECK_HR(device->CreateSharedHandle(
        fence.Get(),
        nullptr,
        GENERIC_ALL,
        nullptr,
        &fenceHandle));

    return fenceHandle;
}

ComPtr<ID3D12Fence> openSharedFenceHandle(ComPtr<ID3D12Device> device, HANDLE handle)
{
    ComPtr<ID3D12Fence> sharedFence;
    CHECK_HR(device->OpenSharedHandle(handle, IID_PPV_ARGS(&sharedFence)));
    return sharedFence;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    /*
    - render (=clear) to a texture on device1
    - copy the result to shared heap 
    - copy from shared heap to swapchain backbuffer    
    */
    enableConsole();
    HWND hwnd = createRenderWindow(hInstance, nCmdShow);

    ComPtr<IDXGIFactory4> factory = createFactory();
    std::vector<ComPtr<IDXGIAdapter>> adapters = getAdapters(factory.Get());
    printAdapters(adapters);

    ComPtr<ID3D12Device> device0 = createDevice(adapters[0]);
    ComPtr<ID3D12Device> device1 = createDevice(adapters[1]);

    //CHECK(isCrossAdapterSupported(device0));
    //CHECK(isCrossAdapterSupported(device1));

    ComPtr<ID3D12CommandQueue> directQueue0 = createCommandQueue(device0, D3D12_COMMAND_LIST_TYPE_DIRECT);
    ComPtr<ID3D12CommandQueue> directQueue1 = createCommandQueue(device1, D3D12_COMMAND_LIST_TYPE_DIRECT);
    ComPtr<ID3D12CommandQueue> copyQueue1 = createCommandQueue(device1, D3D12_COMMAND_LIST_TYPE_COPY);

    ComPtr<IDXGISwapChain3> swapChain = createSwapChain(factory, directQueue0, hwnd);
    std::vector<ComPtr<ID3D12Resource>> backBuffers = getBackBuffers(swapChain);
    int frameIndex = swapChain->GetCurrentBackBufferIndex();

    ComPtr<ID3D12DescriptorHeap> rtvHeap0 = createRtvHeap(device0);
    createRtvs(device0, rtvHeap0, backBuffers);

    ComPtr<ID3D12DescriptorHeap> rtvHeap1 = createRtvHeap(device1);
    std::vector<ComPtr<ID3D12Resource>> textures = createTextures(device1);
    createRtvs(device1, rtvHeap1, textures);

    std::vector<ComPtr<ID3D12CommandAllocator>> commandAllocators0 = createCommandAllocators(device0, D3D12_COMMAND_LIST_TYPE_DIRECT);
    std::vector<ComPtr<ID3D12CommandAllocator>> commandAllocators1 = createCommandAllocators(device1, D3D12_COMMAND_LIST_TYPE_DIRECT);
    std::vector<ComPtr<ID3D12CommandAllocator>> copyCommandAllocators1 = createCommandAllocators(device1, D3D12_COMMAND_LIST_TYPE_COPY);

    ComPtr<ID3D12GraphicsCommandList> list0 = createCommandList(device0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators0[0]);
    ComPtr<ID3D12GraphicsCommandList> list1 = createCommandList(device1, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators1[0]);
    ComPtr<ID3D12GraphicsCommandList> copyList1 = createCommandList(device1, D3D12_COMMAND_LIST_TYPE_COPY, copyCommandAllocators1[0]);

    ComPtr<ID3D12Heap> sharedHeap1 = createSharedHeap(device1);
    HANDLE sharedHeapHandle = createSharedHeapHandle(device1, sharedHeap1);
    ComPtr<ID3D12Heap> sharedHeap0 = openSharedHeapHandle(device0, sharedHeapHandle);

    std::vector<ComPtr<ID3D12Resource>> sharedHeapResources0 = createSharedHeapTexture(device0, sharedHeap0, D3D12_RESOURCE_STATE_COPY_DEST);
    std::vector<ComPtr<ID3D12Resource>> sharedHeapResources1 = createSharedHeapTexture(device1, sharedHeap1, D3D12_RESOURCE_STATE_RENDER_TARGET);

    ComPtr<ID3D12Fence> frameFence = createFence(device0, D3D12_FENCE_FLAG_NONE);
    ComPtr<ID3D12Fence> renderFence = createFence(device1, D3D12_FENCE_FLAG_NONE);
    ComPtr<ID3D12Fence> crossAdapterFence1 = createFence(device1, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER);
    HANDLE sharedFenceHandle = createSharedFenceHandle(device1, crossAdapterFence1);
    ComPtr<ID3D12Fence> crossAdapterFence0 = openSharedFenceHandle(device0, sharedFenceHandle);
    std::vector<UINT64> frameFenceValues(c_swapChainFrameCount, 0);
    UINT64 presentFenceValue = 2;
    std::vector<HANDLE> fenceEvents(c_gpuCount, CreateEvent(nullptr, FALSE, FALSE, nullptr));

    const UINT rtvDescriptorSize = device0->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    bool running = true;
    float blue = 0.0f;

    bool first = true;

    CHECK_HR(list0->Close());
    CHECK_HR(list1->Close());
    CHECK_HR(copyList1->Close());

    while (running)
    {
        MSG msg = {};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        CHECK_HR(commandAllocators0[frameIndex]->Reset());
        CHECK_HR(list0->Reset(commandAllocators0[frameIndex].Get(), nullptr));

        ID3D12Resource* backBuffer = backBuffers[frameIndex].Get();
        D3D12_RESOURCE_STATES state = first ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_PRESENT;
        list0->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, state, D3D12_RESOURCE_STATE_RENDER_TARGET));

        blue = blue > 1.0f ? 0.0f : blue + 0.01f;
        float clearColor[4] = {0.0f, 0.2f, blue, 1.0f};
        CD3DX12_CPU_DESCRIPTOR_HANDLE backBufferRtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap0->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);

        list0->ClearRenderTargetView(backBufferRtv, clearColor, 0, nullptr);

        list0->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        CHECK_HR(list0->Close());

        std::vector<ID3D12CommandList*> cmdLists{list0.Get()};
        directQueue0->ExecuteCommandLists(static_cast<UINT>(cmdLists.size()), cmdLists.data());

        swapChain->Present(1, 0);

        CHECK_HR(directQueue0->Signal(frameFence.Get(), presentFenceValue));
        frameFenceValues[frameIndex] = presentFenceValue;
        ++presentFenceValue;

        frameIndex = swapChain->GetCurrentBackBufferIndex();

        const UINT64 completedFenceValue = frameFence->GetCompletedValue();
        if (completedFenceValue < frameFenceValues[frameIndex])
        {
            CHECK_HR(frameFence->SetEventOnCompletion(frameFenceValues[frameIndex], fenceEvents[0]));
            WaitForSingleObject(fenceEvents[0], INFINITE);
        }

        first = false;
    }

    return 0;
}