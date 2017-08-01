#include "dx12resources.hpp"
#include "util/dxDependencySolver.hpp"

#include <algorithm>

namespace faze
{
  namespace backend
  {
    void handle(ID3D12GraphicsCommandList* buffer, gfxpacket::Subpass& packet)
    {
      // set viewport and rendertargets
      uint2 size;
      if (packet.rtvs.size() > 0)
      {
        size = uint2{ packet.rtvs[0].desc().desc.width, packet.rtvs[0].desc().desc.height };
      }
      else if (packet.dsvs.size() > 0)
      {
        size = uint2{ packet.dsvs[0].desc().desc.width, packet.dsvs[0].desc().desc.height };
      }
      else
      {
        return;
      }
      D3D12_VIEWPORT port{};
      port.Width = float(size.x());
      port.Height = float(size.y());
      port.MinDepth = D3D12_MIN_DEPTH;
      port.MaxDepth = D3D12_MAX_DEPTH;
      buffer->RSSetViewports(1, &port);

	  D3D12_RECT rect{};
	  rect.bottom = size.y();
	  rect.right = size.x();
	  buffer->RSSetScissorRects(1, &rect);

      D3D12_CPU_DESCRIPTOR_HANDLE rtvs[8]{};
      unsigned maxSize = static_cast<unsigned>(std::min(8ull, packet.rtvs.size()));
      D3D12_CPU_DESCRIPTOR_HANDLE dsv;
      D3D12_CPU_DESCRIPTOR_HANDLE* dsvPtr = nullptr;

      if (packet.rtvs.size() > 0)
      {
        for (unsigned i = 0; i < maxSize; ++i)
        {
          rtvs[i].ptr = packet.rtvs[i].view().view;
        }
      }
      if (packet.dsvs.size() > 0)
      {
        dsv.ptr = packet.dsvs[0].view().view;
        dsvPtr = &dsv;
      }
      buffer->OMSetRenderTargets(maxSize, rtvs, false, dsvPtr);
    }

    void handle(ID3D12GraphicsCommandList* buffer, size_t hash, gfxpacket::GraphicsPipelineBind& pipelines)
    {
      for (auto&& it : *pipelines.pipeline.m_pipelines)
      {
        if (it.first == hash)
        {
          auto pipeline = std::static_pointer_cast<DX12Pipeline>(it.second);
          buffer->SetGraphicsRootSignature(pipeline->root.Get());
          buffer->SetPipelineState(pipeline->pipeline.Get());
		  buffer->IASetPrimitiveTopology(pipeline->primitive);
        }
      }
    }

    void handle(ID3D12GraphicsCommandList* buffer, gfxpacket::ComputePipelineBind& packet)
    {
      auto pipeline = std::static_pointer_cast<DX12Pipeline>(packet.pipeline.impl);
      buffer->SetComputeRootSignature(pipeline->root.Get());
      buffer->SetPipelineState(pipeline->pipeline.Get());
    }

    void handle(ID3D12GraphicsCommandList* buffer, gfxpacket::Draw& packet)
    {
      buffer->DrawInstanced(packet.vertexCountPerInstance, packet.instanceCount, packet.startVertex, packet.startInstance);
    }
    void handle(ID3D12GraphicsCommandList* buffer, gfxpacket::Dispatch& packet)
    {
      buffer->Dispatch(packet.groups.x(), packet.groups.y(), packet.groups.z());
    }

	UploadBlock DX12CommandList::allocateConstants(size_t size)
	{
		auto block = m_constantsAllocator.allocate(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		if (!block)
		{
			auto newBlock = m_constants->allocate(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * 16);
			F_ASSERT(newBlock, "What!");
			m_freeResources->uploadBlocks.push_back(newBlock);
			m_constantsAllocator = UploadLinearAllocator(newBlock);
			block = m_constantsAllocator.allocate(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}

		F_ASSERT(block, "What!");
		return block;
	}

    void DX12CommandList::handleBindings(ID3D12GraphicsCommandList* buffer, gfxpacket::ResourceBinding& ding)
    {
		if (ding.constants.size() > 0)
		{
			auto block = allocateConstants(ding.constants.size());
			memcpy(block.data(), ding.constants.data(), ding.constants.size());
			auto virtualAddress = m_constants->native()->GetGPUVirtualAddress();
			virtualAddress += block.block.offset;
			if (ding.graphicsBinding == gfxpacket::ResourceBinding::BindingType::Graphics)
			{
				buffer->SetGraphicsRootConstantBufferView(0, virtualAddress);
			}
			else
			{
				buffer->SetComputeRootConstantBufferView(0, virtualAddress);
			}
		}
    }

    void handle(ID3D12GraphicsCommandList* buffer, gfxpacket::ClearRT& packet)
    {
      auto view = std::static_pointer_cast<DX12TextureView>(packet.rtv.native());
      auto texture = std::static_pointer_cast<DX12Texture>(packet.rtv.texture().native());
      float rgba[4]{ packet.color.x(), packet.color.y(), packet.color.z(), packet.color.w() };
      buffer->ClearRenderTargetView(view->native().cpu, rgba, 0, nullptr);
    }

    void DX12CommandList::addCommands(ID3D12GraphicsCommandList* buffer, DX12DependencySolver* solver, backend::IntermediateList& list)
    {
      int drawIndex = 0;

      size_t currentActiveSubpassHash = 0;

      for (CommandPacket* packet : list)
      {
        switch (packet->type())
        {
          //        case CommandPacket::PacketType::BufferCopy:
        case CommandPacket::PacketType::Subpass:
        {
          currentActiveSubpassHash = (packetRef(gfxpacket::Subpass, packet)).hash;
          handle(buffer, packetRef(gfxpacket::Subpass, packet));
          break;
        }
        case CommandPacket::PacketType::ResourceBinding:
        {
          handleBindings(buffer, packetRef(gfxpacket::ResourceBinding, packet));
          break;
        }
        case CommandPacket::PacketType::Draw:
        {
          handle(buffer, packetRef(gfxpacket::Draw, packet));
          break;
        }
        case CommandPacket::PacketType::Dispatch:
        {
          //handle(buffer, packetRef(gfxpacket::Dispatch, packet));
          break;
        }
        case CommandPacket::PacketType::GraphicsPipelineBind:
        {
          if (currentActiveSubpassHash == 0)
            break;
          handle(buffer, currentActiveSubpassHash, packetRef(gfxpacket::GraphicsPipelineBind, packet));
          break;
        }
        case CommandPacket::PacketType::ComputePipelineBind:
        {
          handle(buffer, packetRef(gfxpacket::ComputePipelineBind, packet));
          break;
        }
        case CommandPacket::PacketType::ClearRT:
        {
          solver->runBarrier(buffer, drawIndex);
          handle(buffer, packetRef(gfxpacket::ClearRT, packet));
          drawIndex++;
          break;
        }
        case CommandPacket::PacketType::PrepareForPresent:
        {
          solver->runBarrier(buffer, drawIndex);
          drawIndex++;
          break;
        }
        default:
          break;
        }
      }
    }

    void DX12CommandList::addDepedencyDataAndSolve(DX12DependencySolver* solver, backend::IntermediateList& list)
    {
      int drawIndex = 0;

      auto addTextureView = [&](int index, TextureView& texView, D3D12_RESOURCE_STATES flags)
      {
        auto view = std::static_pointer_cast<DX12TextureView>(texView.native());
        auto texture = std::static_pointer_cast<DX12Texture>(texView.texture().native());
        solver->addTexture(index, texView.texture().id(), *texture, *view, static_cast<int16_t>(texView.texture().desc().desc.miplevels), flags);
      };

      auto addTexture = [&](int index, Texture& texture, D3D12_RESOURCE_STATES flags)
      {
        auto tex = std::static_pointer_cast<DX12Texture>(texture.native());
        SubresourceRange range{};
        range.mipOffset = 0;
        range.mipLevels = 1;
        range.sliceOffset = 0;
        range.arraySize = 1;
        solver->addTexture(index, texture.id(), *tex, static_cast<int16_t>(texture.desc().desc.miplevels), flags, range);
      };

      for (CommandPacket* packet : list)
      {
        switch (packet->type())
        {
          //        case CommandPacket::PacketType::BufferCopy:
          //        case CommandPacket::PacketType::Dispatch:
        case CommandPacket::PacketType::ClearRT:
        {
          auto& p = packetRef(gfxpacket::ClearRT, packet);
          drawIndex = solver->addDrawCall(packet->type());
          addTextureView(drawIndex, p.rtv, D3D12_RESOURCE_STATE_RENDER_TARGET);
          break;
        }
        case CommandPacket::PacketType::PrepareForPresent:
        {
          auto& p = packetRef(gfxpacket::PrepareForPresent, packet);
          drawIndex = solver->addDrawCall(packet->type());
          addTexture(drawIndex, p.texture, D3D12_RESOURCE_STATE_PRESENT);
          drawIndex++;
          break;
        }
        default:
          break;
        }
      }
      solver->makeAllBarriers();
    }

    void DX12CommandList::processRenderpasses(DX12Device* dev, backend::IntermediateList& list)
    {
      gfxpacket::Subpass* activeSubpass = nullptr;

      for (CommandPacket* packet : list)
      {
        switch (packet->type())
        {
        case CommandPacket::PacketType::Subpass:
        {
          activeSubpass = packetPtr(gfxpacket::Subpass, packet);
          break;
        }
        case CommandPacket::PacketType::ComputePipelineBind:
        {
          auto& ref = packetRef(gfxpacket::ComputePipelineBind, packet);
          dev->updatePipeline(ref.pipeline);
          break;
        }
        case CommandPacket::PacketType::GraphicsPipelineBind:
        {
          auto& ref = packetRef(gfxpacket::GraphicsPipelineBind, packet);
          dev->updatePipeline(ref.pipeline, *activeSubpass);
          break;
        }
        default:
          break;
        }
      }
    }

    // implementations
    void DX12CommandList::fillWith(std::shared_ptr<prototypes::DeviceImpl> device, backend::IntermediateList& list)
    {
      DX12DependencySolver solver;

      DX12Device* dev = static_cast<DX12Device*>(device.get());

      addDepedencyDataAndSolve(&solver, list);
      processRenderpasses(dev, list);
      addCommands(m_buffer->list(), &solver, list);

      m_buffer->closeList();
    }
  }
}