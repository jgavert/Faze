#include "tests/graphics/graphics_config.hpp"

SResources::SResources()
  : subsystem("higanbana", true)
  , fileSystem(TESTS_FILESYSTEM_PATH, FileSystem::MappingMode::TryFirstMappingFile, "tests\\data\\mapping")
{
  auto gpus = subsystem.getVendorDevice(GraphicsApi::Vulkan); // TODO: make it so that it always tests both api at once
  gpu = subsystem.createDevice(fileSystem, gpus);
}

std::shared_ptr<SResources> _g;

GraphicsFixture::GraphicsFixture() {
  if (!_initialised) {
    _initialised = true;

    _g = std::make_shared<SResources>();
  }
  else
  {
    _g->gpu.waitGpuIdle();
  }
}
bool GraphicsFixture::_initialised = false;


GpuGroup& GraphicsFixture::gpu() { 
  return _g->gpu;
}