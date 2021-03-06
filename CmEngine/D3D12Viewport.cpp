#include "D3D12Viewport.h"
#include "D3D12RhiGlobal.h"
#include "D3D12RhiUtil.h"
#include "d3dx12.h"
#include "DescriptorHandleManager.h"
#include "Util.h"
#include "Global.h"

void FD3D12Viewport::__Init()
{
	DXGI_SWAP_CHAIN_DESC desc;
	desc.BufferCount = mBackBufferCount;
	desc.BufferDesc.Format = mPixelFormat;
	desc.BufferDesc.Height = mSizeY;
	desc.BufferDesc.RefreshRate.Denominator = 60;
	desc.BufferDesc.RefreshRate.Numerator = 1;
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST;
	desc.BufferDesc.Width = mSizeX;
	desc.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	desc.OutputWindow = mHwnd;
	desc.SampleDesc.Quality = 0;
	desc.SampleDesc.Count = 1;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.Windowed = !bIsFullScreen;

	VerifyD3D12Result(GFactory->CreateSwapChain(GCommandQueue, &desc, &mSwapChain));
	for (auto i = 0; i != mBackBufferCount; ++i)
	{
		VerifyD3D12Result(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBackBuffers[i])));
	}

	for (auto i = 0; i != mBackBufferCount; ++i)
	{
		D3D12_RENDER_TARGET_VIEW_DESC desc;
		desc.Format = mPixelFormat;
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;
		desc.Texture2D.PlaneSlice = 0;
		mBackBufferViews[i] = FDescriptorHandleManager<D3D12_RENDER_TARGET_VIEW_DESC>::Get().CreateView(mBackBuffers[i], desc);
	}
}

FD3D12Viewport::FD3D12Viewport(HWND _windowHandle, uint32_t _sizeX, uint32_t _sizeY, bool _isFullscreen, DXGI_FORMAT _pixelFormat)
	:mHwnd(_windowHandle),
	mSizeX(_sizeX),
	mSizeY(_sizeY),
	mPixelFormat(_pixelFormat),
	bIsFullScreen(_isFullscreen)
{
	mBackBufferCount = DefaultBackBufferCount;
	bShouldPresent = true;

	__Init();
}

void FD3D12Viewport::Resize(uint32_t _sizeX, uint32_t _sizeY, bool _isFullscreen, DXGI_FORMAT _pixelFormat)
{
	constexpr uint8_t NeedResizeBuffer = 1 << 0;
	constexpr uint8_t NeedResizeTarget = 1 << 1;
	constexpr uint8_t NeedSetFullScreenState = 1 << 2;

	uint8_t ChangedFlag = 0;
	if (_sizeX != mSizeX || _sizeY != mSizeY || _pixelFormat != mPixelFormat)
		ChangedFlag |= NeedResizeBuffer;
	if (_isFullscreen != bIsFullScreen)
		ChangedFlag |= NeedSetFullScreenState;

	if (ChangedFlag & (NeedResizeBuffer | NeedResizeTarget))
	{
		WaitForCompletion();
		for (auto i = 0; i != mBackBufferCount; ++i)
		{
			mBackBuffers[i]->Release();
			mBackBuffers[i] = nullptr;
			FDescriptorHandleManager<D3D12_RENDER_TARGET_VIEW_DESC>::Get().FreeSlot(mBackBufferViews[i]);
		}
	}

	mSizeX = _sizeX;
	mSizeY = _sizeY;
	bIsFullScreen = _isFullscreen;
	mPixelFormat = _pixelFormat;

	if (ChangedFlag & NeedResizeBuffer)
	{
		mSwapChain->ResizeBuffers(mBackBufferCount, mSizeX, mSizeY, mPixelFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	}

	if (ChangedFlag & NeedSetFullScreenState)
	{
		mSwapChain->SetFullscreenState(bIsFullScreen, nullptr);
	}

	if (ChangedFlag & (NeedResizeBuffer | NeedResizeTarget))
	{
		for (auto i = 0; i != mBackBufferCount; ++i)
		{
			VerifyD3D12Result(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBackBuffers[i])));
		}
		mLastUseFenceValue = 0;
		mCurrentBackBufferIndex = 0;

		for (auto i = 0; i != mBackBufferCount; ++i)
		{
			D3D12_RENDER_TARGET_VIEW_DESC desc;
			desc.Format = mPixelFormat;
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = 0;
			desc.Texture2D.PlaneSlice = 0;
			mBackBufferViews[i] = FDescriptorHandleManager<D3D12_RENDER_TARGET_VIEW_DESC>::Get().CreateView(mBackBuffers[i], desc);
		}
	}
}

ID3D12Resource * FD3D12Viewport::GetCurrentBackBuffer() const
{
	Assert(IsInRenderThread());
	return mBackBuffers[mCurrentBackBufferIndex];
}

FDescriptorHandle FD3D12Viewport::GetCurrentBackBufferView() const
{
	Assert(IsInRenderThread());
	return mBackBufferViews[mCurrentBackBufferIndex];
}

uint32_t FD3D12Viewport::Present()
{
	if (bShouldPresent)
	{
		mSwapChain->Present(0, 0);
		mCurrentBackBufferIndex = (mCurrentBackBufferIndex + 1) % mBackBufferCount;
	}

	return bShouldPresent;
}

FD3D12Viewport::~FD3D12Viewport()
{
	WaitForCompletion();
	for (auto i = 0; i != mBackBufferCount; ++i)
	{
		FDescriptorHandleManager<D3D12_RENDER_TARGET_VIEW_DESC>::Get().FreeSlot(mBackBufferViews[i]);
		mBackBuffers[i]->Release();
	}
	mSwapChain->Release();
}
