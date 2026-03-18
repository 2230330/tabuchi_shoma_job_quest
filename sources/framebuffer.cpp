#include"../headers/framebuffer.h"

#include"../headers/misc.h"

inline bool operator& (FrameBuffer::usage lhs, FrameBuffer::usage rhs)
{
    return static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs);
}
FrameBuffer::FrameBuffer(
	ID3D11Device* device,
	uint32_t width,
	uint32_t height,
	usage flags ,
	bool shadow_flag,
	bool enable_msaa,
	int subsamples,
	bool generate_mips
)
{
	if (shadow_flag)
	{
		enable_msaa = false;
	}

	HRESULT hr{ S_OK };

	DXGI_FORMAT rt_format =
		(!shadow_flag) ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32_FLOAT;
	UINT msaa_quality_level;
	UINT sample_count = subsamples;
	hr = device->CheckMultisampleQualityLevels(rt_format, sample_count, &msaa_quality_level);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	if (flags & usage::color)
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = width;
		texture2d_desc.Height = height;
		texture2d_desc.MipLevels = generate_mips ? 0 : 1; 
		texture2d_desc.ArraySize = 1;
		texture2d_desc.Format = rt_format;
		texture2d_desc.SampleDesc.Count = enable_msaa ? sample_count : 1; 
		texture2d_desc.SampleDesc.Quality = enable_msaa ? msaa_quality_level - 1 : 0; 
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = generate_mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0; 
		hr = device->CreateTexture2D(&texture2d_desc, 0, render_target_texture_2d_.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc{};
		render_target_view_desc.Format = texture2d_desc.Format;
		render_target_view_desc.ViewDimension = enable_msaa ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D; 
		hr = device->CreateRenderTargetView(render_target_texture_2d_.Get(), &render_target_view_desc, render_target_view_.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
		shader_resource_view_desc.Format = texture2d_desc.Format;
		shader_resource_view_desc.ViewDimension = enable_msaa ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D; 
		shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
		shader_resource_view_desc.Texture2D.MipLevels = generate_mips ? -1 : 1; 
		hr = device->CreateShaderResourceView(render_target_texture_2d_.Get(), &shader_resource_view_desc, shader_resource_views_[0].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}
	if (flags & usage::depth_stencil)
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_stencil_buffer;
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = width;
		texture2d_desc.Height = height;
		texture2d_desc.MipLevels = 1; // UNIT.99:Depth buffers can't have mipmaps.
		texture2d_desc.ArraySize = 1;
		texture2d_desc.Format = flags & usage::stencil ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_R32_TYPELESS; // DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_R16_TYPELESS
		texture2d_desc.SampleDesc.Count = enable_msaa ? sample_count : 1; 
		texture2d_desc.SampleDesc.Quality = enable_msaa ? msaa_quality_level - 1 : 0; 
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0; // UNIT.99:Depth buffers can't have mipmaps.
		hr = device->CreateTexture2D(&texture2d_desc, 0, depth_stencil_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
		depth_stencil_view_desc.Format = flags & usage::stencil ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT; // DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D16_UNORM
		depth_stencil_view_desc.ViewDimension = enable_msaa ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D; // UNIT.99
        depth_stencil_view_desc.Texture2D.MipSlice = 0;
		depth_stencil_view_desc.Flags = 0;
		hr = device->CreateDepthStencilView(depth_stencil_buffer.Get(), &depth_stencil_view_desc, depth_stencil_view_.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
		shader_resource_view_desc.Format = flags & usage::stencil ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R32_FLOAT; // DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_UNORM
		shader_resource_view_desc.ViewDimension = enable_msaa ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D; // UNIT.99
		shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
		shader_resource_view_desc.Texture2D.MipLevels = 1; // UNIT.99:Depth buffers can't have mipmaps.
		hr = device->CreateShaderResourceView(depth_stencil_buffer.Get(), &shader_resource_view_desc, shader_resource_views_[1].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
	viewport_.TopLeftX = 0.0f;
	viewport_.TopLeftY = 0.0f;

	render_state_ = std::make_unique<RenderState>(device);

}

void FrameBuffer::Clear(
	ID3D11DeviceContext* ctx,
	usage flags,
	DirectX::XMFLOAT4 color,
	float depth,
	uint8_t stencil)
{
	if (HasFlag(flags, usage::color) && render_target_view_)
	{
		ctx->ClearRenderTargetView(render_target_view_.Get(), reinterpret_cast<const FLOAT*>(&color));
	}

	if (HasFlag(flags, usage::depth_stencil) && depth_stencil_view_)
	{
		UINT clear_flags = 0;
		if (HasFlag(flags, usage::depth))   clear_flags |= D3D11_CLEAR_DEPTH;
		if (HasFlag(flags, usage::stencil)) clear_flags |= D3D11_CLEAR_STENCIL;

		// depth_stencil で来たら depth+stencil の意味なので両方
		if (flags == usage::depth_stencil) clear_flags = D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL;

		ctx->ClearDepthStencilView(depth_stencil_view_.Get(), clear_flags, depth, stencil);
	}
}

void FrameBuffer::Activate(ID3D11DeviceContext* immediate_context, usage flags)
{
	// cache current viewport
	viewport_count_ = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	immediate_context->RSGetViewports(&viewport_count_, cached_viewports_);

	// --- Cache OM render targets ---
	ID3D11RenderTargetView* cached_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
	immediate_context->OMGetRenderTargets(
		D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
		cached_rtvs,
		cached_depth_stencil_view_.ReleaseAndGetAddressOf()
	);


	cached_num_rtvs_ = 0;
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		cached_render_target_views_[i].Reset();

		if (cached_rtvs[i])
		{
			cached_render_target_views_[i].Attach(cached_rtvs[i]);
			cached_num_rtvs_ = i + 1; // 連続して刺さっている前提
		}
	}

	// --- Set viewport ---
	immediate_context->RSSetViewports(1, &viewport_);

	// --- Bind targets based on flags ---
	ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
	UINT num_rtvs = 0;

	if (HasFlag(flags, usage::color) && render_target_view_)
	{
		rtvs[0] = render_target_view_.Get();
		num_rtvs = 1;
	}

	ID3D11DepthStencilView* dsv = nullptr;
	if (HasFlag(flags, usage::depth_stencil) && depth_stencil_view_)
		dsv = depth_stencil_view_.Get();

	immediate_context->OMSetRenderTargets(num_rtvs, rtvs, dsv);
	immediate_context->OMSetBlendState(render_state_->GetBlendState(BlendState::transparency), nullptr, 0xFFFFFFFF);
    if (flags & usage::depth)
	{
		immediate_context->OMSetDepthStencilState(render_state_->GetDepthStencilState(DepthState::test_and_write), 0);
	}
	else
	{
		immediate_context->OMSetDepthStencilState(render_state_->GetDepthStencilState(DepthState::no_test_no_write), 0);
	}
}


void FrameBuffer::Deactivate(ID3D11DeviceContext* immediate_context)
{
	immediate_context->RSSetViewports(viewport_count_, cached_viewports_);

	ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
	for (UINT i = 0; i < cached_num_rtvs_; ++i)
		rtvs[i] = cached_render_target_views_[i].Get();

	immediate_context->OMSetRenderTargets(
		cached_num_rtvs_,
		rtvs,
		cached_depth_stencil_view_.Get()
	);

	// キャッシュをクリアして二重保持を避ける
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		cached_render_target_views_[i].Reset();

	cached_depth_stencil_view_.Reset();
	cached_num_rtvs_ = 0;
}
