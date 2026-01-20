struct VSOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

struct GSOutput
{
	float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    uint face : SV_RenderTargetArrayIndex;
};

[maxvertexcount(18)]
void main(
	triangle VSOut input[3], 
	inout TriangleStream< GSOutput > stream
)
{
	//各頂点に対して六面分の出力を行う
    for (uint f = 0; f < 6;++f)
    {
        
        for (uint i = 0; i < 3; i++)
        {
            GSOutput o;
            o.pos = input[i].pos;
            o.uv = input[i].uv;
            o.face = f;//出力先スライスを指定
            stream.Append(o);
        }
        stream.RestartStrip();
    }
}