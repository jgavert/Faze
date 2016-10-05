#include "VulkanDescriptorSet.hpp"

void VulkanDescriptorSet::read(unsigned slot, VulkanBufferShaderView& buffer)
{
  buffers.push_back(std::make_pair(slot, buffer));
}
void VulkanDescriptorSet::read(unsigned slot, VulkanTextureShaderView& texture)
{
  textures.push_back(std::make_pair(slot, texture));
}
void VulkanDescriptorSet::modify(unsigned slot, VulkanBufferShaderView& buffer)
{
	buffers.push_back(std::make_pair(slot, buffer));
}
void VulkanDescriptorSet::modify(unsigned slot, VulkanTextureShaderView& texture)
{
	textures.push_back(std::make_pair(slot, texture));
}