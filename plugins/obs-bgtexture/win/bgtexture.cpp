#include <stdio.h>
#include <time.h>
#include <windows.h>

#include <util/base.h>
#include <graphics/vec2.h>
#include <obs.h>

#include <intrin.h>

static const int cx = 800;
static const int cy = 600;

/* --------------------------------------------------- */

class SourceContext {
	obs_source_t *source;

public:
	inline SourceContext(obs_source_t *source) : source(source) {}
	inline ~SourceContext() {obs_source_release(source);}
	inline operator obs_source_t*() {return source;}
};

/* --------------------------------------------------- */

static LRESULT CALLBACK sceneProc(HWND hwnd, UINT message, WPARAM wParam,
		LPARAM lParam)
{
	switch (message) {

	case WM_CLOSE:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

static void do_log(int log_level, const char *msg, va_list args, void *param)
{
	char bla[4096];
	vsnprintf(bla, 4095, msg, args);

	OutputDebugStringA(bla);
	OutputDebugStringA("\n");

	if (log_level < LOG_WARNING)
		__debugbreak();

	UNUSED_PARAMETER(param);
}

static void CreateOBS(HWND hwnd)
{
	RECT rc;
	GetClientRect(hwnd, &rc);

	if (!obs_startup("en-US", nullptr, nullptr))
		throw "Couldn't create Background Texture module";

	struct obs_video_info ovi;
	ovi.adapter         = 0;
	ovi.base_width      = rc.right;
	ovi.base_height     = rc.bottom;
	ovi.fps_num         = 60000;
	ovi.fps_den         = 1000;
	ovi.graphics_module = DL_OPENGL;
	ovi.output_format   = VIDEO_FORMAT_RGBA;
	ovi.output_width    = rc.right;
	ovi.output_height   = rc.bottom;

	if (obs_reset_video(&ovi) != 0)
		throw "Couldn't initialize Background Texture";
}

static HWND CreateBGColorWindow(HINSTANCE instance)
{
	WNDCLASS wc;

	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = TEXT("bla");
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.hInstance     = instance;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.lpfnWndProc   = (WNDPROC)sceneProc;

	if (!RegisterClass(&wc))
		return 0;

	return CreateWindow(TEXT("bla"), TEXT("bla"),
			WS_OVERLAPPEDWINDOW|WS_VISIBLE,
			1920/2 - cx/2, 1080/2 - cy/2, cx, cy,
			NULL, NULL, instance, NULL);
}

/* --------------------------------------------------- */

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine,
		int numCmd)
{
	HWND hwnd = NULL;
	base_set_log_handler(do_log, nullptr);

	try {
		hwnd = CreateBGColorWindow(instance);
		if (!hwnd)
			throw "Couldn't create Background Texture main window";

		/* create OBS */
		CreateOBS(hwnd);

		/* load modules */
		obs_load_all_modules();

		/* create source */
		SourceContext source = obs_source_create("background_texture",
				"Background Texture source", NULL, nullptr);
		if (!source)
			throw "Couldn't create Background Texture source";

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

	} catch (char *error) {
		MessageBoxA(NULL, error, NULL, 0);
	}

	obs_shutdown();

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());
	DestroyWindow(hwnd);

	UNUSED_PARAMETER(prevInstance);
	UNUSED_PARAMETER(cmdLine);
	UNUSED_PARAMETER(numCmd);
	return 0;
}
