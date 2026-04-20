uniform mat4 u_transformMatrix;

attribute vec4 a_position;
attribute vec4 a_color;
attribute vec2 a_texCoords;
attribute vec3 a_normal;

varying vec4 v_color;
varying vec2 v_texCoords;
varying vec3 v_normal;

void main()
{
    v_color = a_color;
    v_texCoords = a_texCoords;
    v_normal = (u_transformMatrix * vec4(a_normal.xy, 0.0, 0.0)).xyz;
    gl_Position = u_transformMatrix * a_position;
}
