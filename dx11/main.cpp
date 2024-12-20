#include <d3d11_1.h>
#include <comdef.h>
#include <windows.h>

#include <iostream>
#include <vector>
#include <fstream>

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

template<typename T>
void releaseDXPtr(T*& ptr)
{
    if (ptr != nullptr)
    {
        ptr->Release();
        ptr = nullptr;
    }
}

const int c_width = 7680;
const int c_height = 3744;

const D3D11_VIEWPORT c_viewport{
    0.0f,
    0.0f,
    static_cast<float>(c_width),
    static_cast<float>(c_height),
    0.0f,
    1.0f,
};

struct AdapterEnv
{
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
};

struct QueryData
{
    void release()
    {
        releaseDXPtr(startQuery);
        releaseDXPtr(endQuery);
        releaseDXPtr(disjointQuery);
    }
    ID3D11Query* startQuery = nullptr;
    ID3D11Query* endQuery = nullptr;
    ID3D11Query* disjointQuery = nullptr;
};

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

IDXGIFactory1* createFactory()
{
    IDXGIFactory1* factory = nullptr;

    CHECK_HR(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory));
    return factory;
}

std::vector<IDXGIAdapter*> getAdapters(IDXGIFactory1* factory)
{
    UINT i = 0;
    IDXGIAdapter* adapter;
    std::vector<IDXGIAdapter*> adapters;
    while (factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        adapters.push_back(adapter);
        ++i;
    }
    return adapters;
}

void printAdapters(std::vector<IDXGIAdapter*>& adapters)
{
    int i = 0;
    for (IDXGIAdapter* adapter : adapters)
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        std::wcout << "Adapter " << i << ": " << desc.Description << "\n";
        ++i;
    }
}

AdapterEnv createAdapterEnv(IDXGIAdapter* adapter)
{
    const UINT flags =
#ifdef _DEBUG
        D3D11_CREATE_DEVICE_DEBUG;
#else
        0;
#endif
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    CHECK_HR(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, 0, flags, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context));
    return AdapterEnv{device, context};
}

ID3D11Texture2D* createTexture(ID3D11Device* device)
{
    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = c_width;
    textureDesc.Height = c_height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    ID3D11Texture2D* texture = nullptr;
    CHECK_HR(device->CreateTexture2D(&textureDesc, nullptr, &texture));

    return texture;
}

ID3D11RenderTargetView* createRtv(ID3D11Device* device, ID3D11Texture2D* texture)
{
    ID3D11RenderTargetView* rtv = nullptr;
    CHECK_HR(device->CreateRenderTargetView(texture, nullptr, &rtv));
    return rtv;
}

ID3D11Texture2D* createStagingTexture(ID3D11Device* device, ID3D11Texture2D* originalTexture)
{
    D3D11_TEXTURE2D_DESC desc{};
    originalTexture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;

    ID3D11Texture2D* stagingTexture = nullptr;
    CHECK_HR(device->CreateTexture2D(&desc, nullptr, &stagingTexture));
    return stagingTexture;
}

IDXGISwapChain* createSwapChain(HWND hwnd, ID3D11Device* device)
{
    IDXGIDevice* dxgiDevice = nullptr;
    CHECK_HR(device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice));

    IDXGIAdapter* adapter = nullptr;
    CHECK_HR(dxgiDevice->GetAdapter(&adapter));

    IDXGIFactory1* factory = nullptr;
    CHECK_HR(adapter->GetParent(__uuidof(IDXGIFactory1), (void**)&factory));

    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = c_width;
    swapChainDesc.BufferDesc.Height = c_height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* swapChain = nullptr;
    CHECK_HR(factory->CreateSwapChain(device, &swapChainDesc, &swapChain));

    factory->Release();
    adapter->Release();
    dxgiDevice->Release();

    return swapChain;
}

ID3D11RenderTargetView* createWindowRtv(IDXGISwapChain* swapChain, ID3D11Device* device)
{
    ID3D11Texture2D* backBuffer = nullptr;
    CHECK_HR(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer));

    ID3D11RenderTargetView* rtv = nullptr;
    CHECK_HR(device->CreateRenderTargetView(backBuffer, nullptr, &rtv));
    backBuffer->Release();

    return rtv;
}

QueryData createQueryData(ID3D11Device* device)
{
    D3D11_QUERY_DESC timestampDesc = {D3D11_QUERY_TIMESTAMP, 0};
    D3D11_QUERY_DESC disjointDesc = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};

    ID3D11Query* startQuery = nullptr;
    ID3D11Query* endQuery = nullptr;
    ID3D11Query* disjointQuery = nullptr;

    device->CreateQuery(&timestampDesc, &startQuery);
    device->CreateQuery(&timestampDesc, &endQuery);
    device->CreateQuery(&disjointDesc, &disjointQuery);

    return QueryData{startQuery, endQuery, disjointQuery};
}

IDXGIFactory1* m_factory = nullptr;
AdapterEnv m_adapterEnv0;
AdapterEnv m_adapterEnv1;
ID3D11Texture2D* m_texture = nullptr;
ID3D11RenderTargetView* m_rtv = nullptr;
ID3D11Texture2D* m_stagingTexture = nullptr;
IDXGISwapChain* m_swapChain = nullptr;
ID3D11RenderTargetView* m_windowRtv = nullptr;
QueryData m_queryData0;
QueryData m_queryData1;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    /*
    - render on adapter (gpu) 1
    - copy the result from adapter 1 to host memory
    - copy the result from host memory to adapter 0 
    - present the result on adapter 0
    */

    enableConsole();
    HWND hwnd = createRenderWindow(hInstance, nCmdShow);

    m_factory = createFactory();
    std::vector<IDXGIAdapter*> adapters = getAdapters(m_factory);
    printAdapters(adapters);
    CHECK(adapters.size() >= 2);

    m_adapterEnv0 = createAdapterEnv(adapters[0]);
    m_adapterEnv1 = createAdapterEnv(adapters[1]);

    m_texture = createTexture(m_adapterEnv1.device);
    m_rtv = createRtv(m_adapterEnv1.device, m_texture);
    m_stagingTexture = createStagingTexture(m_adapterEnv1.device, m_texture);

    m_swapChain = createSwapChain(hwnd, m_adapterEnv0.device);
    m_windowRtv = createWindowRtv(m_swapChain, m_adapterEnv0.device);

    m_queryData0 = createQueryData(m_adapterEnv0.device);
    m_queryData1 = createQueryData(m_adapterEnv1.device);

    bool running = true;
    float blue = 0.0f;

    ID3D11Texture2D* backBuffer = nullptr;
    CHECK_HR(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer));

    std::vector<double> copyTimes0;
    std::vector<double> copyTimes1;

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

        // "Rendering", here only the render target is cleared though
        blue = blue > 1.0f ? 0.0f : blue + 0.01f;
        float clearColor[4] = {0.0f, 0.2f, blue, 1.0f};
        m_adapterEnv1.context->RSSetViewports(1, &c_viewport);
        m_adapterEnv1.context->OMSetRenderTargets(1, &m_rtv, nullptr);
        m_adapterEnv1.context->ClearRenderTargetView(m_rtv, clearColor);

        {
            // Copy from adapter 1 to host memory
            m_adapterEnv1.context->Begin(m_queryData1.disjointQuery);
            m_adapterEnv1.context->End(m_queryData1.startQuery);
            m_adapterEnv1.context->CopyResource(m_stagingTexture, m_texture);
            m_adapterEnv1.context->End(m_queryData1.endQuery);
            m_adapterEnv1.context->End(m_queryData1.disjointQuery);
            UINT64 startTime = 0, endTime = 0;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;

            while (m_adapterEnv1.context->GetData(m_queryData1.disjointQuery, &disjointData, sizeof(disjointData), 0) != S_OK)
                ;
            while (m_adapterEnv1.context->GetData(m_queryData1.startQuery, &startTime, sizeof(startTime), 0) != S_OK)
                ;
            while (m_adapterEnv1.context->GetData(m_queryData1.endQuery, &endTime, sizeof(endTime), 0) != S_OK)
                ;

            if (!disjointData.Disjoint)
            {
                double duration = static_cast<double>(endTime - startTime) / disjointData.Frequency;
                copyTimes1.push_back(duration);
            }
        }

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        m_adapterEnv1.context->Map(m_stagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
        {
            // Copy from host memory to adapter 0
            m_adapterEnv0.context->Begin(m_queryData0.disjointQuery);
            m_adapterEnv0.context->End(m_queryData0.startQuery);
            m_adapterEnv0.context->UpdateSubresource(backBuffer, 0, nullptr, mappedResource.pData, mappedResource.RowPitch, 0);
            m_adapterEnv0.context->End(m_queryData0.endQuery);
            m_adapterEnv0.context->End(m_queryData0.disjointQuery);
            UINT64 startTime = 0, endTime = 0;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;

            while (m_adapterEnv0.context->GetData(m_queryData0.disjointQuery, &disjointData, sizeof(disjointData), 0) != S_OK)
                ;
            while (m_adapterEnv0.context->GetData(m_queryData0.startQuery, &startTime, sizeof(startTime), 0) != S_OK)
                ;
            while (m_adapterEnv0.context->GetData(m_queryData0.endQuery, &endTime, sizeof(endTime), 0) != S_OK)
                ;

            if (!disjointData.Disjoint)
            {
                double duration = static_cast<double>(endTime - startTime) / disjointData.Frequency;
                copyTimes0.push_back(duration);
            }
        }
        m_adapterEnv1.context->Unmap(m_stagingTexture, 0);
        m_swapChain->Present(1, 0);
    }

    m_queryData0.release();
    m_queryData1.release();
    releaseDXPtr(m_windowRtv);
    releaseDXPtr(m_swapChain);
    releaseDXPtr(m_stagingTexture);
    releaseDXPtr(m_rtv);
    releaseDXPtr(m_texture);
    releaseDXPtr(m_adapterEnv0.context);
    releaseDXPtr(m_adapterEnv0.device);
    releaseDXPtr(m_adapterEnv1.context);
    releaseDXPtr(m_adapterEnv1.device);
    for (IDXGIAdapter* adapter : adapters)
    {
        releaseDXPtr(adapter);
    }
    releaseDXPtr(m_factory);

    double copyTimeTotal1 = 0.0;
    for (double t : copyTimes1)
    {
        copyTimeTotal1 += t;
    }

    double copyTimeTotal0 = 0.0;
    for (double t : copyTimes0)
    {
        copyTimeTotal0 += t;
    }

    std::ofstream myfile;
    myfile.open("dx11out.txt");
    myfile << "Average copy times" << std::endl
           << "0: " << (copyTimeTotal0 / copyTimes0.size() * 1000.0) << "ms" << std::endl
           << "1: " << (copyTimeTotal1 / copyTimes1.size() * 1000.0) << "ms" << std::endl;
    myfile.close();

    return 0;
}