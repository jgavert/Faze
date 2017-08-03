#include "triangle.if.hpp"

struct PSInput
{
  float2 uv : TEXCOORD0;
  float4 pos : SV_Position;
};

GX_SIGNATURE
float4 main(PSInput input) : SV_Target
{
	float4 final = constants.color * 0.f;
	final += yellow.Sample(staSam, input.uv);

  return final;
}
