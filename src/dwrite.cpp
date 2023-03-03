#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwrite.h>
#include <d2d1.h>

static IDWriteFactory* dwriteFactory;
static IDWriteTextFormat* format;

static ID2D1Factory* d2dFactory;
static ID2D1DCRenderTarget* renderTarget;
// static ID2D1StrokeStyle* strokeStyle;

extern "C" void drawText(const WCHAR* text, UINT32 length, FLOAT maxWidth, FLOAT maxHeight) {
	IDWriteTextLayout* textLayout;
	dwriteFactory->CreateTextLayout(text, length, format, maxWidth, maxHeight, &textLayout);

	// ID2D1PathGeometry* pathGeometry;
	// renderTarget->CreatePathGeometry(&pathGeometry);

	// ID2D1GeometrySink* sink;
	// pathGeometry->Open(&sink);
	// textLayout->Draw(NULL, sink, 0, 0);
	// sink->Close();
	// sink->Release();

	// renderTarget->DrawGeometry(pathGeometry, brush, 3.f, strokeStyle);

	// pathGeometry->Release();
	textLayout->Release();
}

extern "C" void abc(void) {
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwriteFactory);

	dwriteFactory->CreateTextFormat(L"Comic Sans MS", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.f, L"en-US", &format);

	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);

	D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = { };
	d2dFactory->CreateDCRenderTarget(&renderTargetProperties, &renderTarget);

	// D2D1_STROKE_STYLE_PROPERTIES strokeSyleProperties = {
	// 	.startCap   = D2D1_CAP_STYLE_ROUND,
	// 	.endCap     = D2D1_CAP_STYLE_ROUND,
	// 	.dashCap    = D2D1_CAP_STYLE_ROUND,
	// 	.lineJoin   = D2D1_LINE_JOIN_ROUND,
	// 	.miterLimit = 3.f,
	// 	.dashStyle   = D2D1_DASH_STYLE_SOLID,
	// 	.dashOffset = 0.f
	// };
	// renderTarget->CreateStrokeStyle(&strokeSyleProperties, NULL, 0, &strokeStyle);
}