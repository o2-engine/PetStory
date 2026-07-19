struct O2MaterialParams
{
    float2 u_lightDir;
    float2 u_bounceDir;
    float2 u_glintDir;      // world direction of the base glint (measured from the art)
    float2 u_shadowDir;     // world direction of the icon drop shadow (down-right)
    float2 u_center;        // chip center in UV
    float  u_radiusUV;      // chip radius in UV units
    float  u_texSize;       // DF texture size the distances were encoded at
    float  u_dfRange;       // encoded half-range in px

    float  u_gradSpread;
    float  u_gradPow;
    float  u_darkPos;   float u_darkWidth;   float u_darkSoft;   float u_darkStrength;
    float  u_litPos;    float u_litWidth;    float u_litSoft;    float u_litStrength;
    float  u_azEdge0;   float u_azEdge1;
    float  u_bounceWidePos; float u_bounceWideWidth; float u_bounceWideSoft; float u_bounceWideStrength;
    float  u_bounceLinePos; float u_bounceLineWidth; float u_bounceLineSoft; float u_bounceLineStrength;
    float  u_azB0;      float u_azB1;
    float  u_shadowOffset; float u_shadowSoft; float u_shadowStrength; float u_shadowGrow;
    float  u_glintRad;  float u_glintRx; float u_glintRy; float u_glintPow; float u_glintStrength;
    float  u_iconAA;
    float  u_iconGradSpread; float u_iconGradPow;
    float  u_iconLitPos;   float u_iconLitWidth;   float u_iconLitSoft;   float u_iconLitStrength;
    float  u_iconShadePos; float u_iconShadeWidth; float u_iconShadeSoft; float u_iconShadeStrength;
    float  u_iconBouncePos; float u_iconBounceWidth; float u_iconBounceSoft; float u_iconBounceStrength;
    float  u_iazEdge0;  float u_iazEdge1;
    float  u_iconGlintPos; float u_iconGlintWidth; float u_iconGlintSoft; float u_iconGlintStrength;
    float  u_iglAz0;    float u_iglAz1;

    float4 u_baseTop;   float4 u_baseBottom;
    float4 u_darkColor; float4 u_litColor;
    float4 u_bounceColor; float4 u_bounceLineColor;
    float4 u_shadowColor; float4 u_glintColor;
    float4 u_iconTop;   float4 u_iconBottom;
    float4 u_iconLitColor; float4 u_iconShadeColor; float4 u_iconBounceColor; float4 u_iconGlintColor;
    float4 u_dots[4];         // per dot: world dir x, y, distance px, radius px
    float4 u_dotStrengths;
    float4 u_dotColor;
    float  u_rimLutBlend;
    float  u_iconLutBlend;
    float  u_rimRange;
    float  u_alphaOff;
    float  u_alphaSoft;
    float  u_shadowAz0;
    float  u_shadowAz1;
};

// LUT texture layout: 66x24, columns [wrap|0..63|wrap] over light-relative angle,
// rows 1..9 = base rim by edge distance, rows 14..22 = icon edge by depth.
// Raw sampler coordinates bypass the engine's UV remap: rows address top-down.
static inline float3 o2_sampleLut(texture2d<float> lut, sampler s, float ang, float rowf)
{
    float ub = fract((ang + 3.14159265)/(2.0*3.14159265));
    float2 uvL = float2((1.0 + ub*64.0)/66.0, 1.0 - (rowf + 0.5)/24.0);
    return lut.sample(s, uvL).rgb;
}

static inline float o2_band(float d, float center, float width, float soft)
{
    float lo = center - width*0.5;
    float hi = center + width*0.5;
    return smoothstep(lo - soft, lo, d)*(1.0 - smoothstep(hi, hi + soft, d));
}

// SDF-composed match-3 chip: the reference art is reproduced constructively from two
// distance fields (round base, inner icon). Every light element (gradients, edge
// crescents, bounce, glints, icon drop shadow) is positioned from world-space light
// directions rotated into sprite space, so the look re-anchors to the light while
// the sprite spins.
fragment float4 fragmentShader(O2RasterizerData input [[stage_in]],
                               texture2d<float> u_texture [[texture(0)]],
                               sampler textureSampler [[sampler(0)]],
                               texture2d<float> u_lutMap [[texture(1)]],
                               sampler lutSampler [[sampler(1)]],
                               constant O2MaterialParams& p [[buffer(2)]])
{
    float4 dfs = u_texture.sample(textureSampler, input.texCoords);
    float pxScale = 2.0*p.u_dfRange;
    float baseD = (dfs.r - 0.5)*pxScale;
    float iconD = (dfs.g - 0.5)*pxScale;
    float alpha = smoothstep(p.u_alphaOff - p.u_alphaSoft, p.u_alphaOff + p.u_alphaSoft, baseD);

    // world -> sprite rotation from the sprite world X axis stored in the vertex normal
    float2 axis = input.normal.xy;
    float2 T = dot(axis, axis) > 1.0e-8 ? normalize(axis) : float2(1.0, 0.0);
    float2 P = float2(-T.y, T.x);
    float2 Ls = normalize(float2(dot(p.u_lightDir, T), dot(p.u_lightDir, P)));
    float2 Bs = normalize(float2(dot(p.u_bounceDir, T), dot(p.u_bounceDir, P)));

    // engine texCoords have v growing UP the sprite (bottom-up textures + UV remap),
    // so uv space is already y-up; u_center is stored in this space
    float2 uv = input.texCoords;
    float2 rel = float2((uv.x - p.u_center.x)*p.u_texSize,
                        (uv.y - p.u_center.y)*p.u_texSize);
    float Rpx = p.u_radiusUV*p.u_texSize;
    float2 rad = normalize(rel + float2(1.0e-5, 0.0));
    float azL = dot(rad, Ls);
    float azB = dot(rad, Bs);

    // base gradient along the light
    float t = pow(saturate(0.5 + dot(rel, Ls)/(Rpx*p.u_gradSpread)), p.u_gradPow);
    float3 color = mix(p.u_baseBottom.rgb, p.u_baseTop.rgb, t);

    // dark / lit edge crescents
    float m = o2_band(baseD, p.u_darkPos, p.u_darkWidth, p.u_darkSoft)
            * smoothstep(p.u_azEdge0, p.u_azEdge1, -azL)*p.u_darkStrength;
    color = mix(color, p.u_darkColor.rgb, m);
    m = o2_band(baseD, p.u_litPos, p.u_litWidth, p.u_litSoft)
      * smoothstep(p.u_azEdge0, p.u_azEdge1, azL)*p.u_litStrength;
    color = mix(color, p.u_litColor.rgb, m);

    // complex bottom-right bounce: wide soft glow + narrow bright line
    m = o2_band(baseD, p.u_bounceWidePos, p.u_bounceWideWidth, p.u_bounceWideSoft)
      * smoothstep(p.u_azB0, p.u_azB1, azB)*p.u_bounceWideStrength;
    color = mix(color, p.u_bounceColor.rgb, m);
    m = o2_band(baseD, p.u_bounceLinePos, p.u_bounceLineWidth, p.u_bounceLineSoft)
      * smoothstep(p.u_azB0, p.u_azB1, azB)*p.u_bounceLineStrength;
    color = mix(color, p.u_bounceLineColor.rgb, m);

    // reference rim LUT: baked (light-relative azimuth x edge distance) coloring
    if (p.u_rimLutBlend > 0.001)
    {
        float angB = atan2(rad.x*Ls.y - rad.y*Ls.x, dot(rad, Ls));
        float dn = baseD/p.u_rimRange;
        float3 lutc = o2_sampleLut(u_lutMap, lutSampler, angB, 1.0 + saturate(dn)*8.0);
        float fade = (1.0 - smoothstep(0.75, 1.0, dn))*smoothstep(-3.0, 1.0, baseD)*p.u_rimLutBlend;
        color = mix(color, lutc, fade);
    }

    // icon drop shadow: simply the icon silhouette offset down-right (world dir),
    // dark with a soft edge, covered by the icon itself
    float2 Sd = normalize(float2(dot(p.u_shadowDir, T), dot(p.u_shadowDir, P)));
    float2 offUV = -Sd*p.u_shadowOffset/p.u_texSize;
    float iconOffD = (u_texture.sample(textureSampler, uv + offUV).g - 0.5)*pxScale;
    float2 gdir = normalize(dfs.ba*2.0 - 1.0 + float2(1.0e-6, 0.0));
    float iconMask = smoothstep(-p.u_iconAA, p.u_iconAA, iconD);
    float sh = smoothstep(-p.u_shadowSoft, p.u_shadowSoft, iconOffD)*p.u_shadowStrength
             * (1.0 - iconMask);
    color = mix(color, p.u_shadowColor.rgb, sh);

    // base glint: its own world direction, elliptical falloff
    float2 Gs = normalize(float2(dot(p.u_glintDir, T), dot(p.u_glintDir, P)));
    float2 gpos = Gs*Rpx*p.u_glintRad;
    float2 gd = (rel - gpos)/float2(p.u_glintRx, p.u_glintRy);
    float g = pow(saturate(1.0 - length(gd)), p.u_glintPow)*p.u_glintStrength;
    color = mix(color, p.u_glintColor.rgb, g);

    // icon fill gradient
    float t2 = pow(saturate(0.5 + dot(rel, Ls)/(Rpx*p.u_iconGradSpread)), p.u_iconGradPow);
    float3 icol = mix(p.u_iconBottom.rgb, p.u_iconTop.rgb, t2);

    // icon edge azimuth from the baked smoothed DF gradient (declared above)
    float iaz = dot(gdir, Ls);

    m = o2_band(iconD, p.u_iconLitPos, p.u_iconLitWidth, p.u_iconLitSoft)
      * smoothstep(p.u_iazEdge0, p.u_iazEdge1, iaz)*p.u_iconLitStrength;
    icol = mix(icol, p.u_iconLitColor.rgb, m);
    m = o2_band(iconD, p.u_iconShadePos, p.u_iconShadeWidth, p.u_iconShadeSoft)
      * smoothstep(p.u_iazEdge0, p.u_iazEdge1, -iaz)*p.u_iconShadeStrength;
    icol = mix(icol, p.u_iconShadeColor.rgb, m);
    m = o2_band(iconD, p.u_iconBouncePos, p.u_iconBounceWidth, p.u_iconBounceSoft)
      * smoothstep(p.u_iazEdge0, p.u_iazEdge1, dot(gdir, Bs))*p.u_iconBounceStrength;
    icol = mix(icol, p.u_iconBounceColor.rgb, m);

    // icon glint: crisp DF arc on the lit side
    m = o2_band(iconD, p.u_iconGlintPos, p.u_iconGlintWidth, p.u_iconGlintSoft)
      * smoothstep(p.u_iglAz0, p.u_iglAz1, iaz)*p.u_iconGlintStrength;
    icol = mix(icol, p.u_iconGlintColor.rgb, m);

    // icon edge LUT: baked reference coloring by (grad-to-light angle x depth)
    if (p.u_iconLutBlend > 0.001)
    {
        float angI = atan2(gdir.x*Ls.y - gdir.y*Ls.x, iaz);
        float dni = iconD/20.0;
        float3 lutci = o2_sampleLut(u_lutMap, lutSampler, angI, 14.0 + saturate(dni)*8.0);
        float fadei = (1.0 - smoothstep(0.7, 1.0, dni))*smoothstep(-1.0, 1.5, iconD)*p.u_iconLutBlend;
        icol = mix(icol, lutci, fadei);
    }

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
