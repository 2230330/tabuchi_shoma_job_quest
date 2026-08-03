#include"fullscreen_quad.hlsli"

static const float phi_inv = 0.618033988;

cbuffer GOLDEN_SPIRAL : register(b5)
{
    int orientation;
}

float DrawLine(float value,float pos,float width)
{
    float d = abs(value - pos);
    return 1.0f - smoothstep(width, width * 2.0f, d);
}


float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;


    switch (orientation)
    {
        case 0: // 左上
            break;
        case 1: // 右上
            uv.x = 1.0f - uv.x;
            break;
        case 2: // 右下
            uv.x = 1.0f - uv.x;
            uv.y = 1.0f - uv.y;
            break;
        case 3: // 左下
            uv.y = 1.0f - uv.y;
            break;
    }
    
    float golden_line = 0;
    
    float x1 = 1.0f - phi_inv;
    float x2 = phi_inv;
    float x3 = x1 * phi_inv;
    float x4 = phi_inv + ((1.0f - x2) * (1.0f - phi_inv));
    float x5 = x1 - ((x1 - x3) * phi_inv);
    float x6 = x2 + ((x4 - x2) * phi_inv);
    float x7 = x3 + ((x5 - x3) * phi_inv);
    float x8 = x4 - ((x4 - x6) * phi_inv);
    
    golden_line += DrawLine(uv.x, x1, 0.001f);
    //区切った中にさらに黄金比の線を描く
    if ((uv.y < x1 || x2 < uv.y))
    {
        
        if ((x3 < uv.y && uv.y < x1) 
            || (uv.y < x4 && uv.y > x2))
        {
            golden_line += DrawLine(uv.x, x3, 0.001f);

            if ((uv.y > x3 && uv.y < x5) || (uv.y > x6 && uv.y < x4))
            {
                golden_line += DrawLine(uv.x, x5, 0.001f);
                if ((uv.y < x5 && uv.y > x7) || (uv.y < x8 && uv.y > x6))
                {
                    golden_line += DrawLine(uv.x, x7, 0.001f);
                }
            }
        }
    }
    //右側の縦線
    golden_line += DrawLine(uv.x, x2, 0.001f);
    if ((uv.y < x1 || x2 < uv.y))
    {
        if ((x3 < uv.y && uv.y < x1) || (uv.y < x4 && uv.y > x2))
        {
            golden_line += DrawLine(uv.x, x4, 0.001f);

            if ((uv.y > x3 && uv.y < x5) || (uv.y > x6 && uv.y < x4))
            {
                golden_line += DrawLine(uv.x, x6, 0.001f);

                if ((uv.y < x5 && uv.y > x7) || (uv.y < x8 && uv.y > x6))
                {
                    golden_line += DrawLine(uv.x, x8, 0.001f);
                }
            }
        }
    }
    //上側の横線
    golden_line += DrawLine(uv.y, x1, 0.001f);
    if ((uv.x < x1 || x2 < uv.x))
    {
        
        if ((x3 < uv.x && uv.x < x1) || (uv.x < x4 && uv.x > x2))
        {
            golden_line += DrawLine(uv.y, x3, 0.001f);

            if ((uv.x > x3 && uv.x < x5) || (uv.x > x6 && uv.x < x4))
            {
                golden_line += DrawLine(uv.y, x5, 0.001f);
                if ((uv.x < x5 && uv.x > x7) || (uv.x < x8 && uv.x > x6))
                {
                    golden_line += DrawLine(uv.y, x7, 0.001f);
                }
            }
        }
    }
    golden_line += DrawLine(uv.y, x2, 0.001f);

    if ((uv.x < x1 || x2 < uv.x))
    {
        if ((x3 < uv.x && uv.x < x1) || (uv.x < x4 && uv.x > x2))
        {
            golden_line += DrawLine(uv.y, x4, 0.001f);

            if ((uv.x > x3 && uv.x < x5) || (uv.x > x6 && uv.x < x4))
            {
                golden_line += DrawLine(uv.y, x6, 0.001f);

                if ((uv.x < x5 && uv.x > x7) || (uv.x < x8 && uv.x > x6))
                {
                    golden_line += DrawLine(uv.y, x8, 0.001f);
                }
            }
        }
    }
    
    //1.0fを超えないようにする
    golden_line = saturate(golden_line);
    
    float4 guide = float4(1.0f, 0.8f, 0.1f, 0.0f);
    
    return lerp(float4(.0f, .0f, .0f, .0f), guide, golden_line);
}