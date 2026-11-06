#include "Common.hlsl"

PSInput_Sprite VSMain(VSInput input)
{
    PSInput_Sprite output;
    output.position = mul(mul(mul(float4(input.position, 1.0f), world), view), proj);
    output.uv = input.uv;
    output.color = input.color;
    
    return output;
}
