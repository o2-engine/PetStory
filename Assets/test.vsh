uniform mat4 u_transformMatrix;
uniform float u_brightness;
uniform vec2 u_uvOffset;

attribute vec4 a_position;
attribute vec4 a_color;
attribute vec2 a_texCoords;

varying vec4 v_color;
varying vec2 v_texCoords;

void main()
{
    v_color = a_color;
    v_texCoords = a_texCoords + u_uvOffset;
    gl_Position = u_transformMatrix * a_position;
}
