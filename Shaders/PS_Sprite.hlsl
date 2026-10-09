#include "Common.hlsl"

Texture2D SpriteTexture : register(t0, space1);

float4 PSMain(PSInput_Sprite input) : SV_Target
{    
    return input.color * SpriteTexture.Sample(WrapLinearSampler, input.uv) * color;
}