varying vec4 v_color;
varying vec2 v_texCoords;

uniform sampler2D u_texture;   // normal map (RGB = нормаль, A = альфа)

uniform vec4 u_color;              // цвет окраски
uniform vec2 u_lightDir;           // направление света XY (нормализованное, Z вычисляется)
uniform float u_ambientStrength;   // ambient (0.0–1.0)
uniform float u_specularStrength;  // specular (0.0–1.0)
uniform float u_shininess;         // specular exponent (8–128)
uniform float u_fresnelPower;      // сила френеля (1.0–5.0)
uniform float u_fresnelStrength;   // яркость френеля (0.0–1.0)
uniform float u_fillLightStrength; // сила заполняющего света (0.0–1.0)
uniform float u_sssStrength;       // сила подповерхностного рассеивания (0.0–1.0)
uniform float u_sssDistortion;     // искажение нормали для SSS (0.0–1.0)
uniform vec4 u_sssColor;           // цвет подповерхностного свечения

void main()
{
    vec4 tex = texture2D(u_texture, v_texCoords);

    vec3 normal = normalize(tex.rgb * 2.0 - 1.0);
    vec3 viewDir = vec3(0.0, 0.0, 1.0);

    float lz = sqrt(max(1.0 - dot(u_lightDir, u_lightDir), 0.0));
    vec3 lightDir = normalize(vec3(u_lightDir, lz));

    // Half-Lambert diffuse
    float diff = dot(normal, lightDir) * 0.5 + 0.5;

    // Fill light с противоположной стороны
    vec3 fillDir = normalize(vec3(-u_lightDir, lz));
    float fillDiff = max(dot(normal, fillDir), 0.0) * u_fillLightStrength;

    // Specular (Blinn-Phong), белый блик
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), u_shininess);

    // Fresnel
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), u_fresnelPower) * u_fresnelStrength;

    // Subsurface scattering: свет проходит сквозь области, направленные от источника
    float sssBack = clamp(-dot(normal, lightDir) + u_sssDistortion, 0.0, 1.0);
    float sssRim = pow(1.0 - max(dot(normal, viewDir), 0.0), 2.0);
    vec3 sss = u_sssColor.rgb * sssBack * (0.5 + 0.5 * sssRim) * u_sssStrength;

    vec3 result = u_color.rgb * (u_ambientStrength + diff + fillDiff)
                + vec3(u_specularStrength * spec)
                + vec3(fresnel)
                + sss;

    gl_FragColor = vec4(result, tex.a * u_color.a) * v_color;
}
