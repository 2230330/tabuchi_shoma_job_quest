// ======================================
// ibl_brdf_lut_cs.hlsl
// Generate BRDF LUT (Split-Sum integration)
// Output: RG16F
// ======================================

RWTexture2D<float2> BrdfLUT : register(u0);

static const float PI = 3.14159265359;

// ------------------------------------------------------------
// Hammersley sequence
// ------------------------------------------------------------
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// ------------------------------------------------------------
// GGX importance sampling
// ------------------------------------------------------------
float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N)
{
    float a = roughness * roughness;
    float a2 = a * a;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // Tangent space Å® world space
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// ------------------------------------------------------------
// Geometry term (Smith GGX)
// ------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// ------------------------------------------------------------
// BRDF integration (Split-Sum)
// ------------------------------------------------------------
float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 N = float3(0, 0, 1);
    float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0, NdotV);

    const uint SAMPLE_COUNT = 1024;
    float A = 0.0;
    float B = 0.0;

    for (uint i = 0; i < SAMPLE_COUNT; i++)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, roughness, N);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));

        if (NdotL > 0.0)
        {
            float G = GeometrySmith(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 1e-4);

            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    return float2(A, B) / SAMPLE_COUNT;
}

// ------------------------------------------------------------
// Compute shader entry
// ------------------------------------------------------------
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    BrdfLUT.GetDimensions(width, height);

    if (id.x >= width || id.y >= height)
        return;

    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    float NdotV = saturate(uv.x);
    float roughness = saturate(uv.y);

    BrdfLUT[id.xy] = IntegrateBRDF(NdotV, roughness);
}
