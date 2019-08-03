#include "higanbana/graphics/common/command_packets.hpp"

namespace higanbana
{
  namespace gfxpacket
  {
    const char* packetTypeToString(backend::PacketType type)
    {
      switch (type)
      {
      case backend::PacketType::BufferCopy:
        return "BufferCopy";
      case backend::PacketType::Dispatch:
        return "Dispatch";
      case backend::PacketType::PrepareForPresent:
        return "PrepareForPresent";
      case backend::PacketType::RenderpassBegin:
        return "RenderpassBegin";
      case backend::PacketType::RenderpassEnd:
        return "RenderpassEnd";
      case backend::PacketType::GraphicsPipelineBind:
        return "GraphicsPipelineBind";
      case backend::PacketType::ComputePipelineBind:
        return "ComputePipelineBind";
      case backend::PacketType::ResourceBinding:
        return "ResourceBinding";
      case backend::PacketType::Draw:
        return "Draw";
      case backend::PacketType::RenderBlock:
        return "RenderBlock";
      default:
        return "Unknown";
      }
    }
  }
}