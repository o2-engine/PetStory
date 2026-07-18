varying vec4 v_color;
varying vec2 v_texCoords;

uniform sampler2D u_texture;

uniform vec4 u_edgeColor;
uniform float u_edgeWidth;
uniform float u_progress;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7)))*43758.5453);
}

float vnoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f*f*(3.0 - 2.0*f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

void main()
{
    vec4 tex = texture2D(u_texture, v_texCoords)*v_color;

    float n = vnoise(v_texCoords*40.0)*0.65 + vnoise(v_texCoords*90.0)*0.35;
    float d = n + u_edgeWidth - u_progress*(1.0 + u_edgeWidth*2.0);

    float body = step(u_edgeWidth, d);
    float edge = clamp(smoothstep(0.0, u_edgeWidth, d) - body, 0.0, 1.0);

    vec4 color = tex*body + vec4(u_edgeColor.rgb, 1.0)*tex.a*edge;
    color.a *= smoothstep(0.0, u_edgeWidth*0.5, d);

    gl_FragColor = color;
}
