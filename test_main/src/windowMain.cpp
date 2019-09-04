#include "windowMain.hpp"
#include <higanbana/core/system/LBS.hpp>
#include <higanbana/core/filesystem/filesystem.hpp>
#include <higanbana/core/Platform/Window.hpp>
#include <higanbana/core/system/logger.hpp>
#include <higanbana/core/system/time.hpp>
#include <higanbana/core/global_debug.hpp>
#include <higanbana/core/entity/bitfield.hpp>
#include <higanbana/core/system/misc.hpp>
#include <higanbana/core/system/time.hpp>
#include <higanbana/core/system/AtomicBuffered.hpp>
#include <higanbana/graphics/GraphicsCore.hpp>
#include <random>
#include <tuple>
#include <imgui.h>

#include "entity_test.hpp"
#include "rendering.hpp"

using namespace higanbana;
using namespace higanbana::math;

vector<std::string> splitByDelimiter(std::string data, const char* delimiter)
{
  vector<std::string> splits;
  
  size_t pos = 0;
  std::string token;
  while ((pos = data.find(delimiter)) != std::string::npos) {
    token = data.substr(0, pos);
    splits.emplace_back(token);
    data.erase(0, pos + strlen(delimiter));
  }
  return splits;
}

vector<int> convertToInts(vector<std::string> data)
{
  vector<int> ints;
  for (auto&& s : data)
  {
    try
    {
      auto val = std::stoi(s.c_str());
      ints.push_back(val);
    } catch (...)
    {
    }
  }
  return ints;
}

void mainWindow(ProgramParams& params)
{
  GraphicsApi cmdlineApiId = GraphicsApi::DX12;
  VendorID cmdlineVendorId = VendorID::Nvidia;
  auto cmdLength = strlen(params.m_lpCmdLine);
  HIGAN_LOGi( "cmdline: \"%s\"", params.m_lpCmdLine);
  std::string cmdline = params.m_lpCmdLine;
  std::replace(cmdline.begin(), cmdline.end(), '"', ' ');
  std::replace(cmdline.begin(), cmdline.end(), '\\', ' ');
  HIGAN_LOGi( "cmdline: \"%s\"", cmdline.c_str());
  auto splits = splitByDelimiter(cmdline, " ");
  auto ints = convertToInts(splits);
  if (ints.size() > 0)
  {
    cmdlineApiId = static_cast<GraphicsApi>(ints[0]); 
  }
  if (ints.size() > 1)
  {
    cmdlineVendorId = static_cast<VendorID>(ints[1]); 
  }
  Logger log;
  // test_entity();
  // test_bitfield();
  auto main = [&](GraphicsApi api, VendorID preferredVendor, bool updateLog)
  {
    HIGAN_LOG("Trying to start with %s api and %s vendor\n", toString(api), toString(preferredVendor));
    bool reInit = false;
    int64_t frame = 1;
    FileSystem fs("/../../data");
    bool quit = false;
    LBS lbs;

    log.update();

    bool explicitID = false;
    //GpuInfo gpuinfo{};

    WTime time;

    while (true)
    {
      vector<GpuInfo> allGpus;
      GraphicsSubsystem graphics("higanbana", false);
      auto gpus = graphics.availableGpus();
#if 1
      auto gpuInfo = graphics.getVendorDevice(api, preferredVendor);
      auto api2 = GraphicsApi::Vulkan;
      if (api == GraphicsApi::Vulkan)
      {
        api2 = GraphicsApi::DX12;
      }
      auto gpuInfo2 = graphics.getVendorDevice(api2, preferredVendor);
      allGpus.emplace_back(gpuInfo);
      //allGpus.emplace_back(gpuInfo2);
#else
      if (!explicitID)
      {
        //gpuinfo = graphics.getVendorDevice(api);
        for (auto&& it : gpus)
        {
          if (it.api == api && it.vendor == preferredVendor)
          {
            allGpus.push_back(it);
            break;
          }
          HIGAN_LOG("\t%s: %d. %s (memory: %zdMB, api: %s)\n", toString(it.api), it.id, it.name.c_str(), it.memory/1024/1024, it.apiVersionStr.c_str());
        }
      }
      if (allGpus.empty())
      {
        for (auto&& it : gpus)
        {
          if (it.api == api)
          {
            allGpus.push_back(it);
            break;
          }
          HIGAN_LOG("\t%s: %d. %s (memory: %zdMB, api: %s)\n", toString(it.api), it.id, it.name.c_str(), it.memory/1024/1024, it.apiVersionStr.c_str());
        }
      }
#endif
      if (updateLog) log.update();
      if (gpus.empty())
        return;

	    std::string windowTitle = "";
      for (auto& gpu : allGpus)
      {
        windowTitle += std::string(toString(gpu.api)) + ": " + gpu.name + " ";
      }
      Window window(params, windowTitle, 1280, 720, 300, 200);
      window.open();

      // IMGUI
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      {
        ImGuiIO& io = ImGui::GetIO(); (void)io;
      }
      ImGui::StyleColorsDark();

      auto dev = graphics.createGroup(fs, allGpus);
      app::Renderer rend(graphics, dev);
      {
        auto toggleHDR = false;
        rend.initWindow(window, allGpus[0]);

        for (auto& gpu : allGpus)
        {
          HIGAN_LOG("Created device \"%s\"\n", gpu.name.c_str());
        }

        bool closeAnyway = false;
        bool renderActive = true;
        bool renderResize = false;
        bool captureMouse = false;
        bool controllerConnected = false;

        AtomicDoubleBuffered<InputBuffer> ainputs;
        ainputs.writeValue(window.inputs());
        AtomicDoubleBuffered<MouseState> amouse;
        amouse.writeValue(window.mouse());
        std::atomic<int> inputsUpdated = 0;
        std::atomic<int> inputsRead = 0;

        lbs.addTask("logic&render loop", [&](size_t) {
          while (renderActive)
          {
            ImGuiIO &io = ::ImGui::GetIO();
            auto windowSize = rend.windowSize();
            io.DisplaySize = { float(windowSize.x), float(windowSize.y) };
            time.tick();
            io.DeltaTime = time.getFrameTimeDelta();
            if (io.DeltaTime < 0.f)
              io.DeltaTime = 0.00001f;
            
            auto lastRead = inputsRead.load();
            auto inputs = ainputs.readValue();
            auto currentInput = inputs.frame();
            auto diff = currentInput - lastRead;
            if (diff > 0)
            {
              inputsRead.store(currentInput);
              auto mouse = amouse.readValue();

              if (mouse.captured)
              {
                io.MousePos.x = static_cast<float>(mouse.m_pos.x);
                io.MousePos.y = static_cast<float>(mouse.m_pos.y);

                io.MouseWheel = static_cast<float>(mouse.mouseWheel)*0.1f;
              }

              inputs.goThroughNFrames(diff, [&](Input i)
              {
                if (i.key >= 0)
                {
                  switch (i.key)
                  {
                  case VK_LBUTTON:
                  {
                    io.MouseDown[0] = (i.action > 0);
                    break;
                  }
                  case VK_RBUTTON:
                  {
                    io.MouseDown[1] = (i.action > 0);
                    break;
                  }
                  case VK_MBUTTON:
                  {
                    io.MouseDown[2] = (i.action > 0);
                    break;
                  }
                  default:
                    if (i.key < 512)
                      io.KeysDown[i.key] = (i.action > 0);
                    break;
                  }
                }
              });

              for (auto ch : window.charInputs())
                io.AddInputCharacter(static_cast<ImWchar>(ch));

              io.KeyCtrl = inputs.isPressedWithinNFrames(diff, VK_CONTROL, 2);
              io.KeyShift = inputs.isPressedWithinNFrames(diff, VK_SHIFT, 2);
              io.KeyAlt = inputs.isPressedWithinNFrames(diff, VK_MENU, 2);
              io.KeySuper = false;

              if (inputs.isPressedWithinNFrames(diff, VK_F1, 1))
              {
                window.captureMouse(true);
                captureMouse = true;
              }
              if (inputs.isPressedWithinNFrames(diff, VK_F2, 1))
              {
                window.captureMouse(false);
                captureMouse = false;
              }

              if (inputs.isPressedWithinNFrames(diff, VK_MENU, 2) && inputs.isPressedWithinNFrames(diff, '1', 1))
              {
                window.toggleBorderlessFullscreen();
              }

              if (inputs.isPressedWithinNFrames(diff, VK_MENU, 2) && inputs.isPressedWithinNFrames(diff, '2', 1))
              {
                reInit = true;
                if (api == GraphicsApi::DX12)
                  api = GraphicsApi::Vulkan;
                else
                  api = GraphicsApi::DX12;
                renderActive = false;
              }

              if (inputs.isPressedWithinNFrames(diff, VK_MENU, 2) && inputs.isPressedWithinNFrames(diff, '3', 1))
              {
                reInit = true;
                if (preferredVendor == VendorID::Amd)
                  preferredVendor = VendorID::Nvidia;
                else
                  preferredVendor = VendorID::Amd;
                renderActive = false;
              }

              if (frame > 10 && (closeAnyway || inputs.isPressedWithinNFrames(diff, VK_ESCAPE, 1)))
              {
                renderActive = false;
              }
            }
            ::ImGui::NewFrame();
            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
            ImGui::Text("Missed %zd frames of inputs. current: %zd read %zd", diff, currentInput, lastRead);

            ImGui::Text("average FPS %.2f (%.2fms)", 1000.f / time.getCurrentFps(), time.getCurrentFps());
            ImGui::Text("max FPS %.2f (%.2fms)", 1000.f / time.getMaxFps(), time.getMaxFps());

            auto si = rend.timings();
            if (si)
            {
              auto rsi = si.value();
              ImGui::Text("Graph solving %.3fms", rsi.graphSolve.milliseconds());
              ImGui::Text("Filling Lists %.3fms", rsi.fillCommandLists.milliseconds());
              ImGui::Text("Submitting Lists %.3fms", rsi.submitSolve.milliseconds());
              ImGui::Text("Submit total %.2fms", rsi.submitCpuTime.milliseconds());

              for (auto& cmdlist : rsi.lists)
              {
                ImGui::Text("\nCommandlist");
                ImGui::Text("\tbarrierSolve %.3fms", cmdlist.barrierSolve.milliseconds());
                ImGui::Text("\tfillNativeList %.3fms", cmdlist.fillNativeList.milliseconds());
                ImGui::Text("\tcpuBackendTime(?) %.3fms", cmdlist.cpuBackendTime.milliseconds());
                ImGui::Text("\ttotalTimeOnGPU %.3fms", cmdlist.fromSubmitToFence.milliseconds());
                ImGui::Text("\tGPU time %.3fms", cmdlist.gpuTime.milliseconds());
                ImGui::Text("\tGPU nodes:");
                for (auto& graphNode : cmdlist.nodes)
                {
                  ImGui::Text("\t\t%s %.3fms (%.3fms cpu)", graphNode.nodeName.c_str(), graphNode.gpuTime.milliseconds(), graphNode.cpuTime.milliseconds());
                }
              }
            }

            ImGui::End();
            ImGui::Render();
            //HIGAN_LOG("Rendering...");
            if (renderResize)
            {
              renderResize = false;
              rend.windowResized();
            }
            rend.render();
          }
        });
        while (!window.simpleReadMessages(frame++))
        {
          log.update();
          fs.updateWatchedFiles();
          if (window.hasResized())
          {
            renderResize = true;
            window.resizeHandled();
          }

          auto inputs = window.inputs();
          ainputs.writeValue(inputs);
          amouse.writeValue(window.mouse());
          inputsUpdated = inputs.frame();
          if (!renderActive)
            break;
        }
        renderActive = false;
        lbs.sleepTillKeywords({"logic&render loop"});
        dev.waitGpuIdle();
      }
      if (!reInit)
        break;
      else
      {
        reInit = false;
      }
    }
    quit = true;
  };
#if 0
  main(GraphicsApi::DX12, VendorID::Amd, true);
#else
  main(cmdlineApiId, cmdlineVendorId, true);
  //lbs.addTask("test1", [&](size_t) {main(GraphicsApi::Vulkan, VendorID::Nvidia, true); });
  //lbs.sleepTillKeywords({ "test1" });

#endif
  /*
  RangeMath::difference({ 0, 5, 0, 4 }, { 2, 1, 1, 2 }, [](SubresourceRange r) {printRange(r);});
  RangeMath::difference({0, 5, 1, 2}, {2, 1, 0, 5}, [](SubresourceRange r) {printRange(r);});
  RangeMath::difference({1, 2, 0, 4}, {0, 5, 1, 2}, [](SubresourceRange r) {printRange(r);});
  RangeMath::difference({2, 1, 1, 2}, {0, 5, 0, 4}, [](SubresourceRange r) {printRange(r);});
  RangeMath::difference({ 0, 2, 0, 2 }, { 1, 1, 1, 1 }, [](SubresourceRange r) {printRange(r);});
  RangeMath::difference({ 0, 1, 0, 1 }, { 1, 1, 1, 1 }, [](SubresourceRange r) {printRange(r);});
  */

  log.update();
}
