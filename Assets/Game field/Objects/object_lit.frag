varying vec4 v_color;
varying vec2 v_texCoords;
varying vec2 v_texCoords2;
varying vec3 v_normal;

uniform sampler2D u_texture;   // albedo: the art with the 0-deg lighting divided out
uniform sampler2D u_nrmMap;    // RG = part/bevel normal.xy (sprite space, y-up), B = silhouette DF

uniform vec4 u_fx;             // x = lighting modulation strength
uniform vec4 u_shadowColor;    // shadow tint: the object's own darkened hue, not black

#define LIGHT_DIR vec2(-0.3292, 0.9443)

// The painted lighting lives in the normals: shade = 1 + k * dot(N, L(angle))
// reproduces the source art exactly at the authored orientation and carries the
// same shading around the world light as the sprite spins.
void main()
{
    vec4 art = texture2D(u_texture, v_texCoords);
    vec2 n = texture2D(u_nrmMap, v_texCoords2).rg*2.0 - 1.0;

    vec2 axis = v_normal.xy;
    vec2 T = dot(axis, axis) > 1.0e-8 ? normalize(axis) : vec2(1.0, 0.0);
    vec2 P = vec2(-T.y, T.x);
    vec2 Ls = normalize(vec2(dot(LIGHT_DIR, T), dot(LIGHT_DIR, P)));

    float d = dot(n, Ls);
    float shade = min(1.0 + u_fx.x*max(d, 0.0), 1.7);
    float sh = min(u_fx.x*max(-d, 0.0), 0.4);
    vec3 color = art.rgb*shade*(1.0 - sh) + u_shadowColor.rgb*sh;
    gl_FragColor = vec4(color, art.a)*v_color;
}
