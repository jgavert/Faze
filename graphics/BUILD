cc_library(
  name = "graphics_coro",
  srcs = glob(["**/*.cpp"]),
  hdrs = glob(["**/*.hpp"]),
  strip_include_prefix = "src",
  deps = [
      "//core:core_coro",
      "//ext:IMGui",
      "//ext:DirectXShaderCompiler",
      "//ext:STB"] + select({
          "@bazel_tools//src/conditions:windows": 
              ["//ext:WinPixEventRuntime", 
               "@VulkanSDKWin//:Vulkan",
               "@DX12//:DX12", 
               "@DXGI//:DXGI", 
               "@DXGUID//:DXGUID",
               "@DXIL//:DXIL"],
          "//conditions:default": ["@VulkanSDKLinux//:Vulkan"],
    }),
  copts = select({
          "@bazel_tools//src/conditions:windows": ["/std:c++latest", "/arch:AVX2", "/permissive-", "/Z7", "-ftime-trace"],
          "//conditions:default": ["-std=c++2a", "-msse4.2", "-m64"],
    }),
  defines = ["_ENABLE_EXTENDED_ALIGNED_STORAGE"],
  visibility = ["//visibility:public"], 
)
cc_library(
  name = "graphics",
  srcs = glob(["**/*.cpp"]),
  hdrs = glob(["**/*.hpp"]),
  strip_include_prefix = "src",
  deps = [
      "//core:core",
      "//ext:IMGui",
      "//ext:DirectXShaderCompiler",
      "//ext:STB"] + select({
          "@bazel_tools//src/conditions:windows": 
              ["//ext:WinPixEventRuntime", 
               "@VulkanSDKWin//:Vulkan",
               "@DX12//:DX12", 
               "@DXGI//:DXGI", 
               "@DXGUID//:DXGUID",
               "@DXIL//:DXIL"],
          "//conditions:default": ["@VulkanSDKLinux//:Vulkan"],
    }),
  copts = select({
          "@bazel_tools//src/conditions:windows": ["/std:c++latest", "/arch:AVX2", "/permissive-", "/Z7", "-ftime-trace"],
          "//conditions:default": ["-std=c++2a", "-msse4.2", "-m64"],
    }),
  defines = ["_ENABLE_EXTENDED_ALIGNED_STORAGE"],
  visibility = ["//visibility:public"], 
)