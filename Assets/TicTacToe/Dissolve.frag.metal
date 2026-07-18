struct O2MaterialParams
{
    float4 u_edgeColor;
    float u_edgeWidth;
    float u_progress;
};

static float o2_dissolve_hash(float2 p)
{
    return fract(sin(dot(p, float2(127.1, 311.7)))*43758.5453);
}

static float o2_dissolve_vnoise(float2 p)
{
    float2 i = floor(p);
    float2 f = fract(p);
    float2 u = f*f*(3.0 - 2.0*f);
    return mix(mix(o2_dissolve_hash(i), o2_dissolve_hash(i + float2(1.0, 0.0)), u.x),
               mix(o2_dissolve_hash(i + float2(0.0, 1.0)), o2_dissolve_hash(i + float2(1.0, 1.0)), u.x), u.y);
}

fragment float4 fragmentShader(O2RasterizerData input [[stage_in]],
                               constant O2MaterialParams& materialParams [[buffer(2)]],
                               texture2d<float> u_texture [[texture(0)]],
                               sampler textureSampler [[sampler(0)]])
{
    float4 tex = input.color * u_texture.sample(textureSampler, input.texCoords);

    float n = o2_dissolve_vnoise(input.texCoords*40.0)*0.65 + o2_dissolve_vnoise(input.texCoords*90.0)*0.35;
    float d = n + materialParams.u_edgeWidth - materialParams.u_progress*(1.0 + materialParams.u_edgeWidth*2.0);

    float body = step(materialParams.u_edgeWidth, d);
    float edge = clamp(smoothstep(0.0, materialParams.u_edgeWidth, d) - body, 0.0, 1.0);

    float4 color = tex*body + float4(materialParams.u_edgeColor.rgb, 1.0)*tex.a*edge;
    color.a *= smoothstep(0.0, materialParams.u_edgeWidth*0.5, d);

    return color;
}
