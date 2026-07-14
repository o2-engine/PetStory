varying vec4 v_color;
varying vec2 v_texCoords;
varying vec3 v_normal;

uniform sampler2D u_texture;
uniform sampler2D u_lightMap;

void main()
{
    vec4 tex = texture2D(u_texture, v_texCoords);

    vec3 nTangent = normalize(tex.rgb * 2.0 - 1.0);

    vec2 T = normalize(v_normal.xy);
    vec2 B = vec2(-T.y, T.x);
    vec3 normal = normalize(vec3(
        nTangent.x * T.x + nTangent.y * B.x,
        nTangent.x * T.y + nTangent.y * B.y,
        nTangent.z
    ));

    vec2 lightUV = normal.xy * 0.5 + 0.5;
    vec4 lightColor = texture2D(u_lightMap, lightUV);

    gl_FragColor = vec4(lightColor.rgb, tex.a * v_color.a);
}
