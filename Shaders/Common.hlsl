
struct VSInput
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
    float3 normal   : NORMAL;
    float3 tangent : TANGENT;
    float4 color    : COLOR;
};

struct PSInput_Default
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float2 uv       : TEXCOORD0;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float4 color    : COLOR;
};

struct PSInput_Sprite
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;
};

cbuffer PerFrameBuffer : register(b0)
{
    float4x4 view;
    float4x4 proj;
    float3 cameraPos;
    float time;
};

cbuffer PerObjectBuffer : register(b1)
{
    float4x4 world;
};

cbuffer PerMaterialBuffer : register(b2)
{
    float4 color;
    float4 specular;
    float4 emission;
    float roughness;
    float metallic;
};

SamplerState WrapLinearSampler : register(s0);
SamplerState ClampLinearSampler : register(s1);
SamplerState WrapPointSampler : register(s2);
SamplerState ClampPointSampler : register(s3);