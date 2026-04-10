struct O2MaterialParams
{
    float u_brightness;
    float4 u_outlineColor;
    float u_outlineRadius;
    float2 u_texelSize;
};

fragment float4 fragmentShader(O2RasterizerData input [[stage_in]],
                               constant O2MaterialParams& materialParams [[buffer(2)]],
                               texture2d<float> u_texture [[texture(0)]],
                               texture2d<float> u_texture2 [[texture(1)]])
{
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);

    float4 tex = u_texture.sample(textureSampler, input.texCoords);
    float centerAlpha = tex.a;
    float2 pixelOffset = materialParams.u_outlineRadius * materialParams.u_texelSize;

    float maxNeighborAlpha = centerAlpha;
    maxNeighborAlpha = max(maxNeighborAlpha, u_texture.sample(textureSampler, input.texCoords + float2( pixelOffset.x,  0.0)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, u_texture.sample(textureSampler, input.texCoords + float2(-pixelOffset.x,  0.0)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, u_texture.sample(textureSampler, input.texCoords + float2( 0.0,  pixelOffset.y)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, u_texture.sample(textureSampler, input.texCoords + float2( 0.0, -pixelOffset.y)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, u_texture.sample(textureSampler, input.texCoords + float2( pixelOffset.x,  pixelOffset.y)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, u_texture.sample(textureSampler, input.texCoords + float2(-pixelOffset.x,  pixelOffset.y)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, u_texture.sample(textureSampler, input.texCoords + float2( pixelOffset.x, -pixelOffset.y)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, u_texture.sample(textureSampler, input.texCoords + float2(-pixelOffset.x, -pixelOffset.y)).a);

    const float edgeThreshold = 0.5;
    float4 result;
    if (centerAlpha > edgeThreshold)
        result = input.color * tex * materialParams.u_brightness;
    else if (maxNeighborAlpha > edgeThreshold)
        result = materialParams.u_outlineColor;
    else
        result = float4(0.0, 0.0, 0.0, 0.0);

    float4 tex2 = u_texture2.sample(textureSampler, input.texCoords2);
    return mix(result, tex2 * input.color, tex2.a);
}