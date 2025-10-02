#ifndef PART2_FULLSCREEN_QUAD_H
#define PART2_FULLSCREEN_QUAD_H

#include <d3d11.h>
#include <wrl.h>
#include <cstdint>



//'fullscreen_quad' dose not have pixel shader and sampler state. you have to make and set pixel shader and sampler state by yourself.
class fullscreen_quad
{
public:
	fullscreen_quad(ID3D11Device* device);
	virtual ~fullscreen_quad() = default;

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> embedded_vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> embedded_pixel_shader;

public:
	void blit(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* const* shader_resource_views, uint32_t start_slot, uint32_t num_views, ID3D11PixelShader* replaced_pixel_shader = nullptr);
};


#endif // PART2_FULLSCREEN_QUAD_H
