struct O2MaterialParams
{
    float4 u_grad;        // gradSpread, gradPow, iconGradSpread, iconGradPow
    float4 u_baseTop;   float4 u_baseBottom;
    float4 u_iconTop;   float4 u_iconBottom;
    float4 u_shadowColor; // rgb, a = strength
    float4 u_glint;       // world dir x, y, radius fraction, pow
    float4 u_glintEx;     // rx, ry, strength, unused
    float4 u_glintColor;
    float4 u_dots[4];     // per dot: world dir x, y, distance px, radius px
    float4 u_dotStrengths;
    float4 u_dotColor;
};

// Geometry shared by every chip: world light, DF layout, alpha fit, shadow style.
constant float2 LIGHT_DIR = float2(-0.3292, 0.9443);
constant float2 SHADOW_DIR = float2(0.7071, -0.7071);
constant float2 CENTER = float2(0.49881, 0.5012);
constant float RADIUS_UV = 0.50076;
constant float TEX_SIZE = 420.0;
constant float PX_SCALE = 256.0;   // 2 * DF half-range (128 px)
constant float ALPHA_OFF = 0.3;
constant float ALPHA_SOFT = 1.63571;
constant float RIM_RANGE = 40.0;
constant float ICON_RANGE = 20.0;
constant float SHADOW_OFFSET = 30.0;
constant float SHADOW_SOFT = 3.0;
constant float ICON_AA = 2.5;

// LUT texture layout: 66x24, columns [wrap|0..63|wrap] over light-relative angle,
// rows 1..9 = base rim by edge distance, rows 14..22 = icon edge by depth.
// Raw sampler coordinates bypass the engine's UV remap: rows address top-down.
static inline float3 o2_sampleLut(texture2d<float> lut, sampler s, float ang, float rowf)
{
    float ub = fract((ang + 3.14159265)/(2.0*3.14159265));
    float2 uvL = float2((1.0 + ub*64.0)/66.0, 1.0 - (rowf + 0.5)/24.0);
    return lut.sample(s, uvL).rgb;
}

// SDF-composed match-3 chip: the reference art is reproduced from two distance
// fields (round base, inner icon). The rim and icon-edge coloring is the baked
// reference LUT; the interior is a light-aligned gradient; shadow, glint and dots
// are world-anchored so the look re-anchors to the light while the sprite spins.
fragment float4 fragmentShader(O2RasterizerData input [[stage_in]],
                               texture2d<float> u_texture [[texture(0)]],
                               sampler textureSampler [[sampler(0)]],
                               texture2d<float> u_lutMap [[texture(1)]],
                               sampler lutSampler [[sampler(1)]],
                               constant O2MaterialParams& p [[buffer(2)]])
{
    float4 dfs = u_texture.sample(textureSampler, input.texCoords);
    float baseD = (dfs.r - 0.5)*PX_SCALE;
    float iconD = (dfs.g - 0.5)*PX_SCALE;
    float alpha = smoothstep(ALPHA_OFF - ALPHA_SOFT, ALPHA_OFF + ALPHA_SOFT, baseD);

    // world -> sprite rotation from the sprite world X axis stored in the vertex normal
    float2 axis = input.normal.xy;
    float2 T = dot(axis, axis) > 1.0e-8 ? normalize(axis) : float2(1.0, 0.0);
    float2 P = float2(-T.y, T.x);
    float2 Ls = normalize(float2(dot(LIGHT_DIR, T), dot(LIGHT_DIR, P)));

    // engine texCoords have v growing UP the sprite: uv space is y-up
    float2 uv = input.texCoords;
    float2 rel = (uv - CENTER)*TEX_SIZE;
    float Rpx = RADIUS_UV*TEX_SIZE;
    float2 rad = normalize(rel + float2(1.0e-5, 0.0));

    // base gradient along the light
    float t = pow(saturate(0.5 + dot(rel, Ls)/(Rpx*p.u_grad.x)), p.u_grad.y);
    float3 color = mix(p.u_baseBottom.rgb, p.u_baseTop.rgb, t);

    // rim: the baked reference ring (light-relative azimuth x edge distance)
    float angB = atan2(rad.x*Ls.y - rad.y*Ls.x, dot(rad, Ls));
    float dn = baseD/RIM_RANGE;
    float3 lutc = o2_sampleLut(u_lutMap, lutSampler, angB, 1.0 + saturate(dn)*8.0);
    float fade = (1.0 - smoothstep(0.75, 1.0, dn))*smoothstep(-3.0, 1.0, baseD);
    color = mix(color, lutc, fade);

    // icon drop shadow: the icon silhouette offset along the world shadow dir
    float2 Sd = normalize(float2(dot(SHADOW_DIR, T), dot(SHADOW_DIR, P)));
    float2 offUV = -Sd*SHADOW_OFFSET/TEX_SIZE;
    float iconOffD = (u_texture.sample(textureSampler, uv + offUV).g - 0.5)*PX_SCALE;
    float2 gdir = normalize(dfs.ba*2.0 - 1.0 + float2(1.0e-6, 0.0));
    float iconMask = smoothstep(-ICON_AA, ICON_AA, iconD);
    float sh = smoothstep(-SHADOW_SOFT, SHADOW_SOFT, iconOffD)*p.u_shadowColor.a*(1.0 - iconMask);
    color = mix(color, p.u_shadowColor.rgb, sh);

    // base glint: its own world direction, elliptical falloff
    float2 Gs = normalize(float2(dot(p.u_glint.xy, T), dot(p.u_glint.xy, P)));
    float2 gpos = Gs*Rpx*p.u_glint.z;
    float2 gd = (rel - gpos)/p.u_glintEx.xy;
    float g = pow(saturate(1.0 - length(gd)), p.u_glint.w)*p.u_glintEx.z;
    color = mix(color, p.u_glintColor.rgb, g);

    // icon: fill gradient + baked reference edge coloring
    float t2 = pow(saturate(0.5 + dot(rel, Ls)/(Rpx*p.u_grad.z)), p.u_grad.w);
    float3 icol = mix(p.u_iconBottom.rgb, p.u_iconTop.rgb, t2);

    float angI = atan2(gdir.x*Ls.y - gdir.y*Ls.x, dot(gdir, Ls));
    float dni = iconD/ICON_RANGE;
    float3 lutci = o2_sampleLut(u_lutMap, lutSampler, angI, 14.0 + saturate(dni)*8.0);
    float fadei = (1.0 - smoothstep(0.7, 1.0, dni))*smoothstep(-1.0, 1.5, iconD);
    icol = mix(icol, lutci, fadei);

    color = mix(color, icol, iconMask);

    // round glint dots: bright peaks measured from the art, world-anchored
    for (int i = 0; i < 4; i++)
    {
        float st = p.u_dotStrengths[i];
        if (st <= 0.001)
            continue;
        float4 dv = p.u_dots[i];
        float2 Ds = normalize(float2(dot(dv.xy, T), dot(dv.xy, P)) + float2(1.0e-6, 0.0));
        float2 dpos = Ds*dv.z;
        float dd = length(rel - dpos)/max(dv.w, 1.0e-3);
        float gg = pow(1.0 - smoothstep(0.0, 1.0, dd), 1.5)*st;
        color = mix(color, p.u_dotColor.rgb, gg);
    }

    return float4(color, alpha)*input.color;
}
