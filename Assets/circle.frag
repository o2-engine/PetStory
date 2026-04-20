varying vec4 v_color;
varying vec2 v_uv;
varying vec3 v_normal;

uniform sampler2D u_texture;
const float brightScale = 1.2;
const float darkScale = 0.98;

const vec2 edgeShine1Scale = vec2(0.9, 1.0);
const vec2 edgeShineAngle = vec2(1.0, 2.0);

const float edgeShine = 0.15;
const float edgeShine2 = 0.3;
const float edgeDarken = 0.1;

const vec2 edge2ShineScale = vec2(0.95, 1.2);
const vec2 edge2ShineAngle = vec2(1.0, 1.4);


float circle(vec2 coords)
{
    float dist = length(coords);
    return 1.0 - smoothstep(0.98, 1.0, dist);
}

vec4 circleColor()
{
    vec2 coords = rotate(v_uv, v_normal.xy);

    vec2 edge1ShineCoords = rotate(coords, normalize(edgeShineAngle));
    vec2 edge2ShineCoords = rotate(coords, normalize(edge2ShineAngle));
    
    float grad = (-coords.x + coords.y)*0.5;
    float scale = mix(darkScale, brightScale, grad);

    float alpha = circle(coords);

    vec4 color = vec4(v_color.rgb * scale, v_color.a * alpha);

    if (edge1ShineCoords.x < 0.0)
    {
        float edgeShine1 = (1.0 - circle(edge1ShineCoords/edgeShine1Scale))*edgeShine;
        color.xyz = color.xyz + edgeShine1;
    }
    else
    {
        float edgeDarken1 = (1.0 - circle(edge1ShineCoords/edgeShine1Scale))*edgeDarken;
        float edgeShine2 = (1.0 - circle(edge2ShineCoords/edge2ShineScale))*edgeShine2;
        color.xyz = color.xyz - edgeDarken1 + edgeShine2;
    }

    return color;
}

#define M_PI 3.1415926535897932384626433832795

vec2 rotate(vec2 v, vec2 a)
{
    vec2 an = normalize(a);
    return vec2(an*v.x + vec2(-an.y, an.x)*v.y);
}

vec4 pxl(vec2 coords)
{
    return texture2D(u_texture, coords/2.0 + 0.5);
}

vec4 circleDst(vec2 coords)
{
    float dist = length(coords);
    float d = 1.0 - dist;
    return vec4(d, d, d, 1.0 - smoothstep(0.98, 1.0, dist));
}

float radialShine(float at, float beg, float end, float pdst, float dstThreshold)
{
    float f = (clamp(at, beg, end) - beg)/(end - beg);
    if (f > 0.5)
        f = 1.0 - f;

    float result = 1.0;

    if (pdst > f*dstThreshold)
        result = 0.0;

    return result;
}

vec4 pxlColor(vec2 uv, vec2 coords, vec4 ppxl)
{
    float alpha = ppxl.a;

    float at = (atan(coords.x, coords.y)/M_PI + 1.0)*0.5;

    float rs = radialShine(at, 0.2, 0.7, ppxl.r, 0.5);
    float off = mix(1.0, 1.2, rs);

    float rd = radialShine(at, 0.8, 1.0, ppxl.r, 0.7);
    float off2 = mix(1.0, 0.9, rd);

    float rs2 = radialShine(at, 0.8, 1.0, ppxl.r, 0.3);
    float off3 = mix(1.0, 1.2, rs2);

    float grad = (-coords.x + coords.y)*0.5;
    float scale = mix(0.9, 1.2, grad);
    vec4 color = vec4(v_color.rgb * off * off2 * off3 * scale, v_color.a * alpha);

    //float cc = cos(at*M_PI/2.0);
    //color = vec4(cc, cc, cc, 1.0);

    return color;
}

void main()
{
    vec2 coords = rotate(v_uv, v_normal.xy);

    vec4 circleColor = pxlColor(v_uv, coords, circleDst(coords));
    vec4 pxlColor = pxlColor(v_uv, coords, pxl(v_uv));
    
    vec2 shadowOffs = rotate(vec2(-1, -1), vec2(v_normal.x, -v_normal.y));
    vec4 shadowColor = vec4(0, 0, 0, pxl(v_uv + shadowOffs*0.1).a*0.2);
    
    gl_FragColor = mix(mix(circleColor, shadowColor, shadowColor.a), pxlColor, pxlColor.a);
}
