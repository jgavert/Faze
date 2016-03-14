#include "CommandList.hpp"

ComputeCmdBuffer::ComputeCmdBuffer(FazCPtr<ID3D12GraphicsCommandList> cmdList, FazCPtr<ID3D12CommandAllocator> commandListAllocator)
  :m_CommandList(cmdList), m_CommandListAllocator(commandListAllocator), closed(false),
  m_boundCptPipeline(nullptr), m_boundGfxPipeline(nullptr)
  , m_uavBindlessIndex(-1), m_srvBindlessIndex(-1)
	, m_graphicsBound(false)
{
}

void ComputeCmdBuffer::setResourceBarrier()
{

}

void ComputeCmdBuffer::bindComputeBinding(ComputeBinding& asd)
{

	if (asd.m_resbars.size() > 0)
	{
		unsigned size = static_cast<unsigned>(asd.m_resbars.size());
		const D3D12_RESOURCE_BARRIER * barriers = asd.m_resbars.data();
		m_CommandList->ResourceBarrier(size, barriers);
		asd.m_resbars.clear(); // don't if binding many times, don't accidentally add tons of useless resource barriers.
	}
	{
		if (asd.m_descTableSRV.second != -1)
		{
			m_CommandList->SetComputeRootDescriptorTable(asd.m_descTableSRV.second, asd.m_descTableSRV.first);
		}
		if (asd.m_descTableUAV.second != -1)
		{
			m_CommandList->SetComputeRootDescriptorTable(asd.m_descTableUAV.second, asd.m_descTableUAV.first);
		}
	}
	for (size_t i = 0; i < asd.m_cbvs.size(); ++i)
	{
		if (asd.m_cbvs[i].first.ptr != 0)
			m_CommandList->SetComputeRootConstantBufferView(asd.m_cbvs[i].second, asd.m_cbvs[i].first.ptr);
	}
	for (size_t i = 0; i < asd.m_srvs.size(); ++i)
	{
		if (asd.m_srvs[i].first.ptr != 0)
			m_CommandList->SetComputeRootShaderResourceView(asd.m_srvs[i].second, asd.m_srvs[i].first.ptr);
	}
	for (size_t i = 0; i < asd.m_uavs.size(); ++i)
	{
		if (asd.m_uavs[i].first.ptr != 0)
			m_CommandList->SetComputeRootUnorderedAccessView(asd.m_uavs[i].second, asd.m_uavs[i].first.ptr);
	}
	for (size_t i = 0; i < asd.m_rootConstants.size(); ++i)
	{
		m_CommandList->SetComputeRoot32BitConstant(asd.m_rootConstants[i].second, asd.m_rootConstants[i].first, 0);
	}
}

void ComputeCmdBuffer::CopyResource(Buffer& dstdata, Buffer& srcdata)
{
  D3D12_RESOURCE_BARRIER bD[2];
  size_t count = 0;
  auto& dstbuf = dstdata.getBuffer();
  auto& srcbuf = srcdata.getBuffer();
  if (!dstbuf.m_immutableState && dstbuf.m_state != D3D12_RESOURCE_STATE_COPY_DEST)
  {
    bD[count] = {};
    bD[count].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    bD[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bD[count].Transition.pResource = dstbuf.m_resource.get();
    bD[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    bD[count].Transition.StateBefore = dstbuf.m_state;
    dstbuf.m_state = D3D12_RESOURCE_STATE_COPY_DEST;
    bD[count].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    ++count;
  }
  if (!srcbuf.m_immutableState && srcbuf.m_state != D3D12_RESOURCE_STATE_GENERIC_READ)
  {
    bD[count] = {};
    bD[count].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    bD[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bD[count].Transition.pResource = srcbuf.m_resource.get();
    bD[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    bD[count].Transition.StateBefore = srcbuf.m_state;
    srcbuf.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
    bD[count].Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    ++count;
  }
  if (count > 0)
  {
    m_CommandList->ResourceBarrier(static_cast<unsigned>(count), bD);
  }
  m_CommandList->CopyResource(dstbuf.m_resource.get(), srcbuf.m_resource.get());
}

void ComputeCmdBuffer::CopyResource(Texture& dstdata, Texture& srcdata)
{
	D3D12_RESOURCE_BARRIER bD[2];
	size_t count = 0;
	auto& dstbuf = dstdata.getTexture();
	auto& srcbuf = srcdata.getTexture();
	if (!dstbuf.m_immutableState && dstbuf.m_state != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		bD[count] = {};
		bD[count].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		bD[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		bD[count].Transition.pResource = dstbuf.m_resource->get();
		bD[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		bD[count].Transition.StateBefore = dstbuf.m_state;
		dstbuf.m_state = D3D12_RESOURCE_STATE_COPY_DEST;
		bD[count].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		++count;
	}
	if (!srcbuf.m_immutableState && srcbuf.m_state != D3D12_RESOURCE_STATE_GENERIC_READ)
	{
		bD[count] = {};
		bD[count].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		bD[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		bD[count].Transition.pResource = srcbuf.m_resource->get();
		bD[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		bD[count].Transition.StateBefore = srcbuf.m_state;
		srcbuf.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
		bD[count].Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
		++count;
	}
	if (count > 0)
	{
		m_CommandList->ResourceBarrier(static_cast<unsigned>(count), bD);
	}
	m_CommandList->CopyResource(dstbuf.m_resource->get(), srcbuf.m_resource->get());
}

void ComputeCmdBuffer::Dispatch(ComputeBinding& asd, unsigned int x, unsigned int y, unsigned int z)
{
  bindComputeBinding(asd);

  m_CommandList->Dispatch(x, y, z);
}

void ComputeCmdBuffer::DispatchIndirect(ComputeBinding& bind)
{
  bindComputeBinding(bind);

}

ComputeBinding ComputeCmdBuffer::bind(ComputePipeline& pipeline)
{
  auto& inf = pipeline.getShaderInterface();
  if (m_boundShaderInterface != inf || m_graphicsBound)
  {
    m_boundShaderInterface = inf;
    m_CommandList->SetComputeRootSignature(inf.m_rootSig.get());
	m_graphicsBound = false;
  }
  if (m_boundCptPipeline != &pipeline)
  {
    m_boundCptPipeline = &pipeline;
    m_CommandList->SetPipelineState(pipeline.getState());
    m_boundGfxPipeline = nullptr;
  }
  {
	m_CommandList->SetDescriptorHeaps(1, pipeline.getDescHeap().m_descHeap.addr());
  }
  return pipeline.getBinding();
}

bool ComputeCmdBuffer::isValid()
{
  return m_CommandList.get() != nullptr;
}

void ComputeCmdBuffer::setHeaps(DescriptorHeapManager& heaps)
{
	m_CommandList->SetDescriptorHeaps(heaps.getCount(), heaps.getHeapPtr());
}

void ComputeCmdBuffer::setSRVBindless(DescriptorHeapManager& srvDescHeap)
{
	if (m_srvBindlessIndex < 0)
		return;
	auto srvGpuStart = srvDescHeap.getGeneric().m_descHeap->GetGPUDescriptorHandleForHeapStart();
	srvGpuStart.ptr += srvDescHeap.getGeneric().m_handleIncrementSize * 128;
	m_CommandList->SetComputeRootDescriptorTable(m_srvBindlessIndex, srvGpuStart);
}
void ComputeCmdBuffer::setUAVBindless(DescriptorHeapManager& uavDescHeap)
{
	if (m_uavBindlessIndex < 0)
		return;
	auto uavGpuStart = uavDescHeap.getGeneric().m_descHeap->GetGPUDescriptorHandleForHeapStart();
	uavGpuStart.ptr += uavDescHeap.getGeneric().m_handleIncrementSize * 128 * 2;
	m_CommandList->SetComputeRootDescriptorTable(m_uavBindlessIndex, uavGpuStart);
}
void ComputeCmdBuffer::closeList()
{
	HRESULT hr;
	hr = m_CommandList->Close();
	if (FAILED(hr))
	{
    F_ERROR("CommandList failed to close.");
	}
	closed = true;
}
bool ComputeCmdBuffer::isClosed()
{
	return closed;
}

void ComputeCmdBuffer::resetList()
{
	m_CommandListAllocator->Reset();
	if (m_boundCptPipeline != nullptr)
		m_CommandList->Reset(m_CommandListAllocator.get(), m_boundCptPipeline->getState());
	else
		m_CommandList->Reset(m_CommandListAllocator.get(), nullptr);
	closed = false;
	if (m_boundShaderInterface.valid())
		m_CommandList->SetComputeRootSignature(m_boundShaderInterface.m_rootSig.get());
}


GraphicsBinding GraphicsCmdBuffer::bind(GraphicsPipeline& pipeline)
{
  auto& inf = pipeline.getShaderInterface();
  if (m_boundShaderInterface != inf || !m_graphicsBound)
  {
    m_boundShaderInterface = inf;
    m_CommandList->SetGraphicsRootSignature(inf.m_rootSig.get());
	m_graphicsBound = true;
  }
  if (m_boundGfxPipeline != &pipeline)
  {
    m_boundGfxPipeline = &pipeline;
    m_CommandList->SetPipelineState(pipeline.getState());
    m_boundCptPipeline = nullptr;
  }
  {
	  m_CommandList->SetDescriptorHeaps(1, pipeline.getDescHeap().m_descHeap.addr());
  }
  return pipeline.getBinding();
}

ComputeBinding GraphicsCmdBuffer::bind(ComputePipeline& pipeline)
{
	return ComputeCmdBuffer::bind(pipeline);
}

void GraphicsCmdBuffer::setViewPort(ViewPort& view)
{
  m_CommandList->RSSetViewports(1, &view.getDesc());
  RECT rect;
  rect.bottom = 600;
  rect.top = 0;
  rect.left = 0;
  rect.right = 800;
  m_CommandList->RSSetScissorRects(1, &rect);
}

void GraphicsCmdBuffer::ClearRenderTargetView(TextureRTV& rtv, faze::vec4 color)
{
  m_CommandList->ClearRenderTargetView(rtv.cpuHandle, color.data.data(), NULL, 0);
}


void GraphicsCmdBuffer::ClearDepthView(TextureDSV& dsv)
{
  m_CommandList->ClearDepthStencilView(dsv.cpuHandle, D3D12_CLEAR_FLAG_DEPTH , 1.f, 0, 0, nullptr);
}

void GraphicsCmdBuffer::bindGraphicsBinding(GraphicsBinding& asd)
{
	if (asd.m_resbars.size() > 0)
	{
		unsigned size = static_cast<unsigned>(asd.m_resbars.size());
		const D3D12_RESOURCE_BARRIER * barriers = asd.m_resbars.data();
		m_CommandList->ResourceBarrier(size, barriers);
		asd.m_resbars.clear(); // don't if binding many times, don't accidentally add tons of useless resource barriers.
	}
	{
		if (asd.m_descTableSRV.second != -1)
		{
			m_CommandList->SetGraphicsRootDescriptorTable(asd.m_descTableSRV.second, asd.m_descTableSRV.first);
		}
		if (asd.m_descTableUAV.second != -1)
		{
			m_CommandList->SetGraphicsRootDescriptorTable(asd.m_descTableUAV.second, asd.m_descTableUAV.first);
		}
	}
	for (size_t i = 0; i < asd.m_cbvs.size(); ++i)
	{
		if (asd.m_cbvs[i].first.ptr != 0)
			m_CommandList->SetGraphicsRootConstantBufferView(asd.m_cbvs[i].second, asd.m_cbvs[i].first.ptr);
	}
	for (size_t i = 0; i < asd.m_srvs.size(); ++i)
	{
		if (asd.m_srvs[i].first.ptr != 0)
			m_CommandList->SetGraphicsRootShaderResourceView(asd.m_srvs[i].second, asd.m_srvs[i].first.ptr);
	}
	for (size_t i = 0; i < asd.m_uavs.size(); ++i)
	{
		if (asd.m_uavs[i].first.ptr != 0)
			m_CommandList->SetGraphicsRootUnorderedAccessView(asd.m_uavs[i].second, asd.m_uavs[i].first.ptr);
	}
	for (size_t i = 0; i < asd.m_rootConstants.size(); ++i)
	{
		m_CommandList->SetGraphicsRoot32BitConstant(asd.m_rootConstants[i].second, asd.m_rootConstants[i].first, 0);
	}
}

void GraphicsCmdBuffer::ClearStencilView(TextureDSV& dsv)
{
  m_CommandList->ClearDepthStencilView(dsv.cpuHandle, D3D12_CLEAR_FLAG_STENCIL, 0.f, 0xFF, 0, nullptr);
}

void GraphicsCmdBuffer::ClearDepthStencilView(TextureDSV& dsv)
{
  m_CommandList->ClearDepthStencilView(dsv.cpuHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0xFF, 0, nullptr);
}

void GraphicsCmdBuffer::drawInstanced(GraphicsBinding& bind, unsigned int vertexCountPerInstance, unsigned int instanceCount, unsigned int startVertexId, unsigned int startInstanceId)
{
	bindGraphicsBinding(bind);
	m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_CommandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexId, startInstanceId);
}

void GraphicsCmdBuffer::drawInstancedRaw(unsigned int vertexCountPerInstance, unsigned int instanceCount, unsigned int startVertexId, unsigned int startInstanceId)
{
	m_CommandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexId, startInstanceId);
}

void GraphicsCmdBuffer::preparePresent(TextureRTV& rtv)
{
	if (rtv.texture().m_state != D3D12_RESOURCE_STATE_PRESENT)
	{
		D3D12_RESOURCE_BARRIER desc = {};
		desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		desc.Transition.pResource = rtv.texture().m_resource->get();
		desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		desc.Transition.StateBefore = rtv.texture().m_state;
		rtv.texture().m_state = D3D12_RESOURCE_STATE_PRESENT;
		desc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		m_CommandList->ResourceBarrier(1, &desc);
	}
}

void GraphicsCmdBuffer::setRenderTarget(TextureRTV& rtv)
{
	if (rtv.texture().m_state != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		D3D12_RESOURCE_BARRIER desc = {};
		desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		desc.Transition.pResource = rtv.texture().m_resource->get();
		desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		desc.Transition.StateBefore = rtv.texture().m_state;
		rtv.texture().m_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
		desc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		m_CommandList->ResourceBarrier(1, &desc);
	}
	m_CommandList->OMSetRenderTargets(1, &rtv.cpuHandle, false, nullptr);
}

void GraphicsCmdBuffer::setRenderTarget(TextureRTV& rtv, TextureDSV& dsv)
{
	D3D12_RESOURCE_BARRIER desc[2];
	int count = 0;
	if (rtv.texture().m_state != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		desc[count] = {};
		desc[count].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		desc[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		desc[count].Transition.pResource = rtv.texture().m_resource->get(); // uh oh
		desc[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		desc[count].Transition.StateBefore = rtv.texture().m_state;
		rtv.texture().m_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
		desc[count].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		++count;
	}
	if (dsv.texture().m_state != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		desc[count] = {};
		desc[count].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		desc[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		desc[count].Transition.pResource = dsv.texture().m_resource->get();
		desc[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		desc[count].Transition.StateBefore = dsv.texture().m_state;
		dsv.texture().m_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		desc[count].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}
	if (count != 0)
		m_CommandList->ResourceBarrier(count, desc);
	m_CommandList->OMSetRenderTargets(1, &rtv.cpuHandle, false, &dsv.cpuHandle);
}

void GraphicsCmdBuffer::setSRVBindless(DescriptorHeapManager& srvDescHeap)
{
	if (m_srvBindlessIndex < 0)
		return;
	auto srvGpuStart = srvDescHeap.getGeneric().m_descHeap->GetGPUDescriptorHandleForHeapStart();
	srvGpuStart.ptr += srvDescHeap.getGeneric().m_handleIncrementSize * 128;
	m_CommandList->SetGraphicsRootDescriptorTable(m_srvBindlessIndex, srvGpuStart);
}
void GraphicsCmdBuffer::setUAVBindless(DescriptorHeapManager& uavDescHeap)
{
	if (m_uavBindlessIndex < 0)
		return;
	auto uavGpuStart = uavDescHeap.getGeneric().m_descHeap->GetGPUDescriptorHandleForHeapStart();
	uavGpuStart.ptr += uavDescHeap.getGeneric().m_handleIncrementSize * 128 * 2;
	m_CommandList->SetGraphicsRootDescriptorTable(m_uavBindlessIndex, uavGpuStart);
}

void GraphicsCmdBuffer::resetList()
{
	m_CommandListAllocator->Reset();
	m_CommandList->Reset(m_CommandListAllocator.get(), nullptr);
	closed = false;
	m_boundCptPipeline = nullptr;
	m_boundGfxPipeline = nullptr;
	m_boundShaderInterface = ShaderInterface();
}
