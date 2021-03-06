// INTERFACE_HASH:1602018390450552050:6303928268241932957
// This file is generated from code.
#ifdef HIGANBANA_VULKAN
#define VK_BINDING(index, set) [[vk::binding(index, set)]]
#else // HIGANBANA_DX12
#define VK_BINDING(index, set) 
#endif

#define ROOTSIG "RootFlags(0), \
  CBV(b0), \
  DescriptorTable( UAV(u99, numDescriptors = 1, space=99 )), \
  DescriptorTable(\
     SRV(t0, numDescriptors = 1, space=0 )),\
  DescriptorTable(\
     SRV(t0, numDescriptors = 6, space=1 )),\
  DescriptorTable(\
     SRV(t0, numDescriptors = 1, space=2 ),\
     SRV(t1, numDescriptors = unbounded, space=2, flags=DESCRIPTORS_VOLATILE )),\
  StaticSampler(s0, filter = FILTER_MIN_MAG_LINEAR_MIP_POINT), \
  StaticSampler(s1, filter = FILTER_MIN_MAG_MIP_POINT), \
  StaticSampler(s2, filter = FILTER_MIN_MAG_LINEAR_MIP_POINT, addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP, addressW = TEXTURE_ADDRESS_WRAP), \
  StaticSampler(s3, filter = FILTER_MIN_MAG_MIP_POINT, addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP, addressW = TEXTURE_ADDRESS_WRAP)"

struct Constants
{uint meshletCount; int camera; float2 unused; };
VK_BINDING(0, 3) ConstantBuffer<Constants> constants : register( b0 );
VK_BINDING(1, 3) RWByteAddressBuffer _debugOut : register( u99, space99 );
// Shader Arguments 0
// Struct declarations
struct CameraSettings { float4x4 perspective; };

// Read Only resources
VK_BINDING(0, 0) StructuredBuffer<CameraSettings> cameras : register( t0, space0 );

// Read Write resources
// Shader Arguments 1
// Struct declarations
struct WorldMeshlet { uint primitives; uint vertices; uint offsetUnique; uint offsetPacked; };

// Read Only resources
VK_BINDING(0, 1) StructuredBuffer<WorldMeshlet> meshlets : register( t0, space1 );
VK_BINDING(1, 1) Buffer<uint> uniqueIndices : register( t1, space1 );
VK_BINDING(2, 1) Buffer<uint> packedIndices : register( t2, space1 );
VK_BINDING(3, 1) Buffer<float3> vertices : register( t3, space1 );
VK_BINDING(4, 1) Buffer<float2> uvs : register( t4, space1 );
VK_BINDING(5, 1) Buffer<float3> normals : register( t5, space1 );

// Read Write resources
// Shader Arguments 2
// Struct declarations
struct MaterialData { double3 emissiveFactor; double alphaCutoff; bool doubleSided; double4 baseColorFactor; double metallicFactor; double roughnessFactor; uint albedoIndex; uint normalIndex; uint metallicRoughnessIndex; uint occlusionIndex; uint emissiveIndex; };

// Read Only resources
VK_BINDING(0, 2) StructuredBuffer<MaterialData> materials : register( t0, space2 );

// Read Write resources

// Bindless
VK_BINDING(1, 2) Texture2D materialTextures[] : register( t1, space2 );

// Usable Static Samplers
VK_BINDING(2, 3) SamplerState bilinearSampler : register( s0 );
VK_BINDING(3, 3) SamplerState pointSampler : register( s1 );
VK_BINDING(4, 3) SamplerState bilinearSamplerWarp : register( s2 );
VK_BINDING(5, 3) SamplerState pointSamplerWrap : register( s3 );

uint getIndex(uint count, uint type)
{
	uint myIndex;
	_debugOut.InterlockedAdd(0, count+1, myIndex);
	_debugOut.Store(myIndex*4+4, type);
	return myIndex*4+4+4;
}
void print(uint val)   { _debugOut.Store( getIndex(1, 1), val); }
void print(uint2 val)  { _debugOut.Store2(getIndex(2, 2), val); }
void print(uint3 val)  { _debugOut.Store3(getIndex(3, 3), val); }
void print(uint4 val)  { _debugOut.Store4(getIndex(4, 4), val); }
void print(int val)    { _debugOut.Store( getIndex(1, 5), asuint(val)); }
void print(int2 val)   { _debugOut.Store2(getIndex(2, 6), asuint(val)); }
void print(int3 val)   { _debugOut.Store3(getIndex(3, 7), asuint(val)); }
void print(int4 val)   { _debugOut.Store4(getIndex(4, 8), asuint(val)); }
void print(float val)  { _debugOut.Store( getIndex(1, 9), asuint(val)); }
void print(float2 val) { _debugOut.Store2(getIndex(2, 10), asuint(val)); }
void print(float3 val) { _debugOut.Store3(getIndex(3, 11), asuint(val)); }
void print(float4 val) { _debugOut.Store4(getIndex(4, 12), asuint(val)); }
