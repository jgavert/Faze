#include "rendering.hpp"

#include <imgui.h>

using namespace higanbana;

SHADER_STRUCT(PixelConstants,
  float resx;
  float resy;
  float time;
  int unused;
);

SHADER_STRUCT(ComputeConstants,
  float resx;
  float resy;
  float time;
  int unused;
);

SHADER_STRUCT(OpaqueConsts,
  float resx;
  float resy;
  float time;
  int unused;
  float4x4 worldMat;
  float4x4 viewMat;
);

namespace app
{
  int MeshSystem::allocate(higanbana::GpuGroup& gpu, higanbana::ShaderArgumentsLayout& meshLayout, MeshData& data)
  {
    auto val = freelist.allocate();
    if (views.size() < val+1) views.resize(val+1);

    auto sizeInfo = higanbana::formatSizeInfo(data.indiceFormat);
    auto& view = views[val];
    view.indices = gpu.createBufferIBV(ResourceDescriptor()
    .setFormat(data.indiceFormat)
    .setCount(data.indices.size() / sizeInfo.pixelSize)
    .setUsage(ResourceUsage::GpuReadOnly)
    .setIndexBuffer());

    sizeInfo = higanbana::formatSizeInfo(data.vertexFormat);
    view.vertices = gpu.createBufferSRV(ResourceDescriptor()
    .setFormat(data.vertexFormat)
    .setCount(data.vertices.size() / sizeInfo.pixelSize)
    .setUsage(ResourceUsage::GpuReadOnly));

    view.args = gpu.createShaderArguments(higanbana::ShaderArgumentsDescriptor("World shader layout",meshLayout)
      .bind("vertices", view.vertices));

    auto graph = gpu.createGraph();
    auto node = graph.createPass("Update mesh data");
    auto indData = gpu.dynamicBuffer(makeMemView(data.indices), data.indiceFormat);
    node.copy(view.indices.buffer(), indData);
    auto vertData = gpu.dynamicBuffer(makeMemView(data.vertices), data.vertexFormat);
    node.copy(view.vertices.buffer(), vertData);
    graph.addPass(std::move(node));
    gpu.submit(graph);
    return val;
  }

  void MeshSystem::free(int index)
  {
    views[index] = {};
  }

  int Renderer::loadMesh(MeshData& data)
  {
    return meshes.allocate(dev, worldRend.meshArgLayout(), data);
  }

  void Renderer::unloadMesh(int index)
  {
    return meshes.free(index);
  }

  Renderer::Renderer(higanbana::GraphicsSubsystem& graphics, higanbana::GpuGroup& dev)
   : graphics(graphics)
   , dev(dev)
   , imgui(dev)
   , worldRend(dev)
  {
    scdesc = SwapchainDescriptor()
      .formatType(FormatType::Unorm8RGBA)
      .colorspace(Colorspace::BT709)
      .bufferCount(3).presentMode(PresentMode::Mailbox);
      //.bufferCount(2).presentMode(PresentMode::FifoRelaxed);

    higanbana::ShaderArgumentsLayoutDescriptor triangleLayoutDesc = ShaderArgumentsLayoutDescriptor()
      .readOnly(ShaderResourceType::ByteAddressBuffer, "vertexInput");
    triangleLayout = dev.createShaderArgumentsLayout(triangleLayoutDesc);
    higanbana::PipelineInterfaceDescriptor babyInf = PipelineInterfaceDescriptor()
      .constants<PixelConstants>()
      .shaderArguments(0, triangleLayout);

    auto basicDescriptor = GraphicsPipelineDescriptor()
      .setVertexShader("Triangle")
      .setPixelShader("Triangle")
      .setInterface(babyInf)
      .setPrimitiveTopology(PrimitiveTopology::Triangle)
      .setRTVFormat(0, FormatType::Unorm8BGRA)
      .setRenderTargetCount(1)
      .setDepthStencil(DepthStencilDescriptor()
        .setDepthEnable(false));
    
    triangle = dev.createGraphicsPipeline(basicDescriptor);

    triangleRP = dev.createRenderpass();

    higanbana::PipelineInterfaceDescriptor opaquePassInterface = PipelineInterfaceDescriptor()
      .constants<OpaqueConsts>()
      .shaderArguments(0, triangleLayout);

    auto opaqueDescriptor = GraphicsPipelineDescriptor()
      .setVertexShader("opaquePass")
      .setPixelShader("opaquePass")
      .setInterface(opaquePassInterface)
      .setPrimitiveTopology(PrimitiveTopology::Triangle)
      .setRasterizer(RasterizerDescriptor())
      .setRTVFormat(0, FormatType::Unorm8BGRA)
      .setDSVFormat(FormatType::Depth32)
      .setRenderTargetCount(1)
      .setDepthStencil(DepthStencilDescriptor()
        .setDepthEnable(true)
        .setDepthFunc(ComparisonFunc::Greater));
    
    opaque = dev.createGraphicsPipeline(opaqueDescriptor);

    opaqueRP = dev.createRenderpass();

    depth = dev.createTexture(higanbana::ResourceDescriptor()
      .setSize(uint2(1280, 720))
      .setFormat(FormatType::Depth32)
      .setUsage(ResourceUsage::DepthStencil)
      .setName("opaqueDepth"));
    depthDSV = dev.createTextureDSV(depth);

    proxyTex.resize(dev, ResourceDescriptor()
      .setSize(uint2(1280, 720))
      .setFormat(FormatType::Unorm8RGBA)
      .setUsage(ResourceUsage::RenderTargetRW));

    higanbana::ShaderArgumentsLayoutDescriptor layoutdesc = ShaderArgumentsLayoutDescriptor()
      .readWrite(ShaderResourceType::Texture2D, "float4", "output");
    compLayout = dev.createShaderArgumentsLayout(layoutdesc);
    higanbana::PipelineInterfaceDescriptor babyInf2 = PipelineInterfaceDescriptor()
      .constants<ComputeConstants>()
      .shaderArguments(0, compLayout);

    genTexCompute = dev.createComputePipeline(ComputePipelineDescriptor()
    .setInterface(babyInf2)
    .setShader("simpleEffectAssyt")
    .setThreadGroups(uint3(8, 4, 1)));

    higanbana::ShaderArgumentsLayoutDescriptor layoutdesc2 = ShaderArgumentsLayoutDescriptor()
      .readOnly(ShaderResourceType::ByteAddressBuffer, "vertexInput")
      .readOnly(ShaderResourceType::Texture2D, "float4", "texInput");
    blitLayout = dev.createShaderArgumentsLayout(layoutdesc2);
    composite = dev.createGraphicsPipeline(basicDescriptor
      .setVertexShader("blit")
      .setPixelShader("blit")
      .setInterface(PipelineInterfaceDescriptor()
        .constants<PixelConstants>()
        .shaderArguments(0, blitLayout)));
    compositeRP = dev.createRenderpass();

    targetRT.resize(dev, ResourceDescriptor()
      .setSize(uint2(1280, 720))
      .setFormat(FormatType::Unorm8RGBA)
      .setUsage(ResourceUsage::RenderTarget));

    size_t textureSize = 1280 * 720;

    sBuf = dev.createBuffer(ResourceDescriptor()
      .setCount(textureSize)
      .setFormat(FormatType::Raw32)
      .setUsage(ResourceUsage::GpuRW));
      //.allowCrossAdapter(1));

    sBufSRV = dev.createBufferSRV(sBuf);
     
     /*
    sTex = dev.createTexture(ResourceDescriptor()
      .setSize(uint2(1280, 720))
      .setFormat(FormatType::Unorm8RGBA)
      .setUsage(ResourceUsage::GpuReadOnly)
      .allowCrossAdapter(1));
      */
    
    cameras = dev.createBuffer(ResourceDescriptor()
    .setCount(10)
    .setStructured<CameraSettings>()
    .setName("Cameras")
    .setUsage(ResourceUsage::GpuRW));
    cameraSRV = dev.createBufferSRV(cameras);
    cameraUAV = dev.createBufferUAV(cameras);


    staticDataArgs = dev.createShaderArguments(ShaderArgumentsDescriptor("camera and other static data", worldRend.staticDataLayout())
      .bind("cameras", cameraSRV));


    position = { -10.f, 0.f, -10.f };
    dir = { 1.f, 0.f, 0.f };
    updir = { 0.f, 1.f, 0.f };
    sideVec = { 0.f, 0.f, 1.f };
    direction = { 1.f, 0.f, 0.f, 0.f };

    time.startFrame();
  }

  void Renderer::initWindow(higanbana::Window& window, higanbana::GpuInfo info)
  {
    surface = graphics.createSurface(window, info);
    swapchain = dev.createSwapchain(surface, scdesc);
  }

  int2 Renderer::windowSize()  {
    if (swapchain.buffers().empty())
      return int2(1,1);
    return swapchain.buffers().front().desc().desc.size3D().xy();
  }

  void Renderer::windowResized()
  {
    dev.adjustSwapchain(swapchain, scdesc);

    auto& desc = swapchain.buffers().front().desc();
    proxyTex.resize(dev, ResourceDescriptor()
      .setSize(desc.desc.size3D())
      .setFormat(desc.desc.format)
      .setUsage(ResourceUsage::RenderTargetRW)
      .setName("proxyTex"));

    targetRT.resize(dev, ResourceDescriptor()
      .setSize(desc.desc.size3D())
      .setFormat(desc.desc.format)
      .setUsage(ResourceUsage::RenderTarget)
      .setName("targetRT"));

    depth = dev.createTexture(higanbana::ResourceDescriptor()
      .setSize(desc.desc.size3D())
      .setFormat(FormatType::Depth32)
      .setUsage(ResourceUsage::DepthStencil)
      .setName("opaqueDepth"));

    depthDSV = dev.createTextureDSV(depth);
  }

  std::optional<higanbana::SubmitTiming> Renderer::timings()
  {
    auto info = dev.submitTimingInfo();
    if (!info.empty())
      m_previousInfo = info.front();
    
    return m_previousInfo;
  }

  void Renderer::oldOpaquePass(higanbana::CommandGraphNode& node, higanbana::TextureRTV& backbuffer)
  {
    float redcolor = std::sin(time.getFTime())*.5f + .5f;

    //backbuffer.clearOp(float4{ 0.f, redcolor, 0.f, 1.f });
    backbuffer.setOp(LoadOp::Load);
    depthDSV.clearOp({});
    node.renderpass(opaqueRP, backbuffer, depthDSV);
    {
      auto binding = node.bind(opaque);

      vector<float> vertexData = {
        1.0f, -1.f, -1.f,
        1.0f, -1.f, 1.f, 
        1.0f, 1.f, -1.f,
        1.0f, 1.f, 1.f,
        -1.0f, -1.f, -1.f,
        -1.0f, -1.f, 1.f, 
        -1.0f, 1.f, -1.f,
        -1.0f, 1.f, 1.f,
      };

      auto vert = dev.dynamicBuffer<float>(vertexData, FormatType::Raw32);
      vector<uint16_t> indexData = {
        1, 0, 2,
        1, 2, 3,
        5, 4, 0,
        5, 0, 1,
        7, 2, 6,
        7, 3, 2,
        5, 6, 4,
        5, 7, 6,
        6, 0, 4,
        6, 2, 0,
        7, 5, 1,
        7, 1, 3,
      };
      auto ind = dev.dynamicBuffer<uint16_t>(indexData, FormatType::Uint16);

      quaternion yaw = math::rotateAxis(updir, 0.001f);
      quaternion pitch = math::rotateAxis(sideVec, 0.f);
      quaternion roll = math::rotateAxis(dir, 0.f);
      direction = math::mul(math::mul(math::mul(yaw, pitch), roll), direction);

      auto rotationMatrix = math::rotationMatrixLH(direction);
      auto aspect = float(backbuffer.desc().desc.height)/float(backbuffer.desc().desc.width);
      auto perspective = math::mul(math::translation(3, 0, 8), math::perspectivelh(90.f, aspect, 0.1f, 100.f));

      auto worldMat = math::mul(math::scale(0.2f), rotationMatrix);

      OpaqueConsts consts{};
      consts.time = time.getFTime();
      consts.resx = backbuffer.desc().desc.width; 
      consts.resy = backbuffer.desc().desc.height;
      consts.worldMat = math::mul(worldMat, math::translation(0,0,0));
      consts.viewMat = perspective;
      binding.constants(consts);

      auto args = dev.createShaderArguments(ShaderArgumentsDescriptor("Opaque Arguments", triangleLayout)
        .bind("vertexInput", vert));

      binding.arguments(0, args);

      int gridSize = 8;
      for (int x = 0; x < gridSize; ++x)
      {
        for (int y = 0; y < gridSize; ++y)
        {
          for (int z = 0; z < gridSize; ++z)
          {
            float offset = gridSize*1.5f/2.f;
            float3 pos = float3(x*1.5f-offset, y*1.5f-offset, z*1.5f-offset);
            consts.worldMat = math::mul2(worldMat, math::translation(pos));
            binding.constants(consts);
            node.drawIndexed(binding, ind, 36);
          }
        }
      }
    }
    node.endRenderpass();
  }

  void Renderer::renderMeshes(higanbana::CommandGraphNode& node, higanbana::TextureRTV& backbuffer, higanbana::vector<InstanceDraw>& instances)
  {
    // config matrices...
    quaternion yaw = math::rotateAxis(updir, 0.001f);
    quaternion pitch = math::rotateAxis(sideVec, 0.f);
    quaternion roll = math::rotateAxis(dir, 0.f);
    direction = math::mul(math::mul(math::mul(yaw, pitch), roll), direction);

    auto rotationMatrix = math::rotationMatrixLH(direction);
    auto aspect = float(backbuffer.desc().desc.height)/float(backbuffer.desc().desc.width);
    auto perspective = math::mul(math::translation(3, 0, 8), math::perspectivelh(90.f, aspect, 0.1f, 100.f));

    // upload data to gpu :P
    vector<CameraSettings> sets;
    sets.push_back(CameraSettings{perspective});
    auto matUpdate = dev.dynamicBuffer<CameraSettings>(makeMemView(sets));
    node.copy(cameras, matUpdate);


    backbuffer.setOp(LoadOp::Load);
    depthDSV.clearOp({});
    worldRend.beginRenderpass(node, backbuffer, depthDSV);
    for (auto&& instance : instances)
    {
      auto& mesh = meshes[instance.meshId];
      worldRend.renderMesh(node, mesh.indices, staticDataArgs, mesh.args);
    }
    worldRend.endRenderpass(node);
  }

  void Renderer::render(higanbana::vector<InstanceDraw>& instances)
  {
    if (swapchain.outOfDate()) // swapchain can end up being outOfDate
    {
      windowResized();
    }

    // If you acquire, you must submit it.
    std::optional<TextureRTV> obackbuffer = dev.acquirePresentableImage(swapchain);
    if (!obackbuffer.has_value())
    {
      HIGAN_LOGi( "No backbuffer available, Resizing\n");
      return;
    }
    TextureRTV backbuffer = obackbuffer.value();
    CommandGraph tasks = dev.createGraph();

    {
      auto node = tasks.createPass("generate Texture");

      auto args = dev.createShaderArguments(ShaderArgumentsDescriptor("Generate Texture Descriptors", compLayout)
        .bind("output", proxyTex.uav()));

      auto binding = node.bind(genTexCompute);
      ComputeConstants consts{};
      consts.time = time.getFTime();
      consts.resx = proxyTex.desc().desc.width; 
      consts.resy = proxyTex.desc().desc.height;
      binding.constants(consts);
      binding.arguments(0, args);

      node.dispatchThreads(binding, proxyTex.desc().desc.size3D());


      tasks.addPass(std::move(node));
    }

    {
      auto node = tasks.createPass("composite");
      node.acquirePresentableImage(swapchain);
      backbuffer.setOp(LoadOp::DontCare);
      float redcolor = std::sin(time.getFTime())*.5f + .5f;

      backbuffer.clearOp(float4{ 0.f, redcolor, 0.f, 0.2f });
      node.renderpass(compositeRP, backbuffer);
      {
        auto binding = node.bind(composite);

        PixelConstants consts{};
        consts.time = time.getFTime();
        consts.resx = backbuffer.desc().desc.width; 
        consts.resy = backbuffer.desc().desc.height;
        binding.constants(consts);

        vector<float> vertexData = {
          -1.f, -1.f, 
          -1.0f, 3.f,
          3.0f, -1.0f};
        auto vert = dev.dynamicBuffer<float>(vertexData, FormatType::Raw32);

        auto args = dev.createShaderArguments(ShaderArgumentsDescriptor("blit descriptors", blitLayout)
          .bind("vertexInput", vert)
          .bind("texInput", proxyTex.srv()));

        binding.arguments(0, args);

        node.draw(binding, 3);
      }
      node.endRenderpass();
      tasks.addPass(std::move(node));
    }

    {
      auto node = tasks.createPass("opaquePass");
      if (instances.empty())
        oldOpaquePass(node, backbuffer);
      else
        renderMeshes(node, backbuffer, instances);
      
      tasks.addPass(std::move(node));
    }

    // IMGUI
    {
      auto node = tasks.createPass("IMGui");
      imgui.render(dev, node, backbuffer);
      tasks.addPass(std::move(node));
    }
    
    {
      auto node = tasks.createPass("preparePresent");

      node.prepareForPresent(backbuffer);
      tasks.addPass(std::move(node));
    }

    dev.submit(swapchain, tasks);
    dev.present(swapchain);
    time.tick();
  }
}