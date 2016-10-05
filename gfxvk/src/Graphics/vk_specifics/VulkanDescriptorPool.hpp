#pragma once

#include <vulkan/vulkan.hpp>

class VulkanDescriptorPool
{
private:
  friend class VulkanGpuDevice;
  friend class VulkanCmdBuffer;
  vk::DescriptorPool pool;
public:
  VulkanDescriptorPool(vk::DescriptorPool pool)
    : pool(pool)
  {

  }
};

using DescriptorPoolImpl = VulkanDescriptorPool;