uniform mat4 u_transformMatrix;
uniform float u_brightness;

attribute vec4 a_position;
attribute vec4 a_color;
attribute vec2 a_texCoords;
attribute vec2 a_texCoords2;

varying vec4 v_color;
varying vec2 v_texCoords;
varying vec2 v_texCoords2;

void main()
{
    v_color = a_color;
    v_texCoords = a_texCoords;
    v_texCoords2 = a_texCoords2;
    gl_Position = u_transformMatrix * a_position;
}
