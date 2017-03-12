#include "CommandList.hpp"


bool GraphicsCmdBuffer::isValid()
{
  return m_cmdBuffer->isValid();
}

void GraphicsCmdBuffer::close()
{
  m_cmdBuffer->close();
}

bool GraphicsCmdBuffer::isClosed()
{
  return m_cmdBuffer->isClosed();
}

Fence GraphicsCmdBuffer::fence()
{
  return Fence(m_seqNum);
}

// Pipeline
void GraphicsCmdBuffer::bindPipeline(ComputePipeline& pipeline)
{
  m_cmdBuffer->bindComputePipeline(pipeline.m_pipeline);
}
// compute

void GraphicsCmdBuffer::dispatch(DescriptorSet& inputs, unsigned x, unsigned y, unsigned z)
{
  m_cmdBuffer->dispatch(inputs.set, x, y, z);
  inputs.set.clear(); // TODO: .. annoying
}

void GraphicsCmdBuffer::dispatchThreads(DescriptorSet& inputs, unsigned x, unsigned y, unsigned z)
{
  const auto roundUpMultiple = [](int value, int multiple)
  {
    return (value + multiple - 1) / multiple;
  };
  auto xThreads = roundUpMultiple(x, inputs.workGroupX);
  auto yThreads = roundUpMultiple(y, inputs.workGroupY);
  auto zThreads = roundUpMultiple(z, inputs.workGroupZ);
  m_cmdBuffer->dispatch(inputs.set, xThreads, yThreads, zThreads);
  inputs.set.clear(); // TODO: .. annoying
}

// draw

LiveRenderpass GraphicsCmdBuffer::renderpass(Renderpass& rp, TextureRTV& rtv)
{
  m_cmdBuffer->beginRenderpass(rp.impl(), rtv.view());

  return LiveRenderpass(m_cmdBuffer);
}

void GraphicsCmdBuffer::beginSubpass()
{
  m_cmdBuffer->beginSubpass();
}

void GraphicsCmdBuffer::endSubpass()
{
  m_cmdBuffer->endSubpass();
}

// copy

void GraphicsCmdBuffer::copy(Buffer& srcdata, Buffer& dstdata)
{
  m_cmdBuffer->copy(srcdata.getBuffer(), dstdata.getBuffer());
}

void GraphicsCmdBuffer::clearRTV(TextureRTV& texture, float r, float g, float b, float a)
{
  m_cmdBuffer->clearRTV(texture.texture().impl(), r, g, b, a);
}

void GraphicsCmdBuffer::prepareForPresent(Texture& texture)
{
	m_cmdBuffer->prepareForPresent(texture.impl());
}

void GraphicsCmdBuffer::prepareForSubmit(GpuDeviceImpl& device)
{
  m_cmdBuffer->prepareForSubmit(device, m_pool.impl());
}