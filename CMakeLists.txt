cmake_minimum_required(VERSION 3.18)
project(mgpu)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/dx11")
file(GLOB _source_list "${_src_dir}/*.cpp" "${_src_dir}/*.hpp")
set(_target "dx11")
add_executable(${_target} WIN32 ${_source_list})
target_link_libraries(${_target}
    PUBLIC
        d3d11
        d3dcompiler
        dxgi
)

set(_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/dx12")
file(GLOB _source_list "${_src_dir}/*.cpp" "${_src_dir}/*.hpp")
set(_target "dx12")
add_executable(${_target} WIN32 ${_source_list})
target_link_libraries(${_target}
    PRIVATE
        d3d12
        dxgi
        dxguid
        dxcompiler
        d3dcompiler
)
target_include_directories(${_target} PRIVATE ${_src_dir})
