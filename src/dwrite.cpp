#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwrite.h>
#include <intrin.h>

#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "dwrite.lib")

static IDWriteBitmapRenderTarget* target;
static IDWriteRenderingParams* params;
static IDWriteFactory* factory;
static IDWriteGdiInterop* gdi;
static COLORREF color;

struct MyRender : IDWriteTextRenderer {
	STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) {
		return E_NOTIMPL;
	}

	STDMETHOD_(ULONG, AddRef)() {
		return 0;
	}

	STDMETHOD_(ULONG, Release)() {
		return 0;
	}

	STDMETHOD(IsPixelSnappingDisabled)(void* ctx, BOOL* isDisabled) {
		*isDisabled = FALSE;
		return S_OK;
	}

	STDMETHOD(GetCurrentTransform)(void* ctx, DWRITE_MATRIX* transform) {
		DWRITE_MATRIX identity = {
			1, 0,
			0, 1,
			0, 0
		};
		*transform = identity;
		return S_OK;
	}

	STDMETHOD(GetPixelsPerDip) (void* ctx, FLOAT* pixelsPerDip) {
		// TODO: get from current monitor or smth?
		*pixelsPerDip = 96.0f;
		return S_OK;
	}

	STDMETHOD(DrawGlyphRun) (void* ctx, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode, const DWRITE_GLYPH_RUN* glyphRun, const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription, IUnknown* clientDrawingEffect) {
		return target->DrawGlyphRun(baselineOriginX, baselineOriginY, measuringMode, glyphRun, params, color);
	}

	STDMETHOD(DrawUnderline) (void* ctx, FLOAT baselineOriginX, FLOAT baselineOriginY, const DWRITE_UNDERLINE* underline, IUnknown* clientDrawingEffect) {
		return E_NOTIMPL;
	}

	STDMETHOD(DrawStrikethrough) (void* ctx, FLOAT baselineOriginX, FLOAT baselineOriginY, const DWRITE_STRIKETHROUGH* strikethrough, IUnknown* clientDrawingEffect) {
		return E_NOTIMPL;
	}

	STDMETHOD(DrawInlineObject) (void* ctx, FLOAT originX, FLOAT originY, IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown* clientDrawingEffect) {
		return E_NOTIMPL;
	}
};

extern "C" void exitHRESULT(const char* function, HRESULT err);

extern "C" int abc() {
	HRESULT r;

	IDWriteTextFormat* format;

	const DWORD width = 256;
	const DWORD height = 256;

	if (FAILED(r = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&factory)))
		exitHRESULT("DWriteCreateFactory", r);
	if (FAILED(r = factory->CreateTextFormat(L"Comic Sans MS", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.f, L"en-us", &format)))
		exitHRESULT("IDWriteFactory::CreateTextFormat", r);
	if (FAILED(r = factory->CreateRenderingParams(&params)))
		exitHRESULT("IDWriteFactory::CreateRenderingParams", r);
	if (FAILED(r = factory->GetGdiInterop(&gdi)))
		exitHRESULT("IDWriteFactory::GetGdiInterop", r);

	if (FAILED(r = gdi->CreateBitmapRenderTarget(NULL, width, height, &target)))
		exitHRESULT("IDWriteGdiInterop::CreateBitmapRenderTarget", r);

	MyRender renderer;
	color = RGB(0, 255, 0);

	const WCHAR text[] = L"Hello 😂 World!";

	IDWriteTextLayout* layout;
	if (FAILED(r = factory->CreateTextLayout(text, _countof(text) - 1, format, width, 0, &layout)))
		exitHRESULT("IDWriteFactory::CreateTextLayout", r);
	if (FAILED(r = layout->Draw(NULL, &renderer, 0.f, 0.f)))
		exitHRESULT("IDWriteTextLayout::Draw", r);
	layout->Release();

	// the memory bitmap is always 32-bit top-down
	HDC dc = target->GetMemoryDC();
	HBITMAP bitmap = (HBITMAP)GetCurrentObject(dc, OBJ_BITMAP);

	DIBSECTION dib;
	GetObjectW(bitmap, sizeof(dib), &dib);

	HANDLE f = CreateFileA("test.bmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	BITMAPINFOHEADER header = {};
	header.biSize = sizeof(header);
	header.biWidth = width;
	header.biHeight = -height; // vertical flip
	header.biPlanes = 1;
	header.biBitCount = 32;
	header.biCompression = BI_RGB;

	BITMAPFILEHEADER bmp;
	bmp.bfType = 'B' + ('M' << 8);
	bmp.bfSize = sizeof(bmp) + sizeof(header) + height * dib.dsBm.bmWidthBytes;
	bmp.bfOffBits = sizeof(bmp) + sizeof(header);

	DWORD written;
	WriteFile(f, &bmp, sizeof(bmp), &written, NULL);
	WriteFile(f, &header, sizeof(header), &written, NULL);
	WriteFile(f, dib.dsBm.bmBits, height * dib.dsBm.bmWidthBytes, &written, NULL);

	CloseHandle(f);

	format->Release();

	return 0;
}