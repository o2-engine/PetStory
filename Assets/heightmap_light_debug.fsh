varying vec4 v_color;
varying vec2 v_texCoords;
varying vec3 v_normal;

uniform sampler2D u_texture;   // normal map (RGB = нормаль, A = альфа)

uniform vec2 u_lightDir;
uniform float u_lightHeight;
uniform float u_lightPower;
uniform float u_ambientStrength;
uniform float u_specularStrength;
uniform float u_shininess;
uniform float u_fresnelPower;
uniform float u_fresnelStrength;
uniform float u_fillLightStrength;
uniform float u_sssStrength;
uniform float u_sssDistortion;
uniform vec4 u_sssColor;

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

    vec2 lightXY = normalize(u_lightDir);
    vec3 lightDir = normalize(vec3(lightXY * (1.0 - u_lightHeight), u_lightHeight));

    float diff = max(dot(normal, lightDir), 0.0);

    vec3 result = v_color.rgb * (u_ambientStrength + diff * u_lightPower);

    gl_FragColor = vec4(result, tex.a * v_color.a);
}
