#include"../headers/framebuffer.h"

#include"../headers/misc.h"

inline bool operator& (FrameBuffer::usage lhs, FrameBuffer::usage rhs)
{
    return static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs);
}
FrameBuffer::FrameBuffer(ID3D11Device* device, uint32_t width, uint32_t height, usage flags , bool enable_msaa, int subsamples, bool generate_mips)
{
	HRESULT hr{ S_OK };

	DXGI_FORMAT rt_format = DXGI_FORMAT_R16G16B16A16_FLOAT; // DXGI_FORMAT_R16G16B16A16_FLOAT DXGI_FORMAT_R32G32B32A32_FLOAT
	UINT msaa_quality_level;
	UINT sample_count = subsamples;
	hr = device->CheckMultisampleQualityLevels(rt_format, sample_count, &msaa_quality_level);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	if (flags & usage::color)
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> render_target_buffer;
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
		hr = device->CreateTexture2D(&texture2d_desc, 0, render_target_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc{};
		render_target_view_desc.Format = texture2d_desc.Format;
		render_target_view_desc.ViewDimension = enable_msaa ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D; 
		hr = device->CreateRenderTargetView(render_target_buffer.Get(), &render_target_view_desc, render_target_view_.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
		shader_resource_view_desc.Format = texture2d_desc.Format;
		shader_resource_view_desc.ViewDimension = enable_msaa ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D; 
		shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
		shader_resource_view_desc.Texture2D.MipLevels = generate_mips ? -1 : 1; 
		hr = device->CreateShaderResourceView(render_target_buffer.Get(), &shader_resource_view_desc, shader_resource_views_[0].GetAddressOf());
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

}

void FrameBuffer::Clear(
	ID3D11DeviceContext* immediate_context,
	usage flags, 
	DirectX::XMFLOAT4 color,
	float depth, 
	uint8_t stencil)
{
	if (flags & usage::color && render_target_view_) // UNIT.99
	{
		immediate_context->ClearRenderTargetView(render_target_view_.Get(), reinterpret_cast<const FLOAT*>(&color));
	}
	if (flags & usage::depth_stencil && depth_stencil_view_) // UNIT.99
	{
		immediate_context->ClearDepthStencilView(depth_stencil_view_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
	}
}
void FrameBuffer::Activate(ID3D11DeviceContext* immediate_context, usage flags)
{
	viewport_count_ = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	immediate_context->RSGetViewports(&viewport_count_, cached_viewports_);
	immediate_context->OMGetRenderTargets(1, cached_render_target_view_.ReleaseAndGetAddressOf(), cached_depth_stencil_view_.ReleaseAndGetAddressOf());

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> null_render_target_view;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> null_depth_stencil_view;
	immediate_context->RSSetViewports(1, &viewport_);
	immediate_context->OMSetRenderTargets(1, 
		flags & usage::color ? render_target_view_.GetAddressOf() : null_render_target_view.GetAddressOf(),
		flags & usage::depth_stencil ? depth_stencil_view_.Get() : null_depth_stencil_view.Get());
}
void FrameBuffer::Deactivate(ID3D11DeviceContext* immediate_context)
{
	immediate_context->RSSetViewports(viewport_count_, cached_viewports_);
	immediate_context->OMSetRenderTargets(1, cached_render_target_view_.GetAddressOf(), cached_depth_stencil_view_.Get());
}
