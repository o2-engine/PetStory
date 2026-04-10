struct O2MaterialParams
{
    float4 u_color;
    float2 u_lightDir;
    float u_ambientStrength;
    float u_specularStrength;
    float u_shininess;
    float u_fresnelPower;
    float u_fresnelStrength;
    float u_fillLightStrength;
    float u_sssStrength;
    float u_sssDistortion;
    float4 u_sssColor;
};

fragment float4 fragmentShader(O2RasterizerData input [[stage_in]],
                               constant O2MaterialParams& materialParams [[buffer(2)]],
                               texture2d<float> u_texture [[texture(0)]])
{
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);

    float4 tex = u_texture.sample(textureSampler, input.texCoords);
    float3 normal = normalize(tex.rgb * 2.0 - 1.0);
    float3 viewDir = float3(0.0, 0.0, 1.0);

    float lz = sqrt(max(1.0 - dot(materialParams.u_lightDir, materialParams.u_lightDir), 0.0));
    float3 lightDir = normalize(float3(materialParams.u_lightDir, lz));

    float diff = dot(normal, lightDir) * 0.5 + 0.5;

    float3 fillDir = normalize(float3(-materialParams.u_lightDir, lz));
    float fillDiff = max(dot(normal, fillDir), 0.0) * materialParams.u_fillLightStrength;

    float3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), materialParams.u_shininess);

    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), materialParams.u_fresnelPower) * materialParams.u_fresnelStrength;

    float sssBack = clamp(-dot(normal, lightDir) + materialParams.u_sssDistortion, 0.0, 1.0);
    float sssRim = pow(1.0 - max(dot(normal, viewDir), 0.0), 2.0);
    float3 sss = materialParams.u_sssColor.rgb * sssBack * (0.5 + 0.5 * sssRim) * materialParams.u_sssStrength;

    float3 result = materialParams.u_color.rgb * (materialParams.u_ambientStrength + diff + fillDiff)
                  + float3(materialParams.u_specularStrength * spec)
                  + float3(fresnel)
                  + sss;

    return float4(result, tex.a * materialParams.u_color.a) * input.color;
}