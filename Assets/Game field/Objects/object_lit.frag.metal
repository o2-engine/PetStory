struct O2MaterialParams
{
    float4 u_fx;          // x = lighting modulation strength
    float4 u_shadowColor; // shadow tint: the object's own darkened hue, not black
};

constant float2 LIGHT_DIR = float2(-0.3292, 0.9443);

// The painted lighting lives in the normals: shade = 1 + k * dot(N, L(angle))
// reproduces the source art exactly at the authored orientation and carries the
// same shading around the world light as the sprite spins.
fragment float4 fragmentShader(O2RasterizerData input [[stage_in]],
                               texture2d<float> u_texture [[texture(0)]],
                               sampler textureSampler [[sampler(0)]],
                               texture2d<float> u_nrmMap [[texture(1)]],
                               sampler nrmSampler [[sampler(1)]],
                               constant O2MaterialParams& p [[buffer(2)]])
{
    float4 art = u_texture.sample(textureSampler, input.texCoords);
    float2 n = u_nrmMap.sample(nrmSampler, input.texCoords2).rg*2.0 - 1.0;

    float2 axis = input.normal.xy;
    float2 T = dot(axis, axis) > 1.0e-8 ? normalize(axis) : float2(1.0, 0.0);
    float2 P = float2(-T.y, T.x);
    float2 Ls = normalize(float2(dot(LIGHT_DIR, T), dot(LIGHT_DIR, P)));

    float d = dot(n, Ls);
    float shade = min(1.0 + p.u_fx.x*max(d, 0.0), 1.7);
    float sh = min(p.u_fx.x*max(-d, 0.0), 0.4);
    float3 color = art.rgb*shade*(1.0 - sh) + p.u_shadowColor.rgb*sh;
    return float4(color, art.a)*input.color;
}
