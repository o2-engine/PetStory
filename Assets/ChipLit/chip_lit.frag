varying vec4 v_color;
varying vec2 v_texCoords;
varying vec2 v_texCoords2;
varying vec3 v_normal;

uniform sampler2D u_texture;
uniform sampler2D u_normalMap;

uniform vec2 u_lightDir;
uniform float u_ambient;
uniform float u_diffuse;
uniform float u_diffusePow;
uniform float u_specular;
uniform float u_shininess;
uniform float u_specular2;
uniform float u_shininess2;
uniform float u_rim;
uniform float u_rimPow;
uniform float u_fill;
uniform float u_fillPow;
uniform float u_edgeLight;
uniform float u_edgePow;
uniform vec4 u_shadowColor;
uniform vec4 u_specColor;

// See chip_lit.frag.metal: world-fixed light over a sprite-space normal map,
// rotated by the sprite world X axis stored in the vertex normal.
void main()
{
    vec4 albedo = texture2D(u_texture, v_texCoords);
    vec3 localN = texture2D(u_normalMap, v_texCoords2).xyz*2.0 - 1.0;

    vec2 axis = v_normal.xy;
    vec2 T = dot(axis, axis) > 1.0e-8 ? normalize(axis) : vec2(1.0, 0.0);
    vec2 B = vec2(-T.y, T.x);
    vec3 N = normalize(vec3(T*localN.x + B*localN.y, localN.z));

    float lz = sqrt(max(1.0 - dot(u_lightDir, u_lightDir), 0.0));
    vec3 L = normalize(vec3(u_lightDir, lz));
    vec3 V = vec3(0.0, 0.0, 1.0);
    vec3 H = normalize(L + V);

    float ndl = dot(N, L);
    float ndv = clamp(dot(N, V), 0.0, 1.0);

    float diff = pow(clamp(ndl*0.5 + 0.5, 0.0, 1.0), u_diffusePow);
    float ndh = clamp(dot(N, H), 0.0, 1.0);
    float spec = pow(ndh, u_shininess)*u_specular + pow(ndh, u_shininess2)*u_specular2;
    float rim = pow(1.0 - ndv, u_rimPow)*clamp(0.5 - 0.5*ndl, 0.0, 1.0)*u_rim;
    float fill = pow(clamp(-ndl, 0.0, 1.0), 2.0)*pow(1.0 - ndv, u_fillPow)*u_fill;
    // grazing sheen: bright arc on the lit side of the silhouette rim
    vec2 edgeDir = normalize(N.xy + vec2(1.0e-6, 0.0));
    vec2 lightXY = normalize(u_lightDir + vec2(1.0e-6, 0.0));
    float edge = pow(1.0 - ndv, 3.0)*pow(clamp(dot(edgeDir, lightXY), 0.0, 1.0), u_edgePow)*u_edgeLight;

    vec3 base = albedo.rgb;
    // candy-style shading: shadow shifts toward a saturated dark tint instead of
    // multiplying brightness down, keeping the color vivid on the dark side
    vec3 shadow = base*base*u_shadowColor.rgb;
    vec3 lit = mix(shadow, base, clamp(u_ambient + u_diffuse*diff, 0.0, 1.0));
    lit = mix(lit, shadow, rim);
    lit += spec*u_specColor.rgb;
    lit += base*fill;
    lit += edge*u_specColor.rgb;

    gl_FragColor = vec4(clamp(lit, 0.0, 1.0), albedo.a)*v_color;
}
