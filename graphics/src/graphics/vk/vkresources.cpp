#include "graphics/vk/vkresources.hpp"
#include "graphics/vk/util/formats.hpp"
#include "graphics/common/graphicssurface.hpp"
#include "graphics/vk/util/pipeline_helpers.hpp"
#include "core/system/bitpacking.hpp"
#include "core/global_debug.hpp"

#include <optional>

namespace faze
{
  namespace backend
  {
    int32_t FindProperties(vk::PhysicalDeviceMemoryProperties memprop, uint32_t memoryTypeBits, vk::MemoryPropertyFlags properties)
    {
      for (int32_t i = 0; i < static_cast<int32_t>(memprop.memoryTypeCount); ++i)
      {
        if ((memoryTypeBits & (1 << i)) &&
          ((memprop.memoryTypes[i].propertyFlags & properties) == properties))
          return i;
      }
      return -1;
    }

    vk::BufferCreateInfo fillBufferInfo(ResourceDescriptor descriptor)
    {
      auto desc = descriptor.desc;
      auto bufSize = desc.stride*desc.width;
      F_ASSERT(bufSize != 0, "Cannot create zero sized buffers.");
      vk::BufferCreateInfo info = vk::BufferCreateInfo()
        .setSharingMode(vk::SharingMode::eExclusive);

      vk::BufferUsageFlags usageBits;

      // testing multiple flags
      usageBits = vk::BufferUsageFlagBits::eUniformTexelBuffer;
      usageBits |= vk::BufferUsageFlagBits::eStorageBuffer;

      if (desc.usage == ResourceUsage::GpuRW)
      {
        usageBits |= vk::BufferUsageFlagBits::eStorageTexelBuffer;
      }
      if (desc.indirect)
      {
        usageBits |= vk::BufferUsageFlagBits::eIndirectBuffer;
      }

      if (desc.index)
      {
        usageBits |= vk::BufferUsageFlagBits::eIndexBuffer;
      }

      auto usage = desc.usage;
      if (usage == ResourceUsage::Readback)
      {
        usageBits = usageBits | vk::BufferUsageFlagBits::eTransferDst;
      }
      else if (usage == ResourceUsage::Upload)
      {
        usageBits = usageBits | vk::BufferUsageFlagBits::eTransferSrc;
      }
      else
      {
        usageBits = usageBits | vk::BufferUsageFlagBits::eTransferSrc;
        usageBits = usageBits | vk::BufferUsageFlagBits::eTransferDst;
      }
      info = info.setUsage(usageBits);
      info = info.setSize(bufSize);
      return info;
    }

    vk::ImageCreateInfo fillImageInfo(ResourceDescriptor descriptor)
    {
      auto desc = descriptor.desc;

      vk::ImageType ivt;
      vk::ImageCreateFlags flags;
      flags |= vk::ImageCreateFlagBits::eMutableFormat; //???
      switch (desc.dimension)
      {
      case FormatDimension::Texture1D:
        ivt = vk::ImageType::e1D;
        break;
      case FormatDimension::Texture2D:
        ivt = vk::ImageType::e2D;
        break;
      case FormatDimension::Texture3D:
        ivt = vk::ImageType::e3D;
        break;
      case FormatDimension::TextureCube:
        ivt = vk::ImageType::e2D;
        flags |= vk::ImageCreateFlagBits::eCubeCompatible;
        break;
      default:
        ivt = vk::ImageType::e2D;
        break;
      }

      int mipLevels = desc.miplevels;

      vk::ImageUsageFlags usage;
      usage |= vk::ImageUsageFlagBits::eTransferDst;
      usage |= vk::ImageUsageFlagBits::eTransferSrc;
      usage |= vk::ImageUsageFlagBits::eSampled;

      switch (desc.usage)
      {
      case ResourceUsage::GpuReadOnly:
      {
        break;
      }
      case ResourceUsage::GpuRW:
      {
        usage |= vk::ImageUsageFlagBits::eStorage;
        break;
      }
      case ResourceUsage::RenderTarget:
      {
        usage |= vk::ImageUsageFlagBits::eColorAttachment;
        break;
      }
      case ResourceUsage::RenderTargetRW:
      {
        usage |= vk::ImageUsageFlagBits::eColorAttachment;
        usage |= vk::ImageUsageFlagBits::eStorage;
        break;
      }
      case ResourceUsage::DepthStencil:
      {
        usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
        F_ASSERT(mipLevels == 1, "DepthStencil doesn't support mips");
        break;
      }
      case ResourceUsage::DepthStencilRW:
      {
        usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
        usage |= vk::ImageUsageFlagBits::eStorage;
        F_ASSERT(mipLevels == 1, "DepthStencil doesn't support mips");
        break;
      }
      case ResourceUsage::Upload:
      {
        usage = vk::ImageUsageFlagBits::eTransferSrc;
        break;
      }
      case ResourceUsage::Readback:
      {
        usage = vk::ImageUsageFlagBits::eTransferDst;
        break;
      }
      default:
        break;
      }

      vk::SampleCountFlagBits sampleFlags = vk::SampleCountFlagBits::e1;

      if (desc.msCount > 32)
      {
        sampleFlags = vk::SampleCountFlagBits::e64;
      }
      else if (desc.msCount > 16)
      {
        sampleFlags = vk::SampleCountFlagBits::e32;
      }
      else if (desc.msCount > 8)
      {
        sampleFlags = vk::SampleCountFlagBits::e16;
      }
      else if (desc.msCount > 4)
      {
        sampleFlags = vk::SampleCountFlagBits::e8;
      }
      else if (desc.msCount > 2)
      {
        sampleFlags = vk::SampleCountFlagBits::e4;
      }
      else if (desc.msCount > 1)
      {
        sampleFlags = vk::SampleCountFlagBits::e2;
      }

      vk::ImageCreateInfo info = vk::ImageCreateInfo()
        .setArrayLayers(desc.arraySize)
        .setExtent(vk::Extent3D()
          .setWidth(desc.width)
          .setHeight(desc.height)
          .setDepth(desc.depth))
        .setFlags(flags)
        .setFormat(formatToVkFormat(desc.format).storage)
        .setImageType(ivt)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setMipLevels(desc.miplevels)
        .setSamples(sampleFlags)
        .setTiling(vk::ImageTiling::eOptimal) // TODO: hmm
        .setUsage(usage)
        .setSharingMode(vk::SharingMode::eExclusive); // TODO: hmm

      if (desc.dimension == FormatDimension::TextureCube)
      {
        info = info.setArrayLayers(desc.arraySize * 6);
      }

      return info;
    }

    MemoryPropertySearch getMemoryProperties(ResourceUsage usage)
    {
      MemoryPropertySearch ret{};
      switch (usage)
      {
      case ResourceUsage::Upload:
      {
        ret.optimal = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        ret.def = vk::MemoryPropertyFlagBits::eHostVisible;
        break;
      }
      case ResourceUsage::Readback:
      {
        ret.optimal = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;
        ret.def = vk::MemoryPropertyFlagBits::eHostVisible;
        break;
      }
      case ResourceUsage::GpuReadOnly:
      case ResourceUsage::GpuRW:
      case ResourceUsage::RenderTarget:
      case ResourceUsage::DepthStencil:
      case ResourceUsage::RenderTargetRW:
      case ResourceUsage::DepthStencilRW:
      default:
      {
        ret.optimal = vk::MemoryPropertyFlagBits::eDeviceLocal;
        ret.def = vk::MemoryPropertyFlagBits::eDeviceLocal;
        break;
      }
      }
      return ret;
    }

    void printMemoryTypeInfos(vk::PhysicalDeviceMemoryProperties prop)
    {
      auto checkFlagSet = [](vk::MemoryType& type, vk::MemoryPropertyFlagBits flag)
      {
        return (type.propertyFlags & flag) == flag;
      };
      F_ILOG("Graphics/Memory", "heapCount %u memTypeCount %u", prop.memoryHeapCount, prop.memoryTypeCount);
      for (int i = 0; i < static_cast<int>(prop.memoryTypeCount); ++i)
      {
        auto memType = prop.memoryTypes[i];
        F_ILOG("Graphics/Memory", "propertyFlags %u", memType.propertyFlags.operator unsigned int());
        if (checkFlagSet(memType, vk::MemoryPropertyFlagBits::eDeviceLocal))
        {
          F_ILOG("Graphics/Memory", "heap %u type %u was eDeviceLocal", memType.heapIndex, i);
          if (checkFlagSet(memType, vk::MemoryPropertyFlagBits::eHostVisible))
          {
            F_ILOG("Graphics/Memory", "heap %u type %u was eHostVisible", memType.heapIndex, i);
            if (checkFlagSet(memType, vk::MemoryPropertyFlagBits::eHostCoherent))
              F_ILOG("Graphics/Memory", "heap %u type %u was eHostCoherent", memType.heapIndex, i);
            if (checkFlagSet(memType, vk::MemoryPropertyFlagBits::eHostCached))
              F_ILOG("Graphics/Memory", "heap %u type %u was eHostCached", memType.heapIndex, i);
          }
        }
        else if (checkFlagSet(memType, vk::MemoryPropertyFlagBits::eHostVisible))
        {
          F_ILOG("Graphics/Memory", "heap %u type %u was eHostVisible", memType.heapIndex, i);
          if (checkFlagSet(memType, vk::MemoryPropertyFlagBits::eHostCoherent))
            F_ILOG("Graphics/Memory", "heap %u type %u was eHostCoherent", memType.heapIndex, i);
          if (checkFlagSet(memType, vk::MemoryPropertyFlagBits::eHostCached))
          {
            F_ILOG("Graphics/Memory", "heap %u type %u was eHostCached", memType.heapIndex, i);
          }
        }
      }
    }

    VulkanDevice::VulkanDevice(
      vk::Device device,
      vk::PhysicalDevice physDev,
      FileSystem& fs,
      std::vector<vk::QueueFamilyProperties> queues,
      GpuInfo info,
      bool debugLayer)
      : m_device(device)
      , m_physDevice(physDev)
      , m_debugLayer(debugLayer)
      , m_queues(queues)
      , m_singleQueue(false)
      , m_computeQueues(false)
      , m_dmaQueues(false)
      , m_graphicQueues(false)
      , m_info(info)
      , m_shaders(fs, std::shared_ptr<ShaderCompiler>(new DXCompiler(fs, "/shaders/")), "shaders", "shaders/bin", ShaderBinaryType::SPIRV)
      , m_freeQueueIndexes({})
      , m_seqTracker(std::make_shared<SequenceTracker>())
      , m_dynamicUpload(std::make_shared<VulkanUploadHeap>(device, physDev, 256 * 256, 1024 * 4)) // TODO: implement dynamically adjusted
      , m_trash(std::make_shared<Garbage>())
    {
      // try to figure out unique queues, abort or something when finding unsupported count.
      // universal
      // graphics+compute
      // compute
      // dma
      size_t totalQueues = 0;
      for (int k = 0; k < static_cast<int>(m_queues.size()); ++k)
      {
        constexpr auto GfxQ = static_cast<uint32_t>(VK_QUEUE_GRAPHICS_BIT);
        constexpr auto CpQ = static_cast<uint32_t>(VK_QUEUE_COMPUTE_BIT);
        constexpr auto DMAQ = static_cast<uint32_t>(VK_QUEUE_TRANSFER_BIT);
        auto&& it = m_queues[k];
        auto current = static_cast<uint32_t>(it.queueFlags);
        if ((current & (GfxQ | CpQ | DMAQ)) == GfxQ + CpQ + DMAQ)
        {
          for (uint32_t i = 0; i < it.queueCount; ++i)
          {
            m_freeQueueIndexes.universal.push_back(i);
          }
          m_freeQueueIndexes.universalIndex = k;
        }
        else if ((current & (GfxQ | CpQ)) == GfxQ + CpQ)
        {
          for (uint32_t i = 0; i < it.queueCount; ++i)
          {
            m_freeQueueIndexes.graphics.push_back(i);
          }
          m_freeQueueIndexes.graphicsIndex = k;
        }
        else if ((current & CpQ) == CpQ)
        {
          for (uint32_t i = 0; i < it.queueCount; ++i)
          {
            m_freeQueueIndexes.compute.push_back(i);
          }
          m_freeQueueIndexes.computeIndex = k;
        }
        else if ((current & DMAQ) == DMAQ)
        {
          for (uint32_t i = 0; i < it.queueCount; ++i)
          {
            m_freeQueueIndexes.dma.push_back(i);
          }
          m_freeQueueIndexes.dmaIndex = k;
        }
        totalQueues += it.queueCount;
      }
      if (totalQueues == 0)
      {
        F_ERROR("wtf, not sane device.");
      }
      else if (totalQueues == 1)
      {
        // single queue =_=, IIIINNNTTTEEEELLL
        m_singleQueue = true;
        // lets just fetch it and copy it for those who need it.
        //auto que = m_device->getQueue(0, 0); // TODO: 0 index is wrong.
        //m_internalUniversalQueue = m_device.getQueue(0, 0);
      }
      if (m_freeQueueIndexes.universal.size() > 0
        && m_freeQueueIndexes.graphics.size() > 0)
      {
        F_ERROR("abort mission. Too many variations of queues.");
      }

      m_computeQueues = !m_freeQueueIndexes.compute.empty();
      m_dmaQueues = !m_freeQueueIndexes.dma.empty();
      m_graphicQueues = !m_freeQueueIndexes.graphics.empty();

      // find graphics, compute, copy queues
      if (m_singleQueue)
      {
        m_mainQueue = m_device.getQueue(0, 0);
        m_mainQueueIndex = 0;
      }
      else
      {
        if (!m_freeQueueIndexes.graphics.empty())
        {
          uint32_t queueFamilyIndex = m_freeQueueIndexes.graphicsIndex;
          uint32_t queueId = m_freeQueueIndexes.graphics.back();
          m_freeQueueIndexes.graphics.pop_back();
          m_mainQueue = m_device.getQueue(queueFamilyIndex, queueId);
          m_mainQueueIndex = queueFamilyIndex;
        }
        else if (!m_freeQueueIndexes.universal.empty())
        {
          uint32_t queueFamilyIndex = m_freeQueueIndexes.universalIndex;
          uint32_t queueId = m_freeQueueIndexes.universal.back();
          m_freeQueueIndexes.universal.pop_back();
          m_mainQueue = m_device.getQueue(queueFamilyIndex, queueId);
          m_mainQueueIndex = queueFamilyIndex;
        }
        // compute
        if (!m_freeQueueIndexes.compute.empty())
        {
          uint32_t queueFamilyIndex = m_freeQueueIndexes.computeIndex;
          uint32_t queueId = m_freeQueueIndexes.compute.back();
          m_freeQueueIndexes.compute.pop_back();
          m_computeQueue = m_device.getQueue(queueFamilyIndex, queueId);
          m_computeQueueIndex = queueFamilyIndex;
        }
        else if (!m_freeQueueIndexes.universal.empty())
        {
          uint32_t queueFamilyIndex = m_freeQueueIndexes.universalIndex;
          uint32_t queueId = m_freeQueueIndexes.universal.back();
          m_freeQueueIndexes.universal.pop_back();
          m_computeQueue = m_device.getQueue(queueFamilyIndex, queueId);
          m_computeQueueIndex = queueFamilyIndex;
        }

        // copy
        if (!m_freeQueueIndexes.dma.empty())
        {
          uint32_t queueFamilyIndex = m_freeQueueIndexes.dmaIndex;
          uint32_t queueId = m_freeQueueIndexes.dma.back();
          m_freeQueueIndexes.dma.pop_back();
          m_copyQueue = m_device.getQueue(queueFamilyIndex, queueId);
          m_copyQueueIndex = queueFamilyIndex;
        }
        else if (!m_freeQueueIndexes.universal.empty())
        {
          uint32_t queueFamilyIndex = m_freeQueueIndexes.universalIndex;
          uint32_t queueId = m_freeQueueIndexes.universal.back();
          m_freeQueueIndexes.universal.pop_back();
          m_copyQueue = m_device.getQueue(queueFamilyIndex, queueId);
          m_copyQueueIndex = queueFamilyIndex;
        }
      }

      m_semaphores = Rabbitpool2<VulkanSemaphore>([device]()
      {
        auto fence = device.createSemaphore(vk::SemaphoreCreateInfo());
        return VulkanSemaphore(std::shared_ptr<vk::Semaphore>(new vk::Semaphore(fence), [=](vk::Semaphore* ptr)
        {
          device.destroySemaphore(*ptr);
          delete ptr;
        }));
      });
      m_fences = Rabbitpool2<VulkanFence>([device]()
      {
        auto fence = device.createFence(vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled));
        return VulkanFence(std::shared_ptr<vk::Fence>(new vk::Fence(fence), [=](vk::Fence* ptr)
        {
          device.destroyFence(*ptr);
          delete ptr;
        }));
      });

      // If we didn't have the queues, we don't really have anything for the lists either.
      m_copyQueueIndex = m_copyQueueIndex != -1 ? m_copyQueueIndex : m_mainQueueIndex;
      m_computeQueueIndex = m_computeQueueIndex != -1 ? m_computeQueueIndex : m_mainQueueIndex;

      m_copyListPool = Rabbitpool2<VulkanCommandList>([&]() {return createCommandBuffer(m_copyQueueIndex); });
      m_computeListPool = Rabbitpool2<VulkanCommandList>([&]() {return createCommandBuffer(m_computeQueueIndex); });
      m_graphicsListPool = Rabbitpool2<VulkanCommandList>([&]() {return createCommandBuffer(m_mainQueueIndex); });

      //auto memProp = m_physDevice.getMemoryProperties();
      //printMemoryTypeInfos(memProp);
      /* if VK_KHR_display is a things, use the below. For now, Borderless fullscreen!
      {
          auto displayProperties = m_physDevice.getDisplayPropertiesKHR();
          F_SLOG("Vulkan", "Trying to query available displays...\n");
          for (auto&& prop : displayProperties)
          {
              F_LOG("%s Physical Dimensions: %dx%d Resolution: %dx%d %s %s\n",
                  prop.displayName, prop.physicalDimensions.width, prop.physicalDimensions.height,
                  prop.physicalResolution.width, prop.physicalResolution.height,
                  ((prop.planeReorderPossible) ? " Plane Reorder possible " : ""),
                  ((prop.persistentContent) ? " Persistent Content " : ""));
          }
      }*/

      // lets make some immutable samplers for shaders...

      vk::SamplerCreateInfo s0 = vk::SamplerCreateInfo()
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear);
      vk::SamplerCreateInfo s1 = vk::SamplerCreateInfo()
        .setMagFilter(vk::Filter::eNearest)
        .setMinFilter(vk::Filter::eNearest);
      vk::SamplerCreateInfo s2 = vk::SamplerCreateInfo()
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eRepeat);
      vk::SamplerCreateInfo s3 = vk::SamplerCreateInfo()
        .setMagFilter(vk::Filter::eNearest)
        .setMinFilter(vk::Filter::eNearest)
        .setAddressModeU(vk::SamplerAddressMode::eRepeat);

      m_bilinearSampler = m_device.createSampler(s0);
      m_pointSampler = m_device.createSampler(s1);
      m_bilinearSamplerWrap = m_device.createSampler(s2);
      m_pointSamplerWrap = m_device.createSampler(s3);
    }

    VulkanDevice::~VulkanDevice()
    {
      m_device.waitIdle();
      collectTrash();
      m_fences.clear();
      m_semaphores.clear();
      m_copyListPool.clear();
      m_computeListPool.clear();
      m_graphicsListPool.clear();
      m_renderpasses.clear();
      m_dynamicUpload.reset();

      m_device.destroySampler(m_bilinearSampler);
      m_device.destroySampler(m_pointSampler);
      m_device.destroySampler(m_bilinearSamplerWrap);
      m_device.destroySampler(m_pointSamplerWrap);

      m_device.destroy();
    }

    std::shared_ptr<prototypes::SwapchainImpl> VulkanDevice::createSwapchain(GraphicsSurface& surface, SwapchainDescriptor descriptor)
    {
      auto format = descriptor.desc.format;
      auto mode = descriptor.desc.mode;
      auto buffers = descriptor.desc.bufferCount;

      auto natSurface = std::static_pointer_cast<VulkanGraphicsSurface>(surface.native());
      auto surfaceCap = m_physDevice.getSurfaceCapabilitiesKHR(natSurface->native());
#ifdef FAZE_GRAPHICS_EXTRA_INFO
      F_SLOG("Graphics/Surface", "surface details\n");
      F_SLOG("Graphics/Surface", "min image Count: %d\n", surfaceCap.minImageCount);
      F_SLOG("Graphics/Surface", "current res %dx%d\n", surfaceCap.currentExtent.width, surfaceCap.currentExtent.height);
      F_SLOG("Graphics/Surface", "min res %dx%d\n", surfaceCap.minImageExtent.width, surfaceCap.minImageExtent.height);
      F_SLOG("Graphics/Surface", "max res %dx%d\n", surfaceCap.maxImageExtent.width, surfaceCap.maxImageExtent.height);
#endif

      auto formats = m_physDevice.getSurfaceFormatsKHR(natSurface->native());

      auto wantedFormat = formatToVkFormat(format).storage;
      auto backupFormat = vk::Format::eB8G8R8A8Unorm;
      bool found = false;
      bool hadBackup = false;
      for (auto&& fmt : formats)
      {
        if (wantedFormat == fmt.format)
        {
          found = true;
        }
        if (backupFormat == fmt.format)
        {
          hadBackup = true;
        }
        F_SLOG("Graphics/Surface", "format: %s\n", vk::to_string(fmt.format).c_str());
      }

      if (!found)
      {
        F_ASSERT(hadBackup, "uh oh, backup format wasn't supported either.");
        wantedFormat = backupFormat;
        format = FormatType::Unorm8BGRA;
      }

      auto asd = m_physDevice.getSurfacePresentModesKHR(natSurface->native());

      vk::PresentModeKHR khrmode;
      switch (mode)
      {
      case PresentMode::Mailbox:
        khrmode = vk::PresentModeKHR::eMailbox;
        break;
      case PresentMode::Fifo:
        khrmode = vk::PresentModeKHR::eFifo;
        break;
      case PresentMode::FifoRelaxed:
        khrmode = vk::PresentModeKHR::eFifoRelaxed;
        break;
      case PresentMode::Immediate:
      default:
        khrmode = vk::PresentModeKHR::eImmediate;
        break;
      }

      bool hadChosenMode = false;
      for (auto&& fmt : asd)
      {
#ifdef        FAZE_GRAPHICS_EXTRA_INFO
        if (fmt == vk::PresentModeKHR::eImmediate)
          F_SLOG("Graphics/AvailablePresentModes", "Immediate\n");
        if (fmt == vk::PresentModeKHR::eMailbox)
          F_SLOG("Graphics/AvailablePresentModes", "Mailbox\n");
        if (fmt == vk::PresentModeKHR::eFifo)
          F_SLOG("Graphics/AvailablePresentModes", "Fifo\n");
        if (fmt == vk::PresentModeKHR::eFifoRelaxed)
          F_SLOG("Graphics/AvailablePresentModes", "FifoRelaxed\n");
#endif
        if (fmt == khrmode)
          hadChosenMode = true;
      }
      if (!hadChosenMode)
      {
        khrmode = vk::PresentModeKHR::eFifo; // guaranteed by spec
        mode = PresentMode::Fifo;
      }

      auto extent = surfaceCap.currentExtent;
      if (extent.height == 0)
      {
        extent.height = surfaceCap.minImageExtent.height;
      }
      if (extent.width == 0)
      {
        extent.width = surfaceCap.minImageExtent.width;
      }

      if (!m_physDevice.getSurfaceSupportKHR(m_mainQueueIndex, natSurface->native()))
      {
        F_ASSERT(false, "Was not supported.");
      }

      for (auto&& f : formats)
      {
        auto cs = f.colorSpace;
        F_LOG("found surface format %s %s\n", vk::to_string(cs).c_str(), vk::to_string(f.format).c_str());
      }

      int minImageCount = std::max(static_cast<int>(surfaceCap.minImageCount), buffers);
      F_SLOG("Vulkan", "creating swapchain to %ux%u, buffers %d\n", extent.width, extent.height, minImageCount);

      vk::SwapchainCreateInfoKHR info = vk::SwapchainCreateInfoKHR()
        .setSurface(natSurface->native())
        .setMinImageCount(minImageCount)
        .setImageFormat(wantedFormat)
        .setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)
        .setImageExtent(surfaceCap.currentExtent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)  // linear to here
        .setImageSharingMode(vk::SharingMode::eExclusive)
        //	.setPreTransform(vk::SurfaceTransformFlagBitsKHR::eInherit)
        //	.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eInherit)
        .setPresentMode(khrmode)
        .setClipped(false);

      auto swapchain = m_device.createSwapchainKHR(info);
      auto sc = std::shared_ptr<VulkanSwapchain>(new VulkanSwapchain(swapchain, *natSurface, m_semaphores.allocate()),
        [dev = m_device](VulkanSwapchain* ptr)
      {
        dev.destroySwapchainKHR(ptr->native());
        delete ptr;
      });
      sc->setBufferMetadata(surfaceCap.currentExtent.width, surfaceCap.currentExtent.height, minImageCount, format, mode);
      return sc;
    }

    void VulkanDevice::adjustSwapchain(std::shared_ptr<prototypes::SwapchainImpl> swapchain, SwapchainDescriptor descriptor)
    {
      auto format = descriptor.desc.format;
      auto mode = descriptor.desc.mode;
      auto bufferCount = descriptor.desc.bufferCount;

      auto natSwapchain = std::static_pointer_cast<VulkanSwapchain>(swapchain);
      auto& natSurface = natSwapchain->surface();
      auto oldSwapchain = natSwapchain->native();

      format = (format == FormatType::Unknown) ? natSwapchain->getDesc().format : format;
      bufferCount = (bufferCount == -1) ? natSwapchain->getDesc().buffers : bufferCount;
      mode = (mode == PresentMode::Unknown) ? natSwapchain->getDesc().mode : mode;

      auto formats = m_physDevice.getSurfaceFormatsKHR(natSurface.native());

      auto wantedFormat = formatToVkFormat(format).storage;
      auto backupFormat = vk::Format::eB8G8R8A8Unorm;
      bool found = false;
      bool hadBackup = false;
      for (auto&& fmt : formats)
      {
        if (wantedFormat == fmt.format)
        {
          found = true;
        }
        if (backupFormat == fmt.format)
        {
          hadBackup = true;
        }
        F_SLOG("Graphics/Surface", "format: %s\n", vk::to_string(fmt.format).c_str());
      }

      if (!found)
      {
        F_ASSERT(hadBackup, "uh oh, backup format wasn't supported either.");
        wantedFormat = backupFormat;
        format = FormatType::Unorm8BGRA;
      }

      auto surfaceCap = m_physDevice.getSurfaceCapabilitiesKHR(natSurface.native());

      auto& extent = surfaceCap.currentExtent;
      if (extent.height < 8)
      {
        extent.height = 8;
      }
      if (extent.width < 8)
      {
        extent.width = 8;
      }

      int minImageCount = std::max(static_cast<int>(surfaceCap.minImageCount), bufferCount);

      if (!m_physDevice.getSurfaceSupportKHR(m_mainQueueIndex, natSurface.native()))
      {
        F_ASSERT(false, "Was not supported.");
      }
      vk::PresentModeKHR khrmode;
      switch (mode)
      {
      case PresentMode::Mailbox:
        khrmode = vk::PresentModeKHR::eMailbox;
        break;
      case PresentMode::Fifo:
        khrmode = vk::PresentModeKHR::eFifo;
        break;
      case PresentMode::FifoRelaxed:
        khrmode = vk::PresentModeKHR::eFifoRelaxed;
        break;
      case PresentMode::Immediate:
      default:
        khrmode = vk::PresentModeKHR::eImmediate;
        break;
      }

      vk::SwapchainCreateInfoKHR info = vk::SwapchainCreateInfoKHR()
        .setSurface(natSurface.native())
        .setMinImageCount(minImageCount)
        .setImageFormat(formatToVkFormat(format).storage)
        .setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)
        .setImageExtent(surfaceCap.currentExtent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)  // linear to here
        .setImageSharingMode(vk::SharingMode::eExclusive)
        //	.setPreTransform(vk::SurfaceTransformFlagBitsKHR::eInherit)
        //	.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eInherit)
        .setPresentMode(khrmode)
        .setClipped(false)
        .setOldSwapchain(oldSwapchain);

      natSwapchain->setSwapchain(m_device.createSwapchainKHR(info));

      m_device.destroySwapchainKHR(oldSwapchain);
      //F_SLOG("Vulkan", "adjusting swapchain to %ux%u\n", surfaceCap.currentExtent.width, surfaceCap.currentExtent.height);
      natSwapchain->setBufferMetadata(surfaceCap.currentExtent.width, surfaceCap.currentExtent.height, minImageCount, format, mode);
    }

    vector<std::shared_ptr<prototypes::TextureImpl>> VulkanDevice::getSwapchainTextures(std::shared_ptr<prototypes::SwapchainImpl> sc)
    {
      auto native = std::static_pointer_cast<VulkanSwapchain>(sc);
      auto images = m_device.getSwapchainImagesKHR(native->native());
      vector<std::shared_ptr<prototypes::TextureImpl>> textures;
      textures.resize(images.size());

      vector<TextureStateFlags> state;
      state.emplace_back(TextureStateFlags(vk::AccessFlagBits(0), vk::ImageLayout::eUndefined, m_mainQueueIndex));

      for (int i = 0; i < static_cast<int>(images.size()); ++i)
      {
        textures[i] = std::make_shared<VulkanTexture>(images[i], std::make_shared<VulkanTextureState>(VulkanTextureState{ state }), false);
      }
      return textures;
    }

    int VulkanDevice::tryAcquirePresentableImage(std::shared_ptr<prototypes::SwapchainImpl> swapchain)
    {
      auto native = std::static_pointer_cast<VulkanSwapchain>(swapchain);

      std::shared_ptr<VulkanSemaphore> freeSemaphore = m_semaphores.allocate();
      auto res = m_device.acquireNextImageKHR(native->native(), 0, freeSemaphore->native(), nullptr);

      if (res.result != vk::Result::eSuboptimalKHR && res.result != vk::Result::eSuccess)
      {
        F_SLOG("Vulkan/AcquireNextImage", "error: %s\n", to_string(res.result).c_str());
        return -1;
      }
      native->setCurrentPresentableImageIndex(res.value);
      native->setAcquireSemaphore(freeSemaphore);

      return res.value;
    }

    // TODO: add fence here, so that we can detect that "we cannot render yet, do something else". Bonus thing honestly.
    int VulkanDevice::acquirePresentableImage(std::shared_ptr<prototypes::SwapchainImpl> sc)
    {
      auto native = std::static_pointer_cast<VulkanSwapchain>(sc);

      std::shared_ptr<VulkanSemaphore> freeSemaphore = m_semaphores.allocate();
      auto res = m_device.acquireNextImageKHR(native->native(), (std::numeric_limits<uint64_t>::max)(), freeSemaphore->native(), nullptr);

      if (res.result != vk::Result::eSuboptimalKHR && res.result != vk::Result::eSuccess)
      {
        F_SLOG("Vulkan/AcquireNextImage", "error: %s\n", to_string(res.result).c_str());
        //return -1;
      }
      native->setCurrentPresentableImageIndex(res.value);
      native->setAcquireSemaphore(freeSemaphore);

      return res.value;
    }

    std::shared_ptr<vk::RenderPass> VulkanDevice::createRenderpass(const vk::RenderPassCreateInfo& info)
    {
      auto attachmentHash = HashMemory(info.pAttachments, info.attachmentCount);
      auto dependencyHash = HashMemory(info.pDependencies, info.dependencyCount);
      auto subpassHash = HashMemory(info.pSubpasses, info.subpassCount);
      auto totalHash = hash_combine(hash_combine(attachmentHash, dependencyHash), subpassHash);

      auto found = m_renderpasses.find(totalHash);
      if (found != m_renderpasses.end())
      {
        //GFX_LOG("Reusing old renderpass.\n");
        return found->second;
      }

      auto renderpass = m_device.createRenderPass(info);

      auto newRP = std::shared_ptr<vk::RenderPass>(new vk::RenderPass(renderpass), [dev = m_device](vk::RenderPass* ptr)
      {
        dev.destroyRenderPass(*ptr);
        delete ptr;
      });

      m_renderpasses[totalHash] = newRP;

      GFX_LOG("Created new renderpass object.\n");
      return newRP;
    }

    vk::GraphicsPipelineCreateInfo getDesc(GraphicsPipeline& pipeline, gfxpacket::Subpass& subpass)
    {
      GraphicsPipelineDescriptor::Desc d = pipeline.descriptor.desc;

      vk::PipelineTessellationStateCreateInfo tesselation;
      
      vk::PipelineMultisampleStateCreateInfo multisample;
      // vk::PipelineVertexInputStateCreateInfo vertexInput;
      // VertexInput is not supported in Faze, deemed to be old feature in compute age without benefits.
/*
        const VkPipelineShaderStageCreateInfo* pStages;
        const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
        const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
        const VkPipelineTessellationStateCreateInfo* pTessellationState;
        const VkPipelineViewportStateCreateInfo* pViewportState;
        const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
        const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
        const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
        const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
        const VkPipelineDynamicStateCreateInfo* pDynamicState;
*/

      vector<vk::PipelineColorBlendAttachmentState> states(8);
      for (int i = 0; i < 8; ++i)
      {
        auto& rtb = d.blendDesc.desc.renderTarget[i].desc;
        vk::PipelineColorBlendAttachmentState state;
        state = state.setBlendEnable(rtb.blendEnable); // Is blending enabled for attachment
        state = state.setSrcColorBlendFactor(convertBlend(rtb.srcBlend));
        state = state.setDstColorBlendFactor(convertBlend(rtb.destBlend));
        state = state.setColorBlendOp(convertBlendOp(rtb.blendOp));
        state = state.setSrcAlphaBlendFactor(convertBlend(rtb.srcBlendAlpha));
        state = state.setDstAlphaBlendFactor(convertBlend(rtb.destBlendAlpha));
        state = state.setAlphaBlendOp(convertBlendOp(rtb.blendOpAlpha));
        state = state.setColorWriteMask(convertColorWriteEnable(rtb.colorWriteEnable));
        // differences to DX12
        //desc.BlendState.RenderTarget[i].LogicOpEnable = rtb.logicOpEnable;
        //desc.BlendState.RenderTarget[i].LogicOp = convertLogicOp(rtb.logicOp);
        states[i] = state;
      }

      // vk::PipelineColorBlendAdvancedStateCreateInfoEXT advancedext;
      // BlendState
      // desc.BlendState.IndependentBlendEnable = d.blendDesc.desc.alphaToCoverageEnable;
      // desc.BlendState.AlphaToCoverageEnable = d.blendDesc.desc.independentBlendEnable;

      vk::PipelineColorBlendStateCreateInfo blendstate  = vk::PipelineColorBlendStateCreateInfo()
      .setAttachmentCount(d.numRenderTargets)
      .setPAttachments(states.data())
      .setLogicOp(vk::LogicOp::eNoOp) // TODO: one logic for all attachments? difference to dx12
      .setLogicOpEnable(false);

      vk::PipelineRasterizationStateCreateInfo rasterState;
      {
        // Rasterization
        auto& rd = d.rasterDesc.desc;

        F_ASSERT(rd.conservativeRaster != faze::ConservativeRasterization::On, "Conservative raster ext code not written.");


        rasterState = vk::PipelineRasterizationStateCreateInfo()
          .setDepthClampEnable(rd.depthBiasClamp)
          .setRasterizerDiscardEnable(true)
          .setPolygonMode(vk::PolygonMode::eFill)
          .setCullMode(convertCullMode(rd.cull))
          .setFrontFace(vk::FrontFace::eCounterClockwise)
          .setDepthBiasEnable(bool(rd.depthBias))
          .setDepthBiasClamp(rd.depthBiasClamp)
          .setDepthBiasSlopeFactor(rd.slopeScaledDepthBias);
          //.setDepthBiasConstantFactor(float(rd.depthBias))
      }
/*
        desc.RasterizerState.FillMode = convertFillMode(rd.fill);
        desc.RasterizerState.CullMode = convertCullMode(rd.cull);
        desc.RasterizerState.FrontCounterClockwise = rd.frontCounterClockwise;
        desc.RasterizerState.DepthBias = rd.depthBias;
        desc.RasterizerState.DepthBiasClamp = rd.depthBiasClamp;
        desc.RasterizerState.SlopeScaledDepthBias = rd.slopeScaledDepthBias;
        desc.RasterizerState.DepthClipEnable = rd.depthClipEnable;
        desc.RasterizerState.MultisampleEnable = rd.multisampleEnable;
        desc.RasterizerState.AntialiasedLineEnable = rd.antialiasedLineEnable;
        desc.RasterizerState.ForcedSampleCount = rd.forcedSampleCount;
        desc.RasterizerState.ConservativeRaster = convertConservativeRasterization(rd.conservativeRaster);
      }
      */
      vk::PipelineDynamicStateCreateInfo dynamicState;
      vk::PipelineDepthStencilStateCreateInfo depthstencil;
      /*
      {
        // DepthStencil
        auto& dss = d.dsdesc.desc;
        desc.DepthStencilState.DepthEnable = dss.depthEnable;
        desc.DepthStencilState.DepthWriteMask = convertDepthWriteMask(dss.depthWriteMask);
        desc.DepthStencilState.DepthFunc = convertComparisonFunc(dss.depthFunc);
        desc.DepthStencilState.StencilEnable = dss.stencilEnable;
        desc.DepthStencilState.StencilReadMask = dss.stencilReadMask;
        desc.DepthStencilState.StencilWriteMask = dss.stencilWriteMask;
        desc.DepthStencilState.FrontFace.StencilFailOp = convertStencilOp(dss.frontFace.desc.failOp);
        desc.DepthStencilState.FrontFace.StencilDepthFailOp = convertStencilOp(dss.frontFace.desc.depthFailOp);
        desc.DepthStencilState.FrontFace.StencilPassOp = convertStencilOp(dss.frontFace.desc.passOp);
        desc.DepthStencilState.FrontFace.StencilFunc = convertComparisonFunc(dss.frontFace.desc.stencilFunc);
        desc.DepthStencilState.BackFace.StencilFailOp = convertStencilOp(dss.backFace.desc.failOp);
        desc.DepthStencilState.BackFace.StencilDepthFailOp = convertStencilOp(dss.backFace.desc.depthFailOp);
        desc.DepthStencilState.BackFace.StencilPassOp = convertStencilOp(dss.backFace.desc.passOp);
        desc.DepthStencilState.BackFace.StencilFunc = convertComparisonFunc(dss.backFace.desc.stencilFunc);
      }
      // subpass things here
      desc.NumRenderTargets = static_cast<unsigned>(subpass.rtvs.size());
      for (size_t i = 0; i < subpass.rtvs.size(); ++i)
      {
        desc.RTVFormats[i] = formatTodxFormat(subpass.rtvs[i].format()).view;
      }
      desc.DSVFormat = (subpass.dsvs.size() == 0) ? DXGI_FORMAT_UNKNOWN : formatTodxFormat(subpass.dsvs[0].format()).view;
      for (size_t i = subpass.rtvs.size(); i < 8; ++i)
      {
        desc.RTVFormats[i] = formatTodxFormat(d.rtvFormats[i]).view;
      }
      desc.SampleMask = UINT_MAX;
      /*
      desc.NumRenderTargets = d.numRenderTargets;
      for (int i = 0; i < 8; ++i)
      {
        desc.RTVFormats[i] = formatTodxFormat(d.rtvFormats[i]).view;
      }
      desc.DSVFormat = formatTodxFormat(d.dsvFormat).view;
      */

      // Input assembly to specify 
      // desc.PrimitiveTopologyType = convertPrimitiveTopologyType(d.primitiveTopology);
      // desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
      vk::PipelineInputAssemblyStateCreateInfo inputAssembly;

      //desc.SampleDesc.Count = d.sampleCount;
      //desc.SampleDesc.Quality = d.sampleCount > 1 ? DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN : 0;
      vk::GraphicsPipelineCreateInfo desc{};
      desc = desc.setPColorBlendState(&blendstate);
      desc = desc.setPRasterizationState(&rasterState);
      desc = desc.setPDepthStencilState(&depthstencil);
      return desc;
    }

    void VulkanDevice::updatePipeline(GraphicsPipeline& pipeline, vk::RenderPass, gfxpacket::Subpass& subpass)
    {
      auto hash = subpass.hash;
      bool missing = true;

      GraphicsPipeline::FullPipeline* ptr = nullptr;

      for (auto&& it : *pipeline.m_pipelines)
      {
        if (it.hash == hash)
        {
          missing = false;
          ptr = &it;
          break;
        }
      }

      if (missing)
      {
        pipeline.m_pipelines->emplace_back(GraphicsPipeline::FullPipeline{});
        ptr = &pipeline.m_pipelines->back();
        ptr->hash = hash;
      }

      if (ptr->needsUpdating())
      {
        F_LOG("updating pipeline for hash %zu\n", hash);
        auto desc = getDesc(pipeline, subpass);

        GraphicsPipelineDescriptor::Desc d = pipeline.descriptor.desc;
        vector<MemoryBlob> blobs;
        if (!d.vertexShaderPath.empty())
        {
          auto shader = m_shaders.shader(ShaderCreateInfo(d.vertexShaderPath, ShaderType::Vertex, d.layout));
          blobs.emplace_back(shader);
          // insert to pipeline description

          if (ptr->vs.empty())
          {
            ptr->vs = m_shaders.watch(d.vertexShaderPath, ShaderType::Vertex);
          }
          ptr->vs.react();
        }

        if (!d.hullShaderPath.empty())
        {
          auto shader = m_shaders.shader(ShaderCreateInfo(d.hullShaderPath, ShaderType::TessControl, d.layout));
          blobs.emplace_back(shader);
          // insert to pipeline description

          if (ptr->hs.empty())
          {
            ptr->hs = m_shaders.watch(d.hullShaderPath, ShaderType::TessControl);
          }
          ptr->hs.react();
        }

        if (!d.domainShaderPath.empty())
        {
          auto shader = m_shaders.shader(ShaderCreateInfo(d.domainShaderPath, ShaderType::TessEvaluation, d.layout));
          blobs.emplace_back(shader);
          // insert to pipeline description
          if (ptr->ds.empty())
          {
            ptr->ds = m_shaders.watch(d.domainShaderPath, ShaderType::TessEvaluation);
          }
          ptr->ds.react();
        }

        if (!d.geometryShaderPath.empty())
        {
          auto shader = m_shaders.shader(ShaderCreateInfo(d.geometryShaderPath, ShaderType::Geometry, d.layout));
          blobs.emplace_back(shader);
          // insert to pipeline description
          if (ptr->gs.empty())
          {
            ptr->gs = m_shaders.watch(d.geometryShaderPath, ShaderType::Geometry);
          }
          ptr->gs.react();
        }

        if (!d.pixelShaderPath.empty())
        {
          auto shader = m_shaders.shader(ShaderCreateInfo(d.pixelShaderPath, ShaderType::Pixel, d.layout));
          blobs.emplace_back(shader);
          // insert to pipeline description
          if (ptr->ps.empty())
          {
            ptr->ps = m_shaders.watch(d.pixelShaderPath, ShaderType::Pixel);
          }
          ptr->ps.react();
        }

         // D3D12_PRIMITIVE_TOPOLOGY primitive = convertPrimitiveTopology(d.primitiveTopology);
         /*
        ComPtr<ID3D12PipelineState> pipe;
        FAZE_CHECK_HR(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipe)));
        */
        auto pipe = m_device.createGraphicsPipeline(nullptr, desc);

        auto* natptr = static_cast<VulkanPipeline*>(ptr->pipeline.get());
        if (ptr->pipeline)
        {
          m_trash->pipelines.emplace_back(natptr->m_pipeline);
          natptr->m_pipeline = pipe;
        }
        auto newPipe = createPipeline(pipeline.descriptor);

        pipeline.m_pipelines->emplace_back(std::make_pair(hash, std::make_shared<DX12Pipeline>(DX12Pipeline(pipe, root, primitive))));
      }
    }

    void VulkanDevice::updatePipeline(ComputePipeline& pipe)
    {
      auto sci = ShaderCreateInfo(pipe.descriptor.shader(), ShaderType::Compute, pipe.descriptor.layout)
        .setComputeGroups(pipe.descriptor.shaderGroups);
      auto shader = m_shaders.shader(sci);
      auto vkpipe = std::static_pointer_cast<VulkanPipeline>(pipe.impl);

      auto shaderInfo = vk::ShaderModuleCreateInfo().setCodeSize(shader.size()).setPCode(reinterpret_cast<uint32_t*>(shader.data()));

      auto smodule = m_device.createShaderModule(shaderInfo);

      auto pipelineDesc = vk::ComputePipelineCreateInfo()
        .setStage(vk::PipelineShaderStageCreateInfo()
          .setModule(smodule)
          .setPName("main")
          .setStage(vk::ShaderStageFlagBits::eCompute))
        .setLayout(vkpipe->m_pipelineLayout);
      auto result = m_device.createComputePipeline(nullptr, pipelineDesc);
      m_device.destroyShaderModule(smodule);
      if (result)
      {
        m_device.destroyPipeline(result.operator VkPipeline());
      }
    }


    std::shared_ptr<prototypes::PipelineImpl> VulkanDevice::createPipeline(GraphicsPipelineDescriptor desc)
    {
      auto bindings = gatherSetLayoutBindings(desc.desc.layout, vk::ShaderStageFlagBits::eAllGraphics);
      vk::DescriptorSetLayoutCreateInfo info = vk::DescriptorSetLayoutCreateInfo()
        .setBindingCount(static_cast<uint32_t>(bindings.size()))
        .setPBindings(bindings.data());

      auto setlayout = m_device.createDescriptorSetLayout(info);

      auto pipelineLayout = m_device.createPipelineLayout(vk::PipelineLayoutCreateInfo()
        .setSetLayoutCount(1)
        .setPSetLayouts(&setlayout));
      return std::make_shared<VulkanPipeline>(setlayout, pipelineLayout);
    }

    std::shared_ptr<prototypes::PipelineImpl> VulkanDevice::createPipeline(ComputePipelineDescriptor desc)
    {
      auto bindings = gatherSetLayoutBindings(desc.layout, vk::ShaderStageFlagBits::eCompute);
      vk::DescriptorSetLayoutCreateInfo info = vk::DescriptorSetLayoutCreateInfo()
        .setBindingCount(static_cast<uint32_t>(bindings.size()))
        .setPBindings(bindings.data());

      auto setlayout = m_device.createDescriptorSetLayout(info);

      auto pipelineLayout = m_device.createPipelineLayout(vk::PipelineLayoutCreateInfo()
        .setSetLayoutCount(1)
        .setPSetLayouts(&setlayout));
      return std::make_shared<VulkanPipeline>(setlayout, pipelineLayout);
    }

    vector<vk::DescriptorSetLayoutBinding> VulkanDevice::gatherSetLayoutBindings(ShaderInputDescriptor desc, vk::ShaderStageFlags flags)
    {
      auto layout = desc.desc;
      vector<vk::DescriptorSetLayoutBinding> bindings;

      int slot = 0;
      if (!layout.constantStructBody.empty())
      {
        bindings.push_back(vk::DescriptorSetLayoutBinding()
          .setBinding(slot++).setDescriptorCount(1)
          .setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
          .setStageFlags(flags));
      }

      auto resources = layout.sortedResources;

      auto getDescriptor = [](ShaderResourceType type, bool ro)
      {
        if (type == ShaderResourceType::Buffer)
        {
          if (ro)
          {
            return vk::DescriptorType::eUniformTexelBuffer;
          }
          else
          {
            return vk::DescriptorType::eStorageTexelBuffer;
          }
        }
        else if (type == ShaderResourceType::ByteAddressBuffer || type == ShaderResourceType::StructuredBuffer)
        {
          return vk::DescriptorType::eStorageBuffer;
        }
        else
        {
          if (ro)
          {
            return vk::DescriptorType::eSampledImage;
          }
          else
          {
            return vk::DescriptorType::eStorageImage;
          }
        }
      };

      vk::DescriptorType currentType;
      int descCount = 0;
      for (auto&& it : resources)
      {
        bindings.push_back(vk::DescriptorSetLayoutBinding()
          .setBinding(slot++)
          .setDescriptorCount(1) // TODO: if the descriptor is array, then touch this
          .setDescriptorType(getDescriptor(it.type, it.readonly))
          .setStageFlags(flags));
      }

      bindings.push_back(vk::DescriptorSetLayoutBinding()
        .setBinding(slot++).setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eSampler)
        .setPImmutableSamplers(&m_bilinearSampler)
        .setStageFlags(flags));
      bindings.push_back(vk::DescriptorSetLayoutBinding()
        .setBinding(slot++).setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eSampler)
        .setPImmutableSamplers(&m_pointSampler)
        .setStageFlags(flags));
      bindings.push_back(vk::DescriptorSetLayoutBinding()
        .setBinding(slot++).setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eSampler)
        .setPImmutableSamplers(&m_bilinearSamplerWrap)
        .setStageFlags(flags));
      bindings.push_back(vk::DescriptorSetLayoutBinding()
        .setBinding(slot++).setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eSampler)
        .setPImmutableSamplers(&m_pointSamplerWrap)
        .setStageFlags(flags));

      return bindings;
    }

    void VulkanDevice::collectTrash()
    {
      auto latestSequenceStarted = m_seqTracker->lastSequence();

      m_collectableTrash.emplace_back(std::make_pair(latestSequenceStarted, *m_trash));
      m_trash->dynamicBuffers.clear();
      m_trash->buffers.clear();
      m_trash->textures.clear();
      m_trash->bufferviews.clear();
      m_trash->textureviews.clear();
      m_trash->heaps.clear();

      while (!m_collectableTrash.empty())
      {
        auto&& it = m_collectableTrash.front();
        if (!m_seqTracker->hasCompletedTill(it.first))
        {
          break;
        }
        for (auto&& upload : it.second.dynamicBuffers)
        {
          m_dynamicUpload->release(upload);
        }
        for (auto&& tex : it.second.textureviews)
        {
          m_device.destroyImageView(tex);
        }
        for (auto&& tex : it.second.textures)
        {
          m_device.destroyImage(tex);
        }
        for (auto&& descriptor : it.second.bufferviews)
        {
          m_device.destroyBufferView(descriptor);
        }
        for (auto&& descriptor : it.second.buffers)
        {
          m_device.destroyBuffer(descriptor);
        }
        for (auto&& descriptor : it.second.heaps)
        {
          m_device.freeMemory(descriptor);
        }

        m_collectableTrash.pop_front();
      }
    }

    void VulkanDevice::waitGpuIdle()
    {
      m_device.waitIdle();
    }

    MemoryRequirements VulkanDevice::getReqs(ResourceDescriptor desc)
    {
      MemoryRequirements reqs{};
      vk::MemoryRequirements requirements;
      if (desc.desc.dimension == FormatDimension::Buffer)
      {
        auto buffer = m_device.createBuffer(fillBufferInfo(desc));
        requirements = m_device.getBufferMemoryRequirements(buffer);
        m_device.destroyBuffer(buffer);
      }
      else
      {
        auto image = m_device.createImage(fillImageInfo(desc));
        requirements = m_device.getImageMemoryRequirements(image);
        m_device.destroyImage(image);
      }
      auto memProp = m_physDevice.getMemoryProperties();
      auto searchProperties = getMemoryProperties(desc.desc.usage);
      auto index = FindProperties(memProp, requirements.memoryTypeBits, searchProperties.optimal);
      F_ASSERT(index != -1, "Couldn't find optimal memory... maybe try default :D?"); // searchProperties.def
      auto packed = packInt64(index, 0);
      //int32_t v1, v2;
      //unpackInt64(packed, v1, v2);
      //F_ILOG("Vulkan", "Did (unnecessary) packing tricks, packed %d -> %zd -> %d", index, packed, v1);
      reqs.heapType = packed;
      reqs.alignment = requirements.alignment;
      reqs.bytes = requirements.size;

      if (reqs.alignment < 128)
      {
        reqs.alignment = ((128 + reqs.alignment - 1) / reqs.alignment) * reqs.alignment;
      }

      return reqs;
    }

    std::shared_ptr<prototypes::RenderpassImpl> VulkanDevice::createRenderpass()
    {
      return std::make_shared<VulkanRenderpass>();
    }


    GpuHeap VulkanDevice::createHeap(HeapDescriptor heapDesc)
    {
      auto&& desc = heapDesc.desc;

      int32_t index, bits;
      unpackInt64(desc.customType, index, bits);

      vk::MemoryAllocateInfo allocInfo;

      allocInfo = vk::MemoryAllocateInfo()
        .setAllocationSize(desc.sizeInBytes)
        .setMemoryTypeIndex(index);

      auto memory = m_device.allocateMemory(allocInfo);
      std::weak_ptr<Garbage> weak = m_trash;
      return GpuHeap(std::shared_ptr<VulkanHeap>(new VulkanHeap(memory),
        [weak, dev = m_device](VulkanHeap* ptr)
      {
        if (auto trash = weak.lock())
        {
          trash->heaps.emplace_back(ptr->native());
        }
        else
        {
          dev.freeMemory(ptr->native());
        }
        delete ptr;
      }), std::move(heapDesc));
    }

    std::shared_ptr<prototypes::BufferImpl> VulkanDevice::createBuffer(ResourceDescriptor& desc)
    {
      auto vkdesc = fillBufferInfo(desc);
      auto buffer = m_device.createBuffer(vkdesc);
      /*
      vk::MemoryDedicatedAllocateInfoKHR dediInfo;
      dediInfo.setBuffer(buffer);

      auto bufMemReq = m_device.getBufferMemoryRequirements2KHR(vk::BufferMemoryRequirementsInfo2KHR().setBuffer(buffer));
      auto memProp = m_physDevice.getMemoryProperties();
      auto searchProperties = getMemoryProperties(desc.desc.usage);
      auto index = FindProperties(memProp, bufMemReq.memoryRequirements.memoryTypeBits, searchProperties.optimal);

      auto res = m_device.allocateMemory(vk::MemoryAllocateInfo().setPNext(&dediInfo).setAllocationSize(bufMemReq.memoryRequirements.size).setMemoryTypeIndex(index));

      m_device.bindBufferMemory(buffer, res.operator VkDeviceMemory(), 0);

      VulkanBufferState state{};
      state.queueIndex = m_mainQueueIndex;
      std::weak_ptr<Garbage> weak = m_trash;
      return std::shared_ptr<VulkanBuffer>(new VulkanBuffer(buffer, std::make_shared<VulkanBufferState>(state)),
          [weak, dev = m_device, mem = res.operator VkDeviceMemory()](VulkanBuffer* ptr)
      {
          if (auto trash = weak.lock())
          {
              trash->buffers.emplace_back(ptr->native());
              trash->heaps.emplace_back(mem);
          }
          else
          {
              dev.destroyBuffer(ptr->native());
              dev.freeMemory(mem);
          }
          delete ptr;
      });
      */
      VulkanBufferState state{};
      state.queueIndex = m_mainQueueIndex;
      std::weak_ptr<Garbage> weak = m_trash;
      return std::shared_ptr<VulkanBuffer>(new VulkanBuffer(buffer, std::make_shared<VulkanBufferState>(state)),
        [weak, dev = m_device](VulkanBuffer* ptr)
      {
        if (auto trash = weak.lock())
        {
          trash->buffers.emplace_back(ptr->native());
        }
        else
        {
          dev.destroyBuffer(ptr->native());
        }
        delete ptr;
      });
    }

    std::shared_ptr<prototypes::BufferImpl> VulkanDevice::createBuffer(HeapAllocation allocation, ResourceDescriptor& desc)
    {
      auto vkdesc = fillBufferInfo(desc);
      auto buffer = m_device.createBuffer(vkdesc);
      auto native = std::static_pointer_cast<VulkanHeap>(allocation.heap.impl);
      vk::DeviceSize size = allocation.allocation.block.offset;
      m_device.getBufferMemoryRequirements(buffer); // Only to silence the debug layers
      m_device.bindBufferMemory(buffer, native->native(), size);

      VulkanBufferState state{};
      state.queueIndex = m_mainQueueIndex;
      std::weak_ptr<Garbage> weak = m_trash;
      return std::shared_ptr<VulkanBuffer>(new VulkanBuffer(buffer, std::make_shared<VulkanBufferState>(state)),
        [weak, dev = m_device](VulkanBuffer* ptr)
      {
        if (auto trash = weak.lock())
        {
          trash->buffers.emplace_back(ptr->native());
        }
        else
        {
          dev.destroyBuffer(ptr->native());
        }
        delete ptr;
      });
    }

    std::shared_ptr<prototypes::BufferViewImpl> VulkanDevice::createBufferView(std::shared_ptr<prototypes::BufferImpl> buffer, ResourceDescriptor& resDesc, ShaderViewDescriptor& viewDesc)
    {
      auto native = std::static_pointer_cast<VulkanBuffer>(buffer);

      auto& desc = resDesc.desc;

      FormatType format = viewDesc.m_format;
      if (format == FormatType::Unknown)
        format = desc.format;

      auto elementSize = desc.stride; // TODO: actually 0 most of the time. FIX SIZE FROM FORMAT.
      auto sizeInElements = desc.width;
      auto firstElement = viewDesc.m_firstElement * elementSize;
      auto maxRange = viewDesc.m_elementCount * elementSize;
      if (viewDesc.m_elementCount <= 0)
        maxRange = elementSize * sizeInElements; // VK_WHOLE_SIZE

      vk::DescriptorType type = vk::DescriptorType::eStorageBuffer;
      if (viewDesc.m_viewType == ResourceShaderType::ReadOnly)
      {
        if (format != FormatType::Unknown && format != FormatType::Raw32)
        {
          type = vk::DescriptorType::eUniformTexelBuffer;
        }
      }
      else if (viewDesc.m_viewType == ResourceShaderType::ReadWrite)
      {
        if (format != FormatType::Unknown && format != FormatType::Raw32)
        {
          type = vk::DescriptorType::eStorageTexelBuffer;
        }
      }
      return std::make_shared<VulkanBufferView>(vk::DescriptorBufferInfo()
        .setBuffer(native->native())
        .setOffset(firstElement)
        .setRange(maxRange)
        , type);
    }

    std::shared_ptr<prototypes::TextureImpl> VulkanDevice::createTexture(ResourceDescriptor& desc)
    {
      auto vkdesc = fillImageInfo(desc);
      auto image = m_device.createImage(vkdesc);
      /*
      m_device.getImageMemoryRequirements(image); // Only to silence the debug layers

      vk::MemoryDedicatedAllocateInfoKHR dediInfo;
      dediInfo.setImage(image);

      auto bufMemReq = m_device.getImageMemoryRequirements2KHR(vk::ImageMemoryRequirementsInfo2KHR().setImage(image));
      auto memProp = m_physDevice.getMemoryProperties();
      auto searchProperties = getMemoryProperties(desc.desc.usage);
      auto index = FindProperties(memProp, bufMemReq.memoryRequirements.memoryTypeBits, searchProperties.optimal);

      auto res = m_device.allocateMemory(vk::MemoryAllocateInfo().setPNext(&dediInfo).setAllocationSize(bufMemReq.memoryRequirements.size).setMemoryTypeIndex(index));

      m_device.bindImageMemory(image, res.operator VkDeviceMemory(), 0);

      vector<TextureStateFlags> state;
      for (uint32_t slice = 0; slice < vkdesc.arrayLayers; ++slice)
      {
          for (uint32_t mip = 0; mip < vkdesc.mipLevels; ++mip)
          {
              state.emplace_back(TextureStateFlags(vk::AccessFlagBits(0), vk::ImageLayout::eUndefined, m_mainQueueIndex));
          }
      }
      std::weak_ptr<Garbage> weak = m_trash;
      return std::shared_ptr<VulkanTexture>(new VulkanTexture(image, std::make_shared<VulkanTextureState>(VulkanTextureState{ state })),
          [weak, dev = m_device, mem = res.operator VkDeviceMemory()](VulkanTexture* ptr)
      {
          if (auto trash = weak.lock())
          {
              trash->textures.emplace_back(ptr->native());
              trash->heaps.emplace_back(mem);
          }
          else
          {
              dev.destroyImage(ptr->native());
              dev.freeMemory(mem);
          }
          delete ptr;
      });  */
      vector<TextureStateFlags> state;
      for (uint32_t slice = 0; slice < vkdesc.arrayLayers; ++slice)
      {
        for (uint32_t mip = 0; mip < vkdesc.mipLevels; ++mip)
        {
          state.emplace_back(TextureStateFlags(vk::AccessFlagBits(0), vk::ImageLayout::eUndefined, m_mainQueueIndex));
        }
      }
      std::weak_ptr<Garbage> weak = m_trash;
      return std::shared_ptr<VulkanTexture>(new VulkanTexture(image, std::make_shared<VulkanTextureState>(VulkanTextureState{ state })),
        [weak, dev = m_device](VulkanTexture* ptr)
      {
        if (auto trash = weak.lock())
        {
          trash->textures.emplace_back(ptr->native());
        }
        else
        {
          dev.destroyImage(ptr->native());
        }
        delete ptr;
      });
    }

    std::shared_ptr<prototypes::TextureImpl> VulkanDevice::createTexture(HeapAllocation allocation, ResourceDescriptor& desc)
    {
      auto vkdesc = fillImageInfo(desc);
      auto image = m_device.createImage(vkdesc);
      auto native = std::static_pointer_cast<VulkanHeap>(allocation.heap.impl);
      vk::DeviceSize size = allocation.allocation.block.offset;
      m_device.getImageMemoryRequirements(image); // Only to silence the debug layers, we've already done this with same description.
      m_device.bindImageMemory(image, native->native(), size);

      vector<TextureStateFlags> state;
      for (uint32_t slice = 0; slice < vkdesc.arrayLayers; ++slice)
      {
        for (uint32_t mip = 0; mip < vkdesc.mipLevels; ++mip)
        {
          state.emplace_back(TextureStateFlags(vk::AccessFlagBits(0), vk::ImageLayout::eUndefined, m_mainQueueIndex));
        }
      }

      std::weak_ptr<Garbage> weak = m_trash;
      return std::shared_ptr<VulkanTexture>(new VulkanTexture(image, std::make_shared<VulkanTextureState>(VulkanTextureState{ state })),
        [weak, dev = m_device](VulkanTexture* ptr)
      {
        if (auto trash = weak.lock())
        {
          trash->textures.emplace_back(ptr->native());
        }
        else
        {
          dev.destroyImage(ptr->native());
        }
        delete ptr;
      });
    }

    std::shared_ptr<prototypes::TextureViewImpl> VulkanDevice::createTextureView(
      std::shared_ptr<prototypes::TextureImpl> texture, ResourceDescriptor& texDesc, ShaderViewDescriptor& viewDesc)
    {
      auto native = std::static_pointer_cast<VulkanTexture>(texture);
      auto desc = texDesc.desc;

      bool isArray = desc.arraySize > 1;

      vk::ImageViewType ivt;
      switch (desc.dimension)
      {
      case FormatDimension::Texture1D:
        ivt = vk::ImageViewType::e1D;
        if (isArray)
          ivt = vk::ImageViewType::e1DArray;
        break;
      case FormatDimension::Texture3D:
        ivt = vk::ImageViewType::e3D;
        break;
      case FormatDimension::TextureCube:
        ivt = vk::ImageViewType::eCube;
        if (isArray)
          ivt = vk::ImageViewType::eCubeArray;
        break;
      case FormatDimension::Texture2D:
      default:
        ivt = vk::ImageViewType::e2D;
        if (isArray)
          ivt = vk::ImageViewType::e2DArray;
        break;
      }

      // TODO: verify swizzle, fine for now. expect problems with backbuffer BGRA format...
      vk::ComponentMapping cm = vk::ComponentMapping()
        .setA(vk::ComponentSwizzle::eIdentity)
        .setR(vk::ComponentSwizzle::eIdentity)
        .setG(vk::ComponentSwizzle::eIdentity)
        .setB(vk::ComponentSwizzle::eIdentity);

      vk::ImageAspectFlags imgFlags;

      if (viewDesc.m_viewType == ResourceShaderType::DepthStencil)
      {
        imgFlags |= vk::ImageAspectFlagBits::eDepth;
        // TODO: support stencil format, no such FormatType yet.
        //imgFlags |= vk::ImageAspectFlagBits::eStencil;
      }
      else
      {
        imgFlags |= vk::ImageAspectFlagBits::eColor;
      }

      decltype(VK_REMAINING_MIP_LEVELS) mips = (viewDesc.m_viewType == ResourceShaderType::ReadOnly) ? texDesc.desc.miplevels - viewDesc.m_mostDetailedMip : 1;
      if (viewDesc.m_mipLevels != -1)
      {
        mips = viewDesc.m_mipLevels;
      }

      decltype(VK_REMAINING_ARRAY_LAYERS) arraySize = (viewDesc.m_arraySize != -1) ? viewDesc.m_arraySize : texDesc.desc.arraySize - viewDesc.m_arraySlice;

      vk::ImageSubresourceRange subResourceRange = vk::ImageSubresourceRange()
        .setAspectMask(imgFlags)
        .setBaseArrayLayer(viewDesc.m_arraySlice)
        .setBaseMipLevel(viewDesc.m_mostDetailedMip)
        .setLevelCount(mips)
        .setLayerCount(arraySize);

      FormatType format = viewDesc.m_format;
      if (format == FormatType::Unknown)
        format = desc.format;

      vk::ImageViewCreateInfo viewInfo = vk::ImageViewCreateInfo()
        .setViewType(ivt)
        .setFormat(formatToVkFormat(format).view)
        .setImage(native->native())
        .setComponents(cm)
        .setSubresourceRange(subResourceRange);

      auto view = m_device.createImageView(viewInfo);

      vk::DescriptorImageInfo info = vk::DescriptorImageInfo()
        .setImageLayout(vk::ImageLayout::eGeneral) // TODO: layouts
        .setImageView(view);

      vk::DescriptorType imageType = vk::DescriptorType::eInputAttachment;
      if (viewDesc.m_viewType == ResourceShaderType::ReadOnly)
      {
        imageType = vk::DescriptorType::eSampledImage;
      }
      else if (viewDesc.m_viewType == ResourceShaderType::ReadWrite)
      {
        imageType = vk::DescriptorType::eStorageImage;
      }
      else if (viewDesc.m_viewType == ResourceShaderType::DepthStencil)
      {
        imageType = vk::DescriptorType::eInputAttachment;
      }
      else if (viewDesc.m_viewType == ResourceShaderType::RenderTarget)
      {
        imageType = vk::DescriptorType::eInputAttachment;
      }

      SubresourceRange range{};
      range.mipOffset = static_cast<int16_t>(subResourceRange.baseMipLevel);
      range.mipLevels = static_cast<int16_t>(subResourceRange.levelCount);
      range.sliceOffset = static_cast<int16_t>(subResourceRange.baseArrayLayer);
      range.arraySize = static_cast<int16_t>(subResourceRange.layerCount);
      std::weak_ptr<Garbage> weak = m_trash;
      return std::shared_ptr<VulkanTextureView>(new VulkanTextureView(view, info, formatToVkFormat(format).view, imageType, range, imgFlags),
        [weak, dev = m_device](VulkanTextureView* ptr)
      {
        if (auto trash = weak.lock())
        {
          trash->textureviews.emplace_back(ptr->native().view);
        }
        else
        {
          dev.destroyImageView(ptr->native().view);
        }
        delete ptr;
      });
    }

    std::shared_ptr<prototypes::DynamicBufferViewImpl> VulkanDevice::dynamic(MemView<uint8_t> dataRange, FormatType desiredFormat)
    {
      auto upload = m_dynamicUpload->allocate(dataRange.size());
      F_ASSERT(upload, "Halp");
      memcpy(upload.data(), dataRange.data(), dataRange.size());

      auto format = formatToVkFormat(desiredFormat).view;
      //auto stride = formatSizeInfo(desiredFormat).pixelSize;

      vk::BufferViewCreateInfo desc = vk::BufferViewCreateInfo()
        .setBuffer(upload.buffer())
        .setFormat(format)
        .setOffset(upload.block.offset)
        .setRange(upload.block.size);

      auto view = m_device.createBufferView(desc);

      // will be collected promtly
      m_trash->dynamicBuffers.emplace_back(upload);
      m_trash->bufferviews.emplace_back(view);
      return std::make_shared<VulkanDynamicBufferView>(upload.buffer(), view, upload);
    }
    std::shared_ptr<prototypes::DynamicBufferViewImpl> VulkanDevice::dynamic(MemView<uint8_t> , unsigned)
    {
      /*
      vk::BufferViewCreateInfo info = vk::BufferViewCreateInfo()
        .setOffset(0) // dynamic offset here?
        .setFormat(vk::Format::eR8Unorm)
        .setRange(1) // dynamic size here
        .setBuffer(vk::Buffer()); // ? Lets look at spec
        */
      return std::make_shared<VulkanDynamicBufferView>();
    }
    std::shared_ptr<prototypes::DynamicBufferViewImpl> VulkanDevice::dynamicImage(MemView<uint8_t>, unsigned)
    {
      return std::make_shared<VulkanDynamicBufferView>();
    }

    VulkanCommandList VulkanDevice::createCommandBuffer(int queueIndex)
    {
      vk::CommandPoolCreateInfo poolInfo = vk::CommandPoolCreateInfo()
        .setFlags(vk::CommandPoolCreateFlags(vk::CommandPoolCreateFlagBits::eTransient))
        .setQueueFamilyIndex(queueIndex);
      auto pool = m_device.createCommandPool(poolInfo);
      auto buffer = m_device.allocateCommandBuffers(vk::CommandBufferAllocateInfo()
        .setCommandBufferCount(1)
        .setCommandPool(pool)
        .setLevel(vk::CommandBufferLevel::ePrimary));

      auto ptr = std::shared_ptr<vk::CommandPool>(new vk::CommandPool(pool), [&](vk::CommandPool* ptr)
      {
        m_device.destroyCommandPool(*ptr);
        delete ptr;
      });

      return VulkanCommandList(buffer[0], ptr);
    }

    std::shared_ptr<CommandBufferImpl> VulkanDevice::createDMAList()
    {
      auto seqNumber = m_seqTracker->next();
      std::weak_ptr<SequenceTracker> tracker = m_seqTracker;

      auto list = m_copyListPool.allocate();
      resetListNative(*list);
      return std::shared_ptr<VulkanCommandBuffer>(new VulkanCommandBuffer(list),
        [tracker, seqNumber](VulkanCommandBuffer* buffer)
      {
        if (auto seqTracker = tracker.lock())
        {
          seqTracker->complete(seqNumber);
        }
        delete buffer;
      });
    }
    std::shared_ptr<CommandBufferImpl> VulkanDevice::createComputeList()
    {
      auto seqNumber = m_seqTracker->next();
      std::weak_ptr<SequenceTracker> tracker = m_seqTracker;

      auto list = m_computeListPool.allocate();
      resetListNative(*list);
      return std::shared_ptr<VulkanCommandBuffer>(new VulkanCommandBuffer(list),
        [tracker, seqNumber](VulkanCommandBuffer* buffer)
      {
        if (auto seqTracker = tracker.lock())
        {
          seqTracker->complete(seqNumber);
        }
        delete buffer;
      });
    }
    std::shared_ptr<CommandBufferImpl> VulkanDevice::createGraphicsList()
    {
      auto seqNumber = m_seqTracker->next();
      std::weak_ptr<SequenceTracker> tracker = m_seqTracker;

      auto list = m_graphicsListPool.allocate();
      resetListNative(*list);
      return std::shared_ptr<VulkanCommandBuffer>(new VulkanCommandBuffer(list),
        [tracker, seqNumber](VulkanCommandBuffer* buffer)
      {
        if (auto seqTracker = tracker.lock())
        {
          seqTracker->complete(seqNumber);
        }
        delete buffer;
      });
    }

    void VulkanDevice::resetListNative(VulkanCommandList list)
    {
      m_device.resetCommandPool(list.pool(), vk::CommandPoolResetFlagBits::eReleaseResources);
    }

    std::shared_ptr<SemaphoreImpl> VulkanDevice::createSemaphore()
    {
      return m_semaphores.allocate();
    }
    std::shared_ptr<FenceImpl> VulkanDevice::createFence()
    {
      return m_fences.allocate();
    }

    void VulkanDevice::submitToQueue(vk::Queue queue,
      MemView<std::shared_ptr<CommandBufferImpl>> lists,
      MemView<std::shared_ptr<SemaphoreImpl>>     wait,
      MemView<std::shared_ptr<SemaphoreImpl>>     signal,
      MemView<std::shared_ptr<FenceImpl>>         fence)
    {
      vector<vk::Semaphore> waitList;
      vector<vk::CommandBuffer> bufferList;
      vector<vk::Semaphore> signalList;
      if (!wait.empty())
      {
        for (auto&& sema : wait)
        {
          auto native = std::static_pointer_cast<VulkanSemaphore>(sema);
          waitList.emplace_back(native->native());
        }
      }
      if (!lists.empty())
      {
        for (auto&& buffer : lists)
        {
          auto native = std::static_pointer_cast<VulkanCommandBuffer>(buffer);
          bufferList.emplace_back(native->list());
        }
      }

      if (!signal.empty())
      {
        for (auto&& sema : signal)
        {
          auto native = std::static_pointer_cast<VulkanSemaphore>(sema);
          signalList.emplace_back(native->native());
        }
      }

      vk::PipelineStageFlags waitMask = vk::PipelineStageFlagBits::eTopOfPipe;
      auto info = vk::SubmitInfo()
        .setPWaitDstStageMask(&waitMask)
        .setCommandBufferCount(static_cast<uint32_t>(bufferList.size()))
        .setPCommandBuffers(bufferList.data())
        .setWaitSemaphoreCount(static_cast<uint32_t>(waitList.size()))
        .setPWaitSemaphores(waitList.data())
        .setSignalSemaphoreCount(static_cast<uint32_t>(signalList.size()))
        .setPSignalSemaphores(signalList.data());
      if (!fence.empty())
      {
        auto native = std::static_pointer_cast<VulkanFence>(fence[0]);
        {
          vk::ArrayProxy<const vk::Fence> proxy(native->native());
          // TODO: is this ok to do always?
          m_device.resetFences(proxy);
        }
        vk::ArrayProxy<const vk::SubmitInfo> proxy(info);
        queue.submit(proxy, native->native());
      }
      else
      {
        vk::ArrayProxy<const vk::SubmitInfo> proxy(info);
        queue.submit(proxy, nullptr);
      }
    }

    void VulkanDevice::submitDMA(
      MemView<std::shared_ptr<CommandBufferImpl>> lists,
      MemView<std::shared_ptr<SemaphoreImpl>>     wait,
      MemView<std::shared_ptr<SemaphoreImpl>>     signal,
      MemView<std::shared_ptr<FenceImpl>>         fence)
    {
      vk::Queue target = m_copyQueue;
      if (m_singleQueue)
      {
        target = m_mainQueue;
      }
      submitToQueue(target, std::forward<decltype(lists)>(lists), std::forward<decltype(wait)>(wait), std::forward<decltype(signal)>(signal), std::forward<decltype(fence)>(fence));
    }

    void VulkanDevice::submitCompute(
      MemView<std::shared_ptr<CommandBufferImpl>> lists,
      MemView<std::shared_ptr<SemaphoreImpl>>     wait,
      MemView<std::shared_ptr<SemaphoreImpl>>     signal,
      MemView<std::shared_ptr<FenceImpl>>         fence)
    {
      vk::Queue target = m_computeQueue;
      if (m_singleQueue)
      {
        target = m_mainQueue;
      }
      submitToQueue(target, std::forward<decltype(lists)>(lists), std::forward<decltype(wait)>(wait), std::forward<decltype(signal)>(signal), std::forward<decltype(fence)>(fence));
    }

    void VulkanDevice::submitGraphics(
      MemView<std::shared_ptr<CommandBufferImpl>> lists,
      MemView<std::shared_ptr<SemaphoreImpl>>     wait,
      MemView<std::shared_ptr<SemaphoreImpl>>     signal,
      MemView<std::shared_ptr<FenceImpl>>         fence)
    {
      submitToQueue(m_mainQueue, std::forward<decltype(lists)>(lists), std::forward<decltype(wait)>(wait), std::forward<decltype(signal)>(signal), std::forward<decltype(fence)>(fence));
    }

    void VulkanDevice::waitFence(std::shared_ptr<FenceImpl> fence)
    {
      auto native = std::static_pointer_cast<VulkanFence>(fence);
      vk::ArrayProxy<const vk::Fence> proxy(native->native());
      auto res = m_device.waitForFences(proxy, 1, (std::numeric_limits<int64_t>::max)());
      F_ASSERT(res == vk::Result::eSuccess, "uups");
    }

    bool VulkanDevice::checkFence(std::shared_ptr<FenceImpl> fence)
    {
      auto native = std::static_pointer_cast<VulkanFence>(fence);
      auto status = m_device.getFenceStatus(native->native());

      return status == vk::Result::eSuccess;
    }

    void VulkanDevice::present(std::shared_ptr<prototypes::SwapchainImpl> swapchain, std::shared_ptr<SemaphoreImpl> renderingFinished)
    {
      auto native = std::static_pointer_cast<VulkanSwapchain>(swapchain);
      uint32_t index = static_cast<uint32_t>(native->getCurrentPresentableImageIndex());
      vk::Semaphore renderFinish = nullptr;
      uint32_t semaphoreCount = 0;
      vk::SwapchainKHR swap = native->native();
      if (renderingFinished)
      {
        auto sema = std::static_pointer_cast<VulkanSemaphore>(renderingFinished);
        renderFinish = sema->native();
        semaphoreCount = 1;
      }

      try
      {
        m_mainQueue.presentKHR(vk::PresentInfoKHR()
          .setSwapchainCount(1)
          .setPSwapchains(&swap)
          .setPImageIndices(&index)
          .setWaitSemaphoreCount(semaphoreCount)
          .setPWaitSemaphores(&renderFinish));
      }
      catch (...)
      {
      }
    };
  }
}