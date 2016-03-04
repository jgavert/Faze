#include "GpuDevice.hpp"

GpuDevice::GpuDevice(void* device, bool debugLayer) : m_device(device), m_debugLayer(debugLayer)
{
}

GpuDevice::~GpuDevice()
{
}


// Needs to be created from descriptor
GpuFence GpuDevice::createFence()
{
  return GpuFence();
}

GraphicsQueue GpuDevice::createQueue()
{
  return GraphicsQueue(nullptr);
}

// Needs to be created from descriptor
GraphicsCmdBuffer GpuDevice::createUniversalCommandList()
{
  return GraphicsCmdBuffer(nullptr);
}

ComputePipeline GpuDevice::createComputePipeline(ComputePipelineDescriptor )
{
	return ComputePipeline();
}

GraphicsPipeline GpuDevice::createGraphicsPipeline(GraphicsPipelineDescriptor )
{
	return GraphicsPipeline();
}

Buffer GpuDevice::createBuffer(ResourceDescriptor resDesc)
{
  Buffer buf;
  BufferInternal inter;
  inter.m_resource = nullptr;
  buf.buffer = std::make_shared<BufferInternal>(inter);
  buf.getBuffer().m_desc = resDesc;
  return buf;
}

Texture GpuDevice::createTexture(ResourceDescriptor resDesc)
{
  Texture tex;
  TextureInternal inter;
  inter.m_resource = nullptr;
  tex.texture = std::make_shared<TextureInternal>(inter);
  tex.getTexture().m_desc = resDesc;
  return tex;
}

SwapChain GpuDevice::createSwapChain(GraphicsQueue&, Window&)
{
  return SwapChain();
}

SwapChain GpuDevice::createSwapChain(GraphicsQueue&, Window&, int, FormatType)
{
  return SwapChain();
}

TextureSRV GpuDevice::createTextureSRV(Texture targetTexture, ShaderViewDescriptor)
{
  TextureSRV srv;
  srv.m_texture = targetTexture;
  return srv;
}
TextureUAV GpuDevice::createTextureUAV(Texture targetTexture, ShaderViewDescriptor)
{
  TextureUAV uav;
  uav.m_texture = targetTexture;
  return uav;
}
TextureRTV GpuDevice::createTextureRTV(Texture targetTexture, ShaderViewDescriptor)
{
  TextureRTV rtv;
  rtv.m_texture = targetTexture;
  return rtv;
}
TextureDSV GpuDevice::createTextureDSV(Texture targetTexture, ShaderViewDescriptor)
{
  TextureDSV dsv;
  dsv.m_texture = targetTexture;
  return dsv;
}

BufferSRV GpuDevice::createBufferSRV(Buffer targetBuffer, ShaderViewDescriptor)
{
  BufferSRV srv;
  srv.m_buffer = targetBuffer;
  return srv;
}
BufferUAV GpuDevice::createBufferUAV(Buffer targetBuffer, ShaderViewDescriptor)
{
  BufferUAV uav;
  uav.m_buffer = targetBuffer;
  return uav;
}
BufferCBV GpuDevice::createBufferCBV(Buffer targetBuffer, ShaderViewDescriptor)
{
  BufferCBV dsv;
  dsv.m_buffer = targetBuffer;
  return dsv;
}
BufferIBV GpuDevice::createBufferIBV(Buffer targetBuffer, ShaderViewDescriptor)
{
  BufferIBV dsv;
  dsv.m_buffer = targetBuffer;
  return dsv;
}
