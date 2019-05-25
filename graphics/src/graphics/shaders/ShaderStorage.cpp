#include "graphics/shaders/ShaderStorage.hpp"
#include <core/external/SpookyV2.hpp>
#include <string_view>

namespace faze
{
  namespace backend
  {    
    ShaderStorage::ShaderStorage(FileSystem& fs, std::shared_ptr<ShaderCompiler> compiler, std::string shaderPath, std::string binaryPath, ShaderBinaryType type)
      : m_fs(fs)
      , sourcePath("/" + shaderPath + "/")
      , compiledPath("/" + binaryPath + "/")
      , m_type(type)
      , m_compiler(compiler)
    {
      m_fs.loadDirectoryContentsRecursive(sourcePath);
    }

    std::string ShaderStorage::sourcePathCombiner(std::string shaderName, ShaderType type)
    {
      return sourcePath + shaderName + "." + shaderFileType(type) + ".hlsl";
    }

    std::string ShaderStorage::binaryPathCombiner(std::string shaderName, ShaderType type, uint3 tgs, std::vector<std::string> definitions)
    {
      std::string shaderBinType = toString(m_type);
      shaderBinType += "/";
      std::string shaderExtension = ".";
      shaderExtension += toString(m_type);

      std::string binType = "Release/";
#if defined(DEBUG)
      binType = "Debug/";
#endif

      std::string additionalBinInfo = "";

      if (type == ShaderType::Compute)
      {
        additionalBinInfo += ".";
        additionalBinInfo += std::to_string(tgs.x) + "_";
        additionalBinInfo += std::to_string(tgs.y) + "_";
        additionalBinInfo += std::to_string(tgs.z);
      }

      // TODO: instead of embedding all definitions to filename, create hash
      for (auto&& it : definitions)
      {
        additionalBinInfo += ".";
        additionalBinInfo += it;
      }

      return compiledPath + shaderBinType + binType + shaderName + additionalBinInfo + "." + shaderFileType(type) + shaderExtension;
    }

    std::string shaderStubFile(ShaderCreateInfo info)
    {
      std::string superSimple = "#include \"" + info.desc.shaderName + ".if.hlsl\"\n";
      superSimple += "// this is trying to be ";
      switch (info.desc.type)
      {
      case ShaderType::Vertex:
        superSimple += "Vertex";
        break;
      case ShaderType::Pixel:
        superSimple += "Pixel";
        break;
      case ShaderType::Compute:
        superSimple += "Compute";
        break;
      case ShaderType::Geometry:
        superSimple += "Geometry";
        break;
      case ShaderType::TessControl: // hull?
        superSimple += "TessControl(hull?)";
        break;
      case ShaderType::TessEvaluation: // domain?
        superSimple += "TessEvaluation(domain?)";
        break;
      default:
        F_ASSERT(false, "Unknown ShaderType");
      }
      superSimple += " shader file.\n";
      superSimple += "\n\n[RootSignature(ROOTSIG)]\n";
      if (info.desc.type == ShaderType::Compute)
      {
        superSimple += "[numthreads(FAZE_THREADGROUP_X, FAZE_THREADGROUP_Y, FAZE_THREADGROUP_Z)] // @nolint\n";
      }
      superSimple += "void main(uint2 id : SV_DispatchThreadID, uint2 gid : SV_GroupThreadID)\n{ \n\n\n}\n";
      return superSimple;
    }

    bool isInterfaceValid(faze::FileSystem& fs, const std::string& interfacePath, const std::string& interfaceData)
    {
      SpookyHash hash;
      hash.Init(1337, 715517);
      hash.Update(interfaceData.data(), interfaceData.size());
      uint64_t h1, h2;
      hash.Final(&h1, &h2);
      bool needsReplacing = true;
      if (fs.fileExists(interfacePath))
      {
        auto fileView = fs.readFile(interfacePath);
        std::string_view view(reinterpret_cast<char*>(fileView.data()), fileView.size());
        uint64_t t1, t2;
        auto v = view.find_first_of("INTERFACE_HASH");
        if (v != std::string_view::npos)
        {
          auto startPos = view.find_first_of(":", v);
          auto middlePos = view.find_first_of(":", startPos+1);
          auto endPos = view.find_first_of("\n", middlePos);
          auto firstHash = std::string(view.data()+startPos+1, middlePos - startPos-1);
          auto secondHash = std::string(view.data()+middlePos+1, endPos - middlePos-1);
          t1 = std::stoull(firstHash);
          t2 = std::stoull(secondHash);
          needsReplacing = h1 != t1 || h2 != t2;
        }
      }
      if (needsReplacing) // if interface old, replace:
      {
        std::string addedHash = "// INTERFACE_HASH:" + std::to_string(h1) + ":" + std::to_string(h2)+"\n";
        addedHash += interfaceData;
        fs.writeFile(interfacePath, makeByteView(addedHash.data(), addedHash.size()));
        F_LOG("ShaderStorage", "Shader interfacefile \"%s\" was old/missing. Created new one.", interfacePath);
        // F_LOG("ShaderStorage", "Made a shader file(%s) for if: %s shader", shaderInterfacePath.c_str(), );
      }
      return !needsReplacing;
    }

    void ShaderStorage::ensureShaderSourceFilesExist(ShaderCreateInfo info)
    {
      // check situation first
      auto shaderInterfacePath = sourcePath + info.desc.shaderName + ".if.hlsl";
      auto shaderPath = sourcePathCombiner(info.desc.shaderName, info.desc.type);
      // just overwrite interface for now...
      // just output the interface from shaderCreateInfo generated by ShaderInputDescriptor
      isInterfaceValid(m_fs, shaderInterfacePath, info.desc.interfaceDeclaration);

      if (!m_fs.fileExists(shaderPath))
      {
        // hurr, stub shader ... I need create them manually first and then generate them.
        // maybe assert on what I haven't done yet.
        std::string superSimple = shaderStubFile(info); 
        m_fs.writeFile(shaderPath, makeByteView(superSimple.data(), superSimple.size()));

        F_LOG("ShaderStorage", "Made a shader file(%s) for %s: %s shader", shaderInterfacePath.c_str(), shaderFileType(info.desc.type), info.desc.shaderName);
      }
    }

    faze::MemoryBlob ShaderStorage::shader(ShaderCreateInfo info)
    {
      auto shaderPath = sourcePathCombiner(info.desc.shaderName, info.desc.type);
      auto dxilPath = binaryPathCombiner(info.desc.shaderName, info.desc.type, info.desc.tgs, info.desc.definitions);

      auto func = [](std::string filename)
      {
        F_LOG("included: %s\n", filename.c_str());
      };
      ensureShaderSourceFilesExist(info);
      if (!m_fs.fileExists(dxilPath))
      {
        //      F_ILOG("ShaderStorage", "First time compiling \"%s\"", shaderName.c_str());
        //F_ASSERT(compileShader(shaderName, type, tgs), "ups");
        F_ASSERT(m_compiler, "no compiler");
        F_ASSERT(m_compiler->compileShader(
          m_type,
          shaderPath,
          dxilPath,
          info,
          func), "ups");
      }
      if (m_fs.fileExists(dxilPath) && m_fs.fileExists(shaderPath))
      {
        auto shaderInterfacePath = sourcePath + info.desc.shaderName + ".if.hlsl";

        auto shaderTime = m_fs.timeModified(shaderPath);
        auto dxilTime = m_fs.timeModified(dxilPath);
        auto shaderInterfaceTime = dxilTime;

        if (m_fs.fileExists(shaderInterfacePath))
        {
          shaderInterfaceTime = m_fs.timeModified(shaderInterfacePath);
        }

        if (m_compiler && (shaderTime > dxilTime || shaderInterfaceTime > dxilTime))
        {
          // F_ILOG("ShaderStorage", "Spirv was old, compiling: \"%s\"", shaderName.c_str());
          bool result = m_compiler->compileShader(
            m_type,
            shaderPath,
            dxilPath,
            info,
            func);
          if (!result)
          {
            F_ILOG("DX12", "Shader compile failed.\n");
          }
        }
      }
      F_ASSERT(m_fs.fileExists(dxilPath), "wtf???");
      auto shader = m_fs.readFile(dxilPath);
      return shader;
    }

    WatchFile ShaderStorage::watch(std::string shaderName, ShaderType type)
    {
      auto shd = sourcePath + shaderName + "." + shaderFileType(type) + ".hlsl";
      if (m_fs.fileExists(shd))
      {
        return m_fs.watchFile(shd);
      }
      return WatchFile{};
    }
  }
}