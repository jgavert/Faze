workspace(name = "faze")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")

new_git_repository(
    name = "googletest",
    build_file = "gmock.BUILD",
    remote = "https://github.com/google/googletest",
    branch = "master"
)

# mostly windows repositories

new_local_repository(
  name = "VulkanSDKWin",
  path = "C:\\VulkanSDK\\1.1.106.0",
  build_file_content = """
cc_library(
    name = "Vulkan_headers",
    hdrs = ["Include/Vulkan/vulkan.h","Include/Vulkan/vulkan.hpp"],
    includes = ["Include"],
    visibility = ["//visibility:public"], 
)

cc_import(
    name = "Vulkan_lib",
    interface_library = "Lib/vulkan-1.lib",
    system_provided = 1,
    visibility = ["//visibility:public"], 
)

cc_library(
    name = "Vulkan",
    deps = [
         "Vulkan_headers",
         "Vulkan_lib",
    ],
    visibility = ["//visibility:public"], 
)
"""
)

new_local_repository(
  name = "DX12",
  path = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.17763.0\\um\\x64",
  build_file_content = """
cc_import(
    name = "DX12",
    interface_library = "d3d12.lib",
    system_provided = 1,
    visibility = ["//visibility:public"], 
)
    """
)

new_local_repository(
  name = "DXGI",
  path = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.17763.0\\um\\x64",
  build_file_content = """
cc_import(
    name = "DXGI",
    interface_library = "dxgi.lib",
    system_provided = 1,
    visibility = ["//visibility:public"], 
)
  """
)

new_local_repository(
  name = "DXGUID",
  path = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.17763.0\\um\\x64",
  build_file_content = """
cc_import(
    name = "DXGUID",
    interface_library = "dxguid.lib",
    system_provided = 1,
    visibility = ["//visibility:public"], 
)
  """
)