
struct VSInput {
    float2 inPosition;
    float3 inColor;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float    time;
};
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 0.0, 1.0))));
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    // three sine waves 120 degrees apart cycle through hues over time
    float3 cycle = float3(
        //sin(  time * SPEED + PHASE_OFFSET) * AMPLITUDE + CENTER
        sin(ubo.time * 0.8 + 0.000) * 0.5 + 0.5,  // R
        sin(ubo.time * 0.8 + 2.094) * 0.5 + 0.5,  // G  (+2π/3) 120 degrees behind
        sin(ubo.time * 0.8 + 4.189) * 0.5 + 0.5   // B  (+4π/3) 240 degrees behind 
    );
    return float4(vertIn.color * cycle, 1.0);
}