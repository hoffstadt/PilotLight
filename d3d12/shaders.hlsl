
// constant buffer
cbuffer ConstantBuffer : register(b0)
{
    float x_offset;
    float y_offset;
};

struct VS_Output
{
    float4 position : SV_POSITION;
    float2 TexCoord : TexCoord;
};

VS_Output vs_main(float2 pos : Position, float2 TexCoord : TexCoord)
{
    VS_Output output;
    output.position = float4(pos.x + x_offset, pos.y + y_offset, 0.0f, 1.0f);
    output.TexCoord = TexCoord;
    return output;
}

float4 ps_main(VS_Output input) : SV_TARGET
{
    //return ColorTexture.Sample(Sampler, input.TexCoord);
    return float4(1.0, 1.0, 0.0, 1.0);
}