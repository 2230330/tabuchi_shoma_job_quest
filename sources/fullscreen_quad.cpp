#include"../headers/fullscreen_quad.h"
#include"../headers/shader.h"
#include"../headers/misc.h"

FullscreenQuad::FullscreenQuad(ID3D11Device* device)
{
	shader_from_cso::CreateVsFromCso(device, ".\\resources\\shader\\fullscreen_quad_vs.cso",embedded_vertex_shader.GetAddressOf(), NULL, NULL, 0);
	shader_from_cso::CreatePsFromCso(device, ".\\resources\\shader\\fullscreen_quad_ps.cso",embedded_pixel_shader.GetAddressOf());

	render_state = std::make_unique<RenderState>(device);
}
void FullscreenQuad::blit(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* const* shader_resource_views, uint32_t start_slot, uint32_t num_views, ID3D11PixelShader* replaced_pixel_shader)
{
	ID3D11ShaderResourceView* cached_shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	immediate_context->PSGetShaderResources(start_slot, num_views, cached_shader_resource_views);

	immediate_context->PSSetShaderResources(start_slot, num_views, shader_resource_views);

	immediate_context->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	immediate_context->IASetInputLayout(NULL);

	render_state->GetSamplerState(SamplerState::linear_clamp);

	immediate_context->VSSetShader(embedded_vertex_shader.Get(), 0, 0);
	replaced_pixel_shader ? immediate_context->PSSetShader(replaced_pixel_shader, 0, 0) : immediate_context->PSSetShader(embedded_pixel_shader.Get(), 0, 0);
	immediate_context->PSSetShaderResources(start_slot, num_views, shader_resource_views);

	immediate_context->Draw(4, 0);

	immediate_context->PSSetShaderResources(start_slot, num_views, cached_shader_resource_views);
	for (ID3D11ShaderResourceView* cached_shader_resource_view : cached_shader_resource_views)
	{
		if (cached_shader_resource_view) cached_shader_resource_view->Release();
	}
}
