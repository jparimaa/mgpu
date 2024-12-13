# mgpu

Minimal tests of using multiple GPUs with DirectX 11 and 12. 
dx11 and dx12: Renders on one GPU and copies the result over host memory to another GPU.
dx12direct: Renders on one GPU and copies the result directly to another GPU using shared handles and D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER

## Build

Run CMake, build and run on up-to-date Windows system. No additional dependencies.
