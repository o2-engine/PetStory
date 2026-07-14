varying vec4 v_color;
varying vec2 v_texCoords;
varying vec2 v_texCoords2;

uniform sampler2D u_texture;
uniform sampler2D u_texture2;

uniform float u_brightness;
uniform vec4 u_outlineColor;
uniform float u_outlineRadius;  // в пикселях
uniform vec2 u_texelSize;       // 1.0 / размер основной текстуры (width, height)

void main()
{
    vec4 tex = texture2D(u_texture, v_texCoords);
    float centerAlpha = tex.a;

    // Смещение в UV за один пиксель радиуса
    vec2 pixelOffset = u_outlineRadius * u_texelSize;

    // Сэмплы соседей для обводки (8 направлений, радиус в пикселях)
    float maxNeighborAlpha = centerAlpha;
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2( pixelOffset.x,  0.0)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2(-pixelOffset.x,  0.0)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2( 0.0,  pixelOffset.y)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2( 0.0, -pixelOffset.y)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2( pixelOffset.x,  pixelOffset.y)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2(-pixelOffset.x,  pixelOffset.y)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2( pixelOffset.x, -pixelOffset.y)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2(-pixelOffset.x, -pixelOffset.y)).a);

    const float edgeThreshold = 0.5;
    vec4 result;
    if (centerAlpha > edgeThreshold)
        result = v_color * tex * u_brightness;
    else if (maxNeighborAlpha > edgeThreshold)
        result = u_outlineColor;
    else
        result = vec4(0.0, 0.0, 0.0, 0.0);

    // Вторая текстура поверх (overlay по альфе)
    vec4 tex2 = texture2D(u_texture2, v_texCoords2);
    result = mix(result, tex2 * v_color, tex2.a);

    gl_FragColor = result;
}
