struct O2MaterialParams
{
    float2 u_lightDir;
    float  u_ambient;
    float  u_diffuse;
    float  u_diffusePow;
    float  u_specular;
    float  u_shininess;
    float  u_specular2;
    float  u_shininess2;
    float  u_rim;
    float  u_rimPow;
    float  u_fill;
    float  u_fillPow;
    float  u_edgeLight;
    float  u_edgePow;
    float4 u_shadowColor;
    float4 u_specColor;
};

// Fake-3D lit match-3 chip: albedo in u_texture, sprite-space normal map in u_normalMap.
// The sprite mesh stores its world X axis in the vertex normal, so the map is rotated
// to world space per pixel and lit by a fixed world-space directional light — highlights
// and shading stay anchored to the light while the sprite spins.
fragment float4 fragmentShader(O2RasterizerData input [[stage_in]],
                               texture2d<float> u_texture [[texture(0)]],
                               sampler textureSampler [[sampler(0)]],
                               texture2d<float> u_normalMap [[texture(1)]],
                               sampler normalMapSampler [[sampler(1)]],
                               constant O2MaterialParams& params [[buffer(2)]])
{
    float4 albedo = u_texture.sample(textureSampler, input.texCoords);
    float3 localN = u_normalMap.sample(normalMapSampler, input.texCoords2).xyz*2.0 - 1.0;

    float2 axis = input.normal.xy;
    float2 T = dot(axis, axis) > 1.0e-8 ? normalize(axis) : float2(1.0, 0.0);
    float2 B = float2(-T.y, T.x);
    float3 N = normalize(float3(T*localN.x + B*localN.y, localN.z));

    float lz = sqrt(max(1.0 - dot(params.u_lightDir, params.u_lightDir), 0.0));
    float3 L = normalize(float3(params.u_lightDir, lz));
    float3 V = float3(0.0, 0.0, 1.0);
    float3 H = normalize(L + V);

    float ndl = dot(N, L);
    float ndv = saturate(dot(N, V));

    float diff = pow(saturate(ndl*0.5 + 0.5), params.u_diffusePow);
    float ndh = saturate(dot(N, H));
    // two lobes: a broad soft sheen plus a tight crisp glint on emboss bevels
    float spec = pow(ndh, params.u_shininess)*params.u_specular
               + pow(ndh, params.u_shininess2)*params.u_specular2;
    float rim = pow(1.0 - ndv, params.u_rimPow)*saturate(0.5 - 0.5*ndl)*params.u_rim;
    float fill = pow(saturate(-ndl), 2.0)*pow(1.0 - ndv, params.u_fillPow)*params.u_fill;
    // grazing sheen: bright arc on the lit side of the silhouette rim
    float2 edgeDir = normalize(N.xy + float2(1.0e-6, 0.0));
    float2 lightXY = normalize(params.u_lightDir + float2(1.0e-6, 0.0));
    float edge = pow(1.0 - ndv, 3.0)*pow(saturate(dot(edgeDir, lightXY)), params.u_edgePow)
               *params.u_edgeLight;

    float3 base = albedo.rgb;
    // candy-style shading: shadow shifts toward a saturated dark tint instead of
    // multiplying brightness down, keeping the color vivid on the dark side
    float3 shadow = base*base*params.u_shadowColor.rgb;
    float3 lit = mix(shadow, base, saturate(params.u_ambient + params.u_diffuse*diff));
    lit = mix(lit, shadow, rim);
    lit += spec*params.u_specColor.rgb;
    lit += base*fill;
    lit += edge*params.u_specColor.rgb;

    return float4(saturate(lit), albedo.a)*input.color;
}
