varying vec4 v_color;
varying vec2 v_texCoords;

uniform sampler2D u_texture;
uniform float u_brightness;
uniform vec4 u_outlineColor;
uniform float u_outlineRadius;

void main()
{
    vec4 tex = texture2D(u_texture, v_texCoords);
    float centerAlpha = tex.a;

    // Sample neighbors for outline (8 directions at u_outlineRadius in UV space)
    float maxNeighborAlpha = centerAlpha;
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2( u_outlineRadius,  0.0)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2(-u_outlineRadius,  0.0)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2( 0.0,  u_outlineRadius)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2( 0.0, -u_outlineRadius)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2( u_outlineRadius,  u_outlineRadius)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2(-u_outlineRadius,  u_outlineRadius)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2( u_outlineRadius, -u_outlineRadius)).a);
    maxNeighborAlpha = max(maxNeighborAlpha, texture2D(u_texture, v_texCoords + vec2(-u_outlineRadius, -u_outlineRadius)).a);

    const float edgeThreshold = 0.5;
    vec4 result;
    if (centerAlpha > edgeThreshold)
        result = v_color * tex * u_brightness;
    else if (maxNeighborAlpha > edgeThreshold)
        result = u_outlineColor;
    else
        result = vec4(0.0, 0.0, 0.0, 0.0);

    gl_FragColor = result;
}
