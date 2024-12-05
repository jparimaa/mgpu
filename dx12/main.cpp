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

ComPtr<ID3D12CommandQueue> createCommandQueue(ComPtr<ID3D12Device> device)
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ComPtr<ID3D12CommandQueue> queue;

    CHECK_HR(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)));
    return queue;
}

ComPtr<IDXGISwapChain1> createSwapChain(ComPtr<IDXGIFactory4> factory, ComPtr<ID3D12CommandQueue> queue, HWND hwnd)
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

    return swapChain;
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

ComPtr<ID3D12CommandAllocator> createCommandAllocator(ComPtr<ID3D12Device> device)
{
    ComPtr<ID3D12CommandAllocator> allocator;
    CHECK_HR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
    return allocator;
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

HANDLE createSharedHandle(ComPtr<ID3D12Device> device, ComPtr<ID3D12Heap> heap)
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

ComPtr<ID3D12Heap> openSharedHandle(ComPtr<ID3D12Device> device, HANDLE handle)
{
    ComPtr<ID3D12Heap> sharedHeap;
    CHECK_HR(device->OpenSharedHandle(handle, IID_PPV_ARGS(&sharedHeap)));
    return sharedHeap;
}

std::vector<ComPtr<ID3D12Resource>> createSharedHeapResource(ComPtr<ID3D12Device> device, ComPtr<ID3D12Heap> heap, D3D12_RESOURCE_STATES states)
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    device->GetCopyableFootprints(&c_textureDesc, 0, 1, 0, &layout, nullptr, nullptr, nullptr);
    UINT textureSize = align(layout.Footprint.RowPitch * layout.Footprint.Height);
    D3D12_RESOURCE_DESC crossAdapterDesc = CD3DX12_RESOURCE_DESC::Buffer(textureSize, D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER);

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

    ComPtr<ID3D12CommandQueue> queue0 = createCommandQueue(device0);
    ComPtr<ID3D12CommandQueue> queue1 = createCommandQueue(device1);

    ComPtr<IDXGISwapChain1> swapChain = createSwapChain(factory, queue0, hwnd);

    ComPtr<ID3D12DescriptorHeap> rtvHeap1 = createRtvHeap(device1);
    std::vector<ComPtr<ID3D12Resource>> textures = createTextures(device1);
    createRtvs(device1, rtvHeap1, textures);

    ComPtr<ID3D12CommandAllocator> commandAllocator0 = createCommandAllocator(device0);
    ComPtr<ID3D12CommandAllocator> commandAllocator1 = createCommandAllocator(device1);

    ComPtr<ID3D12Heap> sharedHeap1 = createSharedHeap(device1);
    HANDLE sharedHeapHandle = createSharedHandle(device1, sharedHeap1);
    ComPtr<ID3D12Heap> sharedHeap0 = openSharedHandle(device0, sharedHeapHandle);

    std::vector<ComPtr<ID3D12Resource>> sharedHeapResources0 = createSharedHeapResource(device0, sharedHeap0, D3D12_RESOURCE_STATE_COPY_SOURCE);
    std::vector<ComPtr<ID3D12Resource>> sharedHeapResources1 = createSharedHeapResource(device1, sharedHeap1, D3D12_RESOURCE_STATE_COPY_DEST);
    /*
    createSharedHeap on 1
    createshared texture 
    createSharedHandle on 1
    open shared handle 2
    */
    return 0;
}