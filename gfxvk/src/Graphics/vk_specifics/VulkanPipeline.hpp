#pragma once
#include "core/src/memory/ManagedResource.hpp"
#include "gfxvk/src/Graphics/PipelineDescriptor.hpp"
#include <vulkan/vk_cpp.h>

class VulkanPipeline
{
private:
  friend class VulkanGpuDevice;

  FazPtrVk<vk::Pipeline>    m_pipeline;
  GraphicsPipelineDescriptor m_graphDesc;
  ComputePipelineDescriptor m_computeDesc;

  VulkanPipeline() {}

  VulkanPipeline(FazPtrVk<vk::Pipeline> pipeline, GraphicsPipelineDescriptor graphDesc);
  VulkanPipeline(FazPtrVk<vk::Pipeline> pipeline, ComputePipelineDescriptor computeDesc);
public:

  GraphicsPipelineDescriptor graphDesc()
  {
    return m_graphDesc;
  }

  ComputePipelineDescriptor computeDesc()
  {
    return m_computeDesc;
  }

  bool isValid();
};

using PipelineImpl = VulkanPipeline;