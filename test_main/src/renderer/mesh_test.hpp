#pragma once

#include "../world/visual_data_structures.hpp"
#include "camera.hpp"
#include <higanbana/graphics/GraphicsCore.hpp>

SHADER_STRUCT(WorldMeshlet,
  uint primitives;
  uint vertices;
  uint offsetUnique;
  uint offsetPacked;
);

namespace app::renderer
{
class MeshTest
{
  higanbana::ShaderArgumentsLayout m_meshArgumentsLayout;
  higanbana::GraphicsPipeline m_pipeline;
  higanbana::Renderpass m_renderpass;
public:
  MeshTest(higanbana::GpuGroup& device, higanbana::ShaderArgumentsLayout camerasLayout, higanbana::ShaderArgumentsLayout materials);
  higanbana::ShaderArgumentsLayout& meshArgLayout() { return m_meshArgumentsLayout; }
  void beginRenderpass(higanbana::CommandGraphNode& node, higanbana::TextureRTV& target, higanbana::TextureDSV& depth);
  void renderMesh(higanbana::CommandGraphNode& node, higanbana::BufferIBV ibv, higanbana::ShaderArguments cameras, higanbana::ShaderArguments meshBuffers, higanbana::ShaderArguments materials, uint meshlets, int cameraIndex);
};
}