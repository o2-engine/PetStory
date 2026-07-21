varying vec4 v_color;
varying vec2 v_texCoords;
varying vec3 v_normal;

uniform sampler2D u_texture;
uniform sampler2D u_lutMap;

uniform vec4 u_grad;        // gradSpread, gradPow, iconGradSpread, iconGradPow
uniform vec4 u_baseTop;    uniform vec4 u_baseBottom;
uniform vec4 u_iconTop;    uniform vec4 u_iconBottom;
uniform vec4 u_shadowColor; // rgb, a = strength
uniform vec4 u_glint;       // dirX, dirY, radiusFrac, pow
uniform vec4 u_glintEx;     // rx, ry, strength, unused
uniform vec4 u_glintColor;
uniform vec4 u_dots[4];     // dirX, dirY, dist, radius
uniform vec4 u_dotStrengths;
uniform vec4 u_dotColor;

// Geometry shared by every chip: world light, DF layout, alpha fit, shadow style.
#define LIGHT_DIR vec2(-0.3292, 0.9443)
#define SHADOW_DIR vec2(0.7071, -0.7071)
#define CENTER vec2(0.49881, 0.5012)
#define RADIUS_UV 0.50076
#define TEX_SIZE 420.0
#define PX_SCALE 256.0
#define ALPHA_OFF 0.3
#define ALPHA_SOFT 1.63571
#define RIM_RANGE 40.0
#define ICON_RANGE 20.0
#define SHADOW_OFFSET 30.0
#define SHADOW_SOFT 3.0
#define ICON_AA 2.5

// see chip_sdf.frag.metal for the 66x24 LUT layout; raw coords address rows top-down
vec3 sampleLut(float ang, float rowf)
{
    float ub = fract((ang + 3.14159265)/(2.0*3.14159265));
    return texture2D(u_lutMap, vec2((1.0 + ub*64.0)/66.0, 1.0 - (rowf + 0.5)/24.0)).rgb;
}

// SDF-composed chip: gradient body + reference rim/edge LUT + offset shadow + glints.
void main()
{
    vec4 dfs = texture2D(u_texture, v_texCoords);
    float baseD = (dfs.r - 0.5)*PX_SCALE;
    float iconD = (dfs.g - 0.5)*PX_SCALE;
    float alpha = smoothstep(ALPHA_OFF - ALPHA_SOFT, ALPHA_OFF + ALPHA_SOFT, baseD);

    vec2 axis = v_normal.xy;
    vec2 T = dot(axis, axis) > 1.0e-8 ? normalize(axis) : vec2(1.0, 0.0);
    vec2 P = vec2(-T.y, T.x);
    vec2 Ls = normalize(vec2(dot(LIGHT_DIR, T), dot(LIGHT_DIR, P)));

    // uv space is y-up in this engine
    vec2 uv = v_texCoords;
    vec2 rel = (uv - CENTER)*TEX_SIZE;
    float Rpx = RADIUS_UV*TEX_SIZE;
    vec2 rad = normalize(rel + vec2(1.0e-5, 0.0));

    float t = pow(clamp(0.5 + dot(rel, Ls)/(Rpx*u_grad.x), 0.0, 1.0), u_grad.y);
    vec3 color = mix(u_baseBottom.rgb, u_baseTop.rgb, t);

    float angB = atan(rad.x*Ls.y - rad.y*Ls.x, dot(rad, Ls));
    float dn = baseD/RIM_RANGE;
    vec3 lutc = sampleLut(angB, 1.0 + clamp(dn, 0.0, 1.0)*8.0);
    float fade = (1.0 - smoothstep(0.75, 1.0, dn))*smoothstep(-3.0, 1.0, baseD);
    color = mix(color, lutc, fade);

    // icon drop shadow: the icon silhouette offset along the world shadow dir
    vec2 Sd = normalize(vec2(dot(SHADOW_DIR, T), dot(SHADOW_DIR, P)));
    vec2 offUV = -Sd*SHADOW_OFFSET/TEX_SIZE;
    float iconOffD = (texture2D(u_texture, uv + offUV).g - 0.5)*PX_SCALE;
    float iconMask = smoothstep(-ICON_AA, ICON_AA, iconD);
    float sh = smoothstep(-SHADOW_SOFT, SHADOW_SOFT, iconOffD)*u_shadowColor.a*(1.0 - iconMask);
    color = mix(color, u_shadowColor.rgb, sh);

    vec2 Gs = normalize(vec2(dot(u_glint.xy, T), dot(u_glint.xy, P)));
    vec2 gpos = Gs*Rpx*u_glint.z;
    vec2 gd = (rel - gpos)/u_glintEx.xy;
    float g = pow(clamp(1.0 - length(gd), 0.0, 1.0), u_glint.w)*u_glintEx.z;
    color = mix(color, u_glintColor.rgb, g);

    float t2 = pow(clamp(0.5 + dot(rel, Ls)/(Rpx*u_grad.z), 0.0, 1.0), u_grad.w);
    vec3 icol = mix(u_iconBottom.rgb, u_iconTop.rgb, t2);

    vec2 gdir = normalize(dfs.ba*2.0 - 1.0 + vec2(1.0e-6, 0.0));
    float angI = atan(gdir.x*Ls.y - gdir.y*Ls.x, dot(gdir, Ls));
    float dni = iconD/ICON_RANGE;
    vec3 lutci = sampleLut(angI, 14.0 + clamp(dni, 0.0, 1.0)*8.0);
    float fadei = (1.0 - smoothstep(0.7, 1.0, dni))*smoothstep(-1.0, 1.5, iconD);
    icol = mix(icol, lutci, fadei);

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
