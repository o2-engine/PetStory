varying vec4 v_color;
varying vec2 v_texCoords;
varying vec3 v_normal;

uniform sampler2D u_texture;

uniform vec2 u_lightDir;
uniform vec2 u_bounceDir;
uniform vec2 u_glintDir;
uniform vec2 u_shadowDir;
uniform vec2 u_center;
uniform float u_radiusUV;
uniform float u_texSize;
uniform float u_dfRange;

uniform float u_gradSpread;
uniform float u_gradPow;
uniform float u_darkPos;   uniform float u_darkWidth;   uniform float u_darkSoft;   uniform float u_darkStrength;
uniform float u_litPos;    uniform float u_litWidth;    uniform float u_litSoft;    uniform float u_litStrength;
uniform float u_azEdge0;   uniform float u_azEdge1;
uniform float u_bounceWidePos; uniform float u_bounceWideWidth; uniform float u_bounceWideSoft; uniform float u_bounceWideStrength;
uniform float u_bounceLinePos; uniform float u_bounceLineWidth; uniform float u_bounceLineSoft; uniform float u_bounceLineStrength;
uniform float u_azB0;      uniform float u_azB1;
uniform float u_shadowOffset; uniform float u_shadowSoft; uniform float u_shadowStrength; uniform float u_shadowGrow;
uniform float u_glintRad;  uniform float u_glintRx; uniform float u_glintRy; uniform float u_glintPow; uniform float u_glintStrength;
uniform float u_iconAA;
uniform float u_iconGradSpread; uniform float u_iconGradPow;
uniform float u_iconLitPos;   uniform float u_iconLitWidth;   uniform float u_iconLitSoft;   uniform float u_iconLitStrength;
uniform float u_iconShadePos; uniform float u_iconShadeWidth; uniform float u_iconShadeSoft; uniform float u_iconShadeStrength;
uniform float u_iconBouncePos; uniform float u_iconBounceWidth; uniform float u_iconBounceSoft; uniform float u_iconBounceStrength;
uniform float u_iazEdge0;  uniform float u_iazEdge1;
uniform float u_iconGlintPos; uniform float u_iconGlintWidth; uniform float u_iconGlintSoft; uniform float u_iconGlintStrength;
uniform float u_iglAz0;    uniform float u_iglAz1;

uniform vec4 u_baseTop;   uniform vec4 u_baseBottom;
uniform vec4 u_darkColor; uniform vec4 u_litColor;
uniform vec4 u_bounceColor; uniform vec4 u_bounceLineColor;
uniform vec4 u_shadowColor; uniform vec4 u_glintColor;
uniform vec4 u_iconTop;   uniform vec4 u_iconBottom;
uniform vec4 u_iconLitColor; uniform vec4 u_iconShadeColor; uniform vec4 u_iconBounceColor; uniform vec4 u_iconGlintColor;
uniform vec4 u_dots[4];
uniform vec4 u_dotStrengths;
uniform vec4 u_dotColor;
uniform float u_rimLutBlend;
uniform float u_iconLutBlend;
uniform float u_rimRange;
uniform float u_alphaOff;
uniform float u_alphaSoft;
uniform float u_shadowAz0;
uniform float u_shadowAz1;
uniform sampler2D u_lutMap;

// see chip_sdf.frag.metal for the 66x24 LUT layout; raw coords address rows top-down
vec3 sampleLut(float ang, float rowf)
{
    float ub = fract((ang + 3.14159265)/(2.0*3.14159265));
    return texture2D(u_lutMap, vec2((1.0 + ub*64.0)/66.0, 1.0 - (rowf + 0.5)/24.0)).rgb;
}

float band(float d, float c, float w, float s)
{
    float lo = c - w*0.5;
    float hi = c + w*0.5;
    return smoothstep(lo - s, lo, d)*(1.0 - smoothstep(hi, hi + s, d));
}

// See chip_sdf.frag.metal: SDF-composed chip, world-anchored light elements.
void main()
{
    vec4 dfs = texture2D(u_texture, v_texCoords);
    float pxScale = 2.0*u_dfRange;
    float baseD = (dfs.r - 0.5)*pxScale;
    float iconD = (dfs.g - 0.5)*pxScale;
    float alpha = smoothstep(u_alphaOff - u_alphaSoft, u_alphaOff + u_alphaSoft, baseD);

    vec2 axis = v_normal.xy;
    vec2 T = dot(axis, axis) > 1.0e-8 ? normalize(axis) : vec2(1.0, 0.0);
    vec2 P = vec2(-T.y, T.x);
    vec2 Ls = normalize(vec2(dot(u_lightDir, T), dot(u_lightDir, P)));
    vec2 Bs = normalize(vec2(dot(u_bounceDir, T), dot(u_bounceDir, P)));

    // see chip_sdf.frag.metal: uv space is y-up in this engine
    vec2 uv = v_texCoords;
    vec2 rel = vec2((uv.x - u_center.x)*u_texSize, (uv.y - u_center.y)*u_texSize);
    float Rpx = u_radiusUV*u_texSize;
    vec2 rad = normalize(rel + vec2(1.0e-5, 0.0));
    float azL = dot(rad, Ls);
    float azB = dot(rad, Bs);

    float t = pow(clamp(0.5 + dot(rel, Ls)/(Rpx*u_gradSpread), 0.0, 1.0), u_gradPow);
    vec3 color = mix(u_baseBottom.rgb, u_baseTop.rgb, t);

    float m = band(baseD, u_darkPos, u_darkWidth, u_darkSoft)
            * smoothstep(u_azEdge0, u_azEdge1, -azL)*u_darkStrength;
    color = mix(color, u_darkColor.rgb, m);
    m = band(baseD, u_litPos, u_litWidth, u_litSoft)
      * smoothstep(u_azEdge0, u_azEdge1, azL)*u_litStrength;
    color = mix(color, u_litColor.rgb, m);

    m = band(baseD, u_bounceWidePos, u_bounceWideWidth, u_bounceWideSoft)
      * smoothstep(u_azB0, u_azB1, azB)*u_bounceWideStrength;
    color = mix(color, u_bounceColor.rgb, m);
    m = band(baseD, u_bounceLinePos, u_bounceLineWidth, u_bounceLineSoft)
      * smoothstep(u_azB0, u_azB1, azB)*u_bounceLineStrength;
    color = mix(color, u_bounceLineColor.rgb, m);

    if (u_rimLutBlend > 0.001)
    {
        float angB = atan(rad.x*Ls.y - rad.y*Ls.x, dot(rad, Ls));
        float dn = baseD/u_rimRange;
        vec3 lutc = sampleLut(angB, 1.0 + clamp(dn, 0.0, 1.0)*8.0);
        float fade = (1.0 - smoothstep(0.75, 1.0, dn))*smoothstep(-3.0, 1.0, baseD)*u_rimLutBlend;
        color = mix(color, lutc, fade);
    }

    // icon drop shadow: simply the icon silhouette offset down-right (world dir)
    vec2 Sd = normalize(vec2(dot(u_shadowDir, T), dot(u_shadowDir, P)));
    vec2 offUV = -Sd*u_shadowOffset/u_texSize;
    float iconOffD = (texture2D(u_texture, uv + offUV).g - 0.5)*pxScale;
    float iconMask = smoothstep(-u_iconAA, u_iconAA, iconD);
    float sh = smoothstep(-u_shadowSoft, u_shadowSoft, iconOffD)*u_shadowStrength*(1.0 - iconMask);
    color = mix(color, u_shadowColor.rgb, sh);

    vec2 Gs = normalize(vec2(dot(u_glintDir, T), dot(u_glintDir, P)));
    vec2 gpos = Gs*Rpx*u_glintRad;
    vec2 gd = (rel - gpos)/vec2(u_glintRx, u_glintRy);
    float g = pow(clamp(1.0 - length(gd), 0.0, 1.0), u_glintPow)*u_glintStrength;
    color = mix(color, u_glintColor.rgb, g);

    float t2 = pow(clamp(0.5 + dot(rel, Ls)/(Rpx*u_iconGradSpread), 0.0, 1.0), u_iconGradPow);
    vec3 icol = mix(u_iconBottom.rgb, u_iconTop.rgb, t2);

    vec2 gdir = normalize(dfs.ba*2.0 - 1.0 + vec2(1.0e-6, 0.0));
    float iaz = dot(gdir, Ls);

    m = band(iconD, u_iconLitPos, u_iconLitWidth, u_iconLitSoft)
      * smoothstep(u_iazEdge0, u_iazEdge1, iaz)*u_iconLitStrength;
    icol = mix(icol, u_iconLitColor.rgb, m);
    m = band(iconD, u_iconShadePos, u_iconShadeWidth, u_iconShadeSoft)
      * smoothstep(u_iazEdge0, u_iazEdge1, -iaz)*u_iconShadeStrength;
    icol = mix(icol, u_iconShadeColor.rgb, m);
    m = band(iconD, u_iconBouncePos, u_iconBounceWidth, u_iconBounceSoft)
      * smoothstep(u_iazEdge0, u_iazEdge1, dot(gdir, Bs))*u_iconBounceStrength;
    icol = mix(icol, u_iconBounceColor.rgb, m);

    m = band(iconD, u_iconGlintPos, u_iconGlintWidth, u_iconGlintSoft)
      * smoothstep(u_iglAz0, u_iglAz1, iaz)*u_iconGlintStrength;
    icol = mix(icol, u_iconGlintColor.rgb, m);

    if (u_iconLutBlend > 0.001)
    {
        float angI = atan(gdir.x*Ls.y - gdir.y*Ls.x, iaz);
        float dni = iconD/20.0;
        vec3 lutci = sampleLut(angI, 14.0 + clamp(dni, 0.0, 1.0)*8.0);
        float fadei = (1.0 - smoothstep(0.7, 1.0, dni))*smoothstep(-1.0, 1.5, iconD)*u_iconLutBlend;
        icol = mix(icol, lutci, fadei);
    }

    color = mix(color, icol, iconMask);

    for (int i = 0; i < 4; i++)
    {
        float st = u_dotStrengths[i];
        if (st <= 0.001)
            continue;
        vec4 dv = u_dots[i];
        vec2 Ds = normalize(vec2(dot(dv.xy, T), dot(dv.xy, P)) + vec2(1.0e-6, 0.0));
        vec2 dpos = Ds*dv.z;
        float dd = length(rel - dpos)/max(dv.w, 1.0e-3);
        float gg = pow(1.0 - smoothstep(0.0, 1.0, dd), 1.5)*st;
        color = mix(color, u_dotColor.rgb, gg);
    }

    gl_FragColor = vec4(color, alpha)*v_color;
}
