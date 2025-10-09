#include"../../headers/post_process/bloom.h"

#include<vector>

#include"../../headers/misc.h"
#include"../../headers/render_state.h"
#include"../../external/imgui/imgui.h"

Bloom::Bloom(ID3D11Device* device, uint32_t& width, uint32_t& height, ResourceManager* resource_manager)
    :resource_manager_(resource_manager)
{
    bit_block_transfer_ = std::make_unique<FullscreenQuad>(device);

    glow_extraction_ = std::make_unique<FrameBuffer>(device, width, height);
    for (size_t downsampled_index = 0; downsampled_index < downsampled_count; ++downsampled_index)
    {
        gaussian_blur[downsampled_index][0] = std::make_unique<FrameBuffer>(device, width >> downsampled_index, height >> downsampled_index);
        gaussian_blur[downsampled_index][1] = std::make_unique<FrameBuffer>(device, width >> downsampled_index, height >> downsampled_index);
    }
    glow_extraction_ps_ = resource_manager_->LoadPixelShader(device, L".//resources//shader//glow_extraction_ps.cso");
    gaussian_blur_downsampling_ps_ = resource_manager_->LoadPixelShader(device, L".//resources//shader//downsample_ps.cso");
    gaussian_blur_horizontal_ps_ = resource_manager_->LoadPixelShader(device, L".//resources//shader//gaussian_blur_horizontal_ps.cso");
    gaussian_blur_vertical_ps_ = resource_manager_->LoadPixelShader(device, L".//resources//shader//gaussian_blur_vertical_ps.cso");
    gaussian_blur_upsampling_ps_ = resource_manager_->LoadPixelShader(device, L".//resources//shader//bloom_upsample_ps.cso");

    RenderState render_state(device);
    rasterizer_state_ = render_state.GetRasterizerState(RasterizerState::solid_cull_back);
    depth_stencil_state_ = render_state.GetDepthStencilState(DepthState::no_test_no_write);
    blend_state_ = render_state.GetBlendState(BlendState::additive);

    HRESULT hr{ S_OK };
    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(BloomConstants);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;
    hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	//パラメータの初期値
	bloom_constant_.bloom_extraction_threshold = 0.3f;
	bloom_constant_.bloom_intensity = 1.5f;
	bloom_constant_.bloom_soft_knee = 0.5f;
}

void Bloom::Make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map)
{
	// Store current states
	ID3D11ShaderResourceView* null_shader_resource_view{};
	ID3D11ShaderResourceView* cached_shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	immediate_context->PSGetShaderResources(0, downsampled_count, cached_shader_resource_views);

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  cached_depth_stencil_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>  cached_rasterizer_state;
	Microsoft::WRL::ComPtr<ID3D11BlendState>  cached_blend_state;
	FLOAT blend_factor[4];
	UINT sample_mask;
	immediate_context->OMGetDepthStencilState(cached_depth_stencil_state.GetAddressOf(), 0);
	immediate_context->RSGetState(cached_rasterizer_state.GetAddressOf());
	immediate_context->OMGetBlendState(cached_blend_state.GetAddressOf(), blend_factor, &sample_mask);

	Microsoft::WRL::ComPtr<ID3D11Buffer>  cached_constant_buffer;
	immediate_context->PSGetConstantBuffers(8, 1, cached_constant_buffer.GetAddressOf());

	// Bind states
	immediate_context->OMSetDepthStencilState(depth_stencil_state_.Get(), 0);
	immediate_context->RSSetState(rasterizer_state_.Get());
	immediate_context->OMSetBlendState(blend_state_.Get(), nullptr, 0xFFFFFFFF);

	BloomConstants data{};
	data.bloom_extraction_threshold = bloom_constant_.bloom_extraction_threshold;
	data.bloom_intensity = bloom_constant_.bloom_intensity;
	immediate_context->UpdateSubresource(constant_buffer_.Get(), 0, 0, &data, 0, 0);
	immediate_context->PSSetConstantBuffers(8, 1, constant_buffer_.GetAddressOf());

	// Extracting bright color
	glow_extraction_->Clear(immediate_context);
	glow_extraction_->Activate(immediate_context);
	bit_block_transfer_->blit(immediate_context, &color_map, 0, 1, glow_extraction_ps_.Get());
	glow_extraction_->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

	// Gaussian blur
	// Efficient Gaussian blur with linear sampling
	// http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
	// Downsampling
	gaussian_blur[0][0]->Clear(immediate_context);
	gaussian_blur[0][0]->Activate(immediate_context);
	bit_block_transfer_->blit(immediate_context, glow_extraction_->GetShaderResourceView(0).GetAddressOf(), 0, 1, gaussian_blur_downsampling_ps_.Get());
	gaussian_blur[0][0]->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

	// Ping-pong gaussian blur
	gaussian_blur[0][1]->Clear(immediate_context);
	gaussian_blur[0][1]->Activate(immediate_context);
	bit_block_transfer_->blit(immediate_context, gaussian_blur[0][0]->GetShaderResourceView(0).GetAddressOf(), 0, 1, gaussian_blur_horizontal_ps_.Get());
	gaussian_blur[0][1]->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

	gaussian_blur[0][0]->Clear(immediate_context);
	gaussian_blur[0][0]->Activate(immediate_context);
	bit_block_transfer_->blit(immediate_context, gaussian_blur[0][1]->GetShaderResourceView(0).GetAddressOf(), 0, 1, gaussian_blur_vertical_ps_.Get());
	gaussian_blur[0][0]->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

	for (size_t downsampled_index = 1; downsampled_index < downsampled_count; ++downsampled_index)
	{
		// Downsampling
		gaussian_blur[downsampled_index][0]->Clear(immediate_context);
		gaussian_blur[downsampled_index][0]->Activate(immediate_context);
		bit_block_transfer_->blit(immediate_context, gaussian_blur[downsampled_index - 1][0]->GetShaderResourceView(0).GetAddressOf(), 0, 1, gaussian_blur_downsampling_ps_.Get());
		gaussian_blur[downsampled_index][0]->Deactivate(immediate_context);
		immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

		// Ping-pong gaussian blur
		gaussian_blur[downsampled_index][1]->Clear(immediate_context);
		gaussian_blur[downsampled_index][1]->Activate(immediate_context);
		bit_block_transfer_->blit(immediate_context, gaussian_blur[downsampled_index][0]->GetShaderResourceView(0).GetAddressOf(), 0, 1, gaussian_blur_horizontal_ps_.Get());
		gaussian_blur[downsampled_index][1]->Deactivate(immediate_context);
		immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

		gaussian_blur[downsampled_index][0]->Clear(immediate_context);
		gaussian_blur[downsampled_index][0]->Activate(immediate_context);
		bit_block_transfer_->blit(immediate_context, gaussian_blur[downsampled_index][1]->GetShaderResourceView(0).GetAddressOf(), 0, 1, gaussian_blur_vertical_ps_.Get());
		gaussian_blur[downsampled_index][0]->Deactivate(immediate_context);
		immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);
	}

	// Downsampling
	glow_extraction_->Clear(immediate_context);
	glow_extraction_->Activate(immediate_context);
	std::vector<ID3D11ShaderResourceView*> shader_resource_views;
	for (size_t downsampled_index = 0; downsampled_index < downsampled_count; ++downsampled_index)
	{
		shader_resource_views.push_back(gaussian_blur[downsampled_index][0]->GetShaderResourceView(0).Get());
	}
	bit_block_transfer_->blit(immediate_context, shader_resource_views.data(), 0, downsampled_count, gaussian_blur_upsampling_ps_.Get());
	glow_extraction_->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);
	

	// Restore states
	immediate_context->PSSetConstantBuffers(8, 1, cached_constant_buffer.GetAddressOf());

	immediate_context->OMSetDepthStencilState(cached_depth_stencil_state.Get(), 0);
	immediate_context->RSSetState(cached_rasterizer_state.Get());
	immediate_context->OMSetBlendState(cached_blend_state.Get(), blend_factor, sample_mask);

	immediate_context->PSSetShaderResources(0, downsampled_count, cached_shader_resource_views);
	for (ID3D11ShaderResourceView* cached_shader_resource_view : cached_shader_resource_views)
	{
		if (cached_shader_resource_view) cached_shader_resource_view->Release();
	}
}

void Bloom::DrawImgui()
{
	if (ImGui::TreeNode("bloom"))
	{

		ImGui::SliderFloat("extraction_threshold", &bloom_constant_.bloom_extraction_threshold, 0.f, 1.f);
		ImGui::SliderFloat("intensity", &bloom_constant_.bloom_intensity, 0.f, 10.f);
		ImGui::SliderFloat("soft_knee", &bloom_constant_.bloom_soft_knee, 0.f, 1.f);

		ImGui::TreePop();
	}
}
