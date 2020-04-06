#pragma once

#include <higanbana/core/math/math.hpp>
#include <higanbana/core/datastructures/proxy.hpp>
#include <higanbana/core/entity/database.hpp>

#include <string>

namespace components
{
struct WorldPosition
{
  float3 pos;
};

struct Rotation
{
  quaternion rot;
};

struct Matrix
{
  float4x4 mat;
};

struct CameraSettings
{
  float fov;
  float minZ;
  float maxZ;
};

struct Name
{
  std::string str;
};

struct Childs
{
  higanbana::vector<higanbana::Id> childs;
};

struct Mesh
{
  higanbana::Id target;
};

struct Camera
{
  higanbana::Id target;
};

struct RawMeshData
{
  int id;
};
struct RawTextureData
{
  int id;
};
struct RawBufferData
{
  int id;
};

struct MaterialLink
{
  higanbana::Id albedo;
  higanbana::Id normal;
  higanbana::Id metallicRoughness;
  higanbana::Id occlusion;
  higanbana::Id emissive;
};
struct BufferInstance
{
  int id;
};

struct MeshInstance
{
  int id;
};

struct TextureInstance
{
  int id;
};
struct MaterialGPUInstance
{
  int id;
};
struct MaterialInstance
{
  higanbana::Id id;
};

struct SceneInstance
{
  higanbana::Id target;
};

struct ViewportCamera
{
  int targetViewport;
};

struct GltfNode;
struct CameraNode;
struct MeshNode;
struct Scene;
struct SceneNode;
struct ActiveCamera;
}