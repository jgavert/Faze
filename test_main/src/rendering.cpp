#include "rendering.hpp"

#include <higanbana/core/profiling/profiling.hpp>

#include <imgui.h>
#include <execution>
#include <algorithm>

using namespace higanbana;

namespace app
{
int Renderer::loadMesh(MeshData& data) {
  return meshes.allocate(dev, worldRend.meshArgLayout(), worldMeshRend.meshArgLayout(), data);
}

void Renderer::unloadMesh(int index) {
  meshes.free(index);
}

int Renderer::loadTexture(TextureData& data) {
  return textures.allocate(dev, data.image);
}
void Renderer::unloadTexture(int index) {
  textures.free(index);
}

int Renderer::loadMaterial(MaterialData& material) {
  //materials.allocate()
  return 0;
}
void Renderer::unloadMaterial(int index) {
  //return materials.free(index);
}

Renderer::Renderer(higanbana::GraphicsSubsystem& graphics, higanbana::GpuGroup& dev)
  : graphics(graphics)
  , dev(dev)
  , m_camerasLayout(dev.createShaderArgumentsLayout(higanbana::ShaderArgumentsLayoutDescriptor()
      .readOnly<CameraSettings>(ShaderResourceType::StructuredBuffer, "cameras")))
  , cubes(dev)
  , imgui(dev)
  , worldRend(dev, m_camerasLayout)
  , worldMeshRend(dev, m_camerasLayout)
  , tsaa(dev)
  , blitter(dev)
  , genImage(dev, "simpleEffectAssyt", uint3(8,8,1))
  , particleSimulation(dev, m_camerasLayout) {
  scdesc = SwapchainDescriptor()
    .formatType(FormatType::Unorm8RGBA)
    .colorspace(Colorspace::BT709)
    .bufferCount(3).presentMode(PresentMode::Mailbox);

  auto basicDescriptor = GraphicsPipelineDescriptor()
    .setVertexShader("Triangle")
    .setPixelShader("Triangle")
    .setPrimitiveTopology(PrimitiveTopology::Triangle)
    .setRTVFormat(0, FormatType::Unorm8BGRA)
    .setRenderTargetCount(1)
    .setDepthStencil(DepthStencilDescriptor()
      .setDepthEnable(false));

  size_t textureSize = 1280 * 720;

  cameras = dev.createBuffer(ResourceDescriptor()
  .setCount(10)
  .setStructured<CameraSettings>()
  .setName("Cameras")
  .setUsage(ResourceUsage::GpuRW));
  cameraSRV = dev.createBufferSRV(cameras);
  cameraUAV = dev.createBufferUAV(cameras);


  cameraArgs = dev.createShaderArguments(ShaderArgumentsDescriptor("camera and other static data", m_camerasLayout)
    .bind("cameras", cameraSRV));
  ResourceDescriptor desc = ResourceDescriptor()
    .setSize(uint3(1280, 720, 1))
    .setFormat(FormatType::Unorm8BGRA);
  resizeExternal(desc);
  resizeInternal(desc.setSize(uint3(960, 540, 1)));
  time.startFrame();
}

void Renderer::initWindow(higanbana::Window& window, higanbana::GpuInfo info) {
  surface = graphics.createSurface(window, info);
  swapchain = dev.createSwapchain(surface, scdesc);
  resizeExternal(swapchain.buffers().front().texture().desc());
}

int2 Renderer::windowSize() {
  if (swapchain.buffers().empty())
    return int2(1,1);
  return swapchain.buffers().front().desc().desc.size3D().xy();
}

void Renderer::resizeInternal(higanbana::ResourceDescriptor& desc) {
  m_gbuffer = dev.createTexture(higanbana::ResourceDescriptor()
    .setSize(desc.desc.size3D())
    .setFormat(FormatType::Unorm16RGBA)
    .setUsage(ResourceUsage::RenderTargetRW)
    .setName("gbuffer"));
  m_gbufferRTV = dev.createTextureRTV(m_gbuffer);
  m_gbufferSRV = dev.createTextureSRV(m_gbuffer);
  m_depth = dev.createTexture(higanbana::ResourceDescriptor()
    .setSize(desc.desc.size3D())
    .setFormat(FormatType::Depth32)
    .setUsage(ResourceUsage::DepthStencil)
    .setName("opaqueDepth"));

  m_depthDSV = dev.createTextureDSV(m_depth);

  proxyTex.resize(dev, ResourceDescriptor()
    .setSize(desc.desc.size3D())
    .setFormat(desc.desc.format)
    .setUsage(ResourceUsage::RenderTargetRW)
    .setName("proxyTex"));
}
void Renderer::resizeExternal(higanbana::ResourceDescriptor& desc) {
  tsaaResolved.resize(dev, ResourceDescriptor()
    .setSize(desc.desc.size3D())
    .setFormat(FormatType::Unorm16RGBA)
    .setUsage(ResourceUsage::RenderTargetRW)
    .setName("tsaa current/history"));

  auto descTsaaResolved = tsaaResolved.desc();
  tsaaDebug = dev.createTexture(descTsaaResolved);
  tsaaDebugSRV = dev.createTextureSRV(tsaaDebug);
  tsaaDebugUAV = dev.createTextureUAV(tsaaDebug);

  depthExternal = dev.createTexture(higanbana::ResourceDescriptor()
    .setSize(desc.desc.size3D())
    .setFormat(FormatType::Depth32)
    .setUsage(ResourceUsage::DepthStencil)
    .setName("opaqueDepth"));

  depthExternalDSV = dev.createTextureDSV(depthExternal);
}

std::optional<higanbana::SubmitTiming> Renderer::timings() {
  auto info = dev.submitTimingInfo();
  if (!info.empty())
    m_previousInfo = info.front();
  
  return m_previousInfo;
}

void Renderer::renderMeshes(higanbana::CommandGraphNode& node, higanbana::TextureRTV& backbuffer, higanbana::TextureDSV& depth, int cameraIndex, higanbana::vector<InstanceDraw>& instances) {
  backbuffer.setOp(LoadOp::Load);
  depth.clearOp({});
  worldRend.beginRenderpass(node, backbuffer, depth);
  for (auto&& instance : instances)
  {
    auto& mesh = meshes[instance.meshId];
    worldRend.renderMesh(node, mesh.indices, cameraArgs, mesh.args, cameraIndex);
  }
  worldRend.endRenderpass(node);
}

void Renderer::renderMeshesWithMeshShaders(higanbana::CommandGraphNode& node, higanbana::TextureRTV& backbuffer, higanbana::TextureDSV& depth, int cameraIndex, higanbana::vector<InstanceDraw>& instances) {
  backbuffer.setOp(LoadOp::Load);
  depth.clearOp({});
  
  worldMeshRend.beginRenderpass(node, backbuffer, depth);
  for (auto&& instance : instances)
  {
    auto& mesh = meshes[instance.meshId];
    worldMeshRend.renderMesh(node, mesh.indices, cameraArgs, mesh.meshArgs, mesh.meshlets.desc().desc.width, cameraIndex);
  }
  node.endRenderpass();
}

void Renderer::renderScene(higanbana::CommandGraph& tasks, higanbana::TextureRTV gbufferRTV, higanbana::TextureDSV depth, RendererOptions options, int cameraIdx, const float4x4& perspective, float3 cameraPos, higanbana::vector<InstanceDraw>& instances, int drawcalls, int drawsSplitInto, std::optional<higanbana::CpuImage>& heightmap) {
  {
    auto node = tasks.createPass("composite");
    node.acquirePresentableImage(swapchain);
    float redcolor = std::sin(time.getFTime())*.5f + .5f;

    gbufferRTV.clearOp(float4{ 0.f, redcolor, 0.f, 0.2f });
    blitter.beginRenderpass(node, gbufferRTV);
    blitter.blitImage(dev, node, proxyTex.srv(), renderer::Blitter::FitMode::Fill);
    node.endRenderpass();
    tasks.addPass(std::move(node));
  }

  {
    HIGAN_CPU_BRACKET("user - outerloop");
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

    auto args = dev.createShaderArguments(ShaderArgumentsDescriptor("Opaque Arguments", cubes.getLayout())
      .bind("vertexInput", vert));
    if (heightmap && instances.empty())
    {
      int pixelsToDraw = drawcalls;
      gbufferRTV.setOp(LoadOp::Load);
      depth.clearOp({});
      
      vector<std::tuple<CommandGraphNode, int, int>> nodes;
      int stepSize = std::max(1, int((float(pixelsToDraw+1) / float(drawsSplitInto))+0.5f));
      for (int i = 0; i < pixelsToDraw; i+=stepSize)
      {
        if (i+stepSize > pixelsToDraw)
        {
          stepSize = stepSize - (i+stepSize - pixelsToDraw);
        }
        nodes.push_back(std::make_tuple(tasks.createPass("opaquePass - cubes"), i, stepSize));
      }


      std::for_each(std::execution::par_unseq, std::begin(nodes), std::end(nodes), [&](std::tuple<CommandGraphNode, int, int>& node)
      {
        HIGAN_CPU_BRACKET("user - innerloop");
        auto ldepth = depth;
        if (std::get<1>(node) == 0)
          ldepth.setOp(LoadOp::Clear);
        else
          ldepth.setOp(LoadOp::Load);
        
        cubes.drawHeightMapInVeryStupidWay2(dev, time.getFTime(), std::get<0>(node), cameraPos, perspective, gbufferRTV, ldepth, heightmap.value(), ind, args, pixelsToDraw, std::get<1>(node), std::get<1>(node)+std::get<2>(node));
      });

      for (auto& node : nodes)
      {
        tasks.addPass(std::move(std::get<0>(node)));
      }
    }
    else if (instances.empty())
    {
      gbufferRTV.setOp(LoadOp::Load);
      depth.clearOp({});
      
      vector<std::tuple<CommandGraphNode, int, int>> nodes;
      if (!options.unbalancedCubes)
      {
        int stepSize = std::max(1, int((float(drawcalls+1) / float(drawsSplitInto))+0.5f));
        for (int i = 0; i < drawcalls; i+=stepSize)
        {
          if (i+stepSize > drawcalls)
          {
            stepSize = stepSize - (i+stepSize - drawcalls);
          }
          nodes.push_back(std::make_tuple(tasks.createPass("opaquePass - cubes"), i, stepSize));
        }
      }
      else
      {
        int cubesLeft = drawcalls;
        int cubesDrawn = 0;
        vector<int> reverseDraw;
        for (int i = 0; i < drawsSplitInto-1; i++)
        {
          auto split = cubesLeft/2;
          cubesLeft -= split;
          reverseDraw.emplace_back(split);
        }
        reverseDraw.push_back(cubesLeft);
        for (int i = reverseDraw.size()-1; i >= 0; i--)
        {
          nodes.push_back(std::make_tuple(tasks.createPass("opaquePass - cubes"), cubesDrawn, reverseDraw[i]));
          cubesDrawn += reverseDraw[i];
        } 
      }
      std::for_each(std::execution::par_unseq, std::begin(nodes), std::end(nodes), [&](std::tuple<CommandGraphNode, int, int>& node)
      {
        HIGAN_CPU_BRACKET("user - innerloop");
        auto ldepth = depth;
        if (std::get<1>(node) == 0)
          ldepth.setOp(LoadOp::Clear);
        else
          ldepth.setOp(LoadOp::Load);
        
        cubes.oldOpaquePass2(dev, time.getFTime(), std::get<0>(node), perspective, gbufferRTV, ldepth, ind, args, drawcalls, std::get<1>(node),  std::get<1>(node)+std::get<2>(node));
      });

      for (auto& node : nodes)
      {
        tasks.addPass(std::move(std::get<0>(node)));
      }
    }
    else
    {
      auto node = tasks.createPass("opaquePass - ecs");
      if (options.allowMeshShaders)
        renderMeshesWithMeshShaders(node, gbufferRTV, depth, cameraIdx, instances);
      else
        renderMeshes(node, gbufferRTV, depth, cameraIdx, instances);
      tasks.addPass(std::move(node));
    }
  }

  // particles draw
  {
    auto node = tasks.createPass("particles - draw");
    particleSimulation.render(dev, node, gbufferRTV, depth, cameraArgs);
    tasks.addPass(std::move(node));
  }
}


void Renderer::render(LBS& lbs, RendererOptions options, ActiveCamera camera, higanbana::vector<InstanceDraw>& instances, int drawcalls, int drawsSplitInto, std::optional<higanbana::CpuImage>& heightmap) {
  if (swapchain.outOfDate())
  {
    dev.adjustSwapchain(swapchain, scdesc);
    resizeExternal(swapchain.buffers().begin()->texture().desc());
  }

  int2 currentRes = math::mul(options.resolutionScale, float2(swapchain.buffers().begin()->texture().desc().desc.size3D().xy()));
  if (currentRes.x > 0 && currentRes.y > 0 && (currentRes.x != m_gbuffer.desc().desc.size3D().x || currentRes.y != m_gbuffer.desc().desc.size3D().y))
  {
    auto desc = ResourceDescriptor(m_gbuffer.desc()).setSize(currentRes);
    resizeInternal(desc);
  }

  CommandGraph tasks = dev.createGraph();

  float4x4 perspective;
  {
    auto ndoe = tasks.createPass("copy cameras");
    vector<CameraSettings> sets;
    auto& swapDesc = swapchain.buffers().front().desc().desc;
    auto aspect = float(swapDesc.height)/float(swapDesc.width);
    float4x4 pers = math::perspectivelh(camera.fov, aspect, camera.minZ, camera.maxZ);
    float4x4 rot = math::rotationMatrixLH(camera.direction);
    float4x4 pos = math::translation(camera.position);
    perspective = math::mul(pos, math::mul(rot, pers));
    if (options.jitterEnabled){
      perspective = tsaa.jitterProjection(time.getFrame(), swapDesc.size3D().xy(), perspective);
    }
    sets.push_back(CameraSettings{perspective});
    auto matUpdate = dev.dynamicBuffer<CameraSettings>(makeMemView(sets));
    ndoe.copy(cameras, matUpdate);
    tasks.addPass(std::move(ndoe));
  }

  {
    auto node = tasks.createPass("generate Texture");
    genImage.generate(dev, node, proxyTex.uav());
    tasks.addPass(std::move(node));
  }

  {
    auto ndoe = tasks.createPass("simulate particles");
    particleSimulation.simulate(dev, ndoe, time.getFrameTimeDelta());
    tasks.addPass(std::move(ndoe));
  }

  renderScene(tasks, m_gbufferRTV, m_depthDSV, options, 0, perspective, camera.position, instances, drawcalls, drawsSplitInto, heightmap);

  {
    auto tsaaNode = tasks.createPass("Temporal Supersampling AA");
    tsaaResolved.next();
    tsaa.resolve(dev, tsaaNode, tsaaResolved.uav(), renderer::TSAAArguments{m_gbufferSRV, tsaaResolved.previousSrv(), m_gbufferSRV, tsaaDebugUAV});
    tasks.addPass(std::move(tsaaNode));
  }

  // If you acquire, you must submit it.
  std::optional<std::pair<int,TextureRTV>> obackbuffer = dev.acquirePresentableImage(swapchain);
  if (!obackbuffer.has_value())
  {
    HIGAN_LOGi( "No backbuffer available...\n");
    dev.submit(tasks);
    time.tick();
    return;
  }
  TextureRTV backbuffer = obackbuffer.value().second;

  {
    auto node = tasks.createPass("copy tsaa resolved to backbuffer");
    blitter.beginRenderpass(node, backbuffer);
    auto target = tsaaResolved.srv();
    if (options.tsaaDebug)
      target = tsaaDebugSRV;
    blitter.blitImage(dev, node, target, renderer::Blitter::FitMode::Fill);
    node.endRenderpass();
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

  if (options.submitSingleThread)
    dev.submitExperimental(swapchain, tasks, ThreadedSubmission::Sequenced);
  else if (options.submitExperimental)
    dev.submitExperimental(swapchain, tasks, ThreadedSubmission::ParallelUnsequenced);
  else if (options.submitLBS)
    dev.submitLBS(lbs, swapchain, tasks, ThreadedSubmission::ParallelUnsequenced);
  else 
    dev.submit(swapchain, tasks);
  {
    HIGAN_CPU_BRACKET("Present");
    dev.present(swapchain, obackbuffer.value().first);
  }
  time.tick();
}
}