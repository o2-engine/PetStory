uniform mat4 u_transformMatrix;

attribute vec4 a_position;
attribute vec4 a_color;
attribute vec2 a_texCoords;
attribute vec3 a_normal;

varying vec4 v_color;
varying vec2 v_uv;
varying vec3 v_normal;

void main()
{
    v_color = a_color;
    v_uv = a_texCoords * 2.0 - 1.0;
    gl_Position = u_transformMatrix * a_position;
    v_normal = a_normal;
}
