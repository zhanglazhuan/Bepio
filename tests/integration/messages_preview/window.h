#ifndef BEPIO_MESSAGES_PREVIEW_WINDOW_H_
#define BEPIO_MESSAGES_PREVIEW_WINDOW_H_
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

static uint16_t *window_frame;
static uint32_t window_pixels[PREVIEW_WIDTH * PREVIEW_HEIGHT];
static lv_obj_t *window_root;
static lv_point_t mouse_position;
static bool mouse_down;
static void pointer_read(lv_indev_t *indev, lv_indev_data_t *data)
{
	(void)indev;
	data->point = mouse_position;
	data->state = mouse_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static LRESULT CALLBACK preview_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message) {
	case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
		mouse_position.x = GET_X_LPARAM(lparam) / 2;
		mouse_position.y = GET_Y_LPARAM(lparam) / 2;
		if (message == WM_LBUTTONDOWN) { mouse_down = true; SetCapture(window); }
		if (message == WM_LBUTTONUP) { mouse_down = false; ReleaseCapture(); }
		return 0;
	case WM_MOUSEWHEEL:
		lv_obj_scroll_by(window_root, 0, GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 60 : -60, LV_ANIM_OFF);
		return 0;
	case WM_TIMER:
		lv_tick_inc(16);
		lv_timer_handler();
		InvalidateRect(window, NULL, FALSE);
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC dc = BeginPaint(window, &paint);
		for (int i = 0; i < PREVIEW_WIDTH * PREVIEW_HEIGHT; i++) {
			uint16_t p = window_frame[i];
			window_pixels[i] = ((((p >> 11) & 31) * 255 / 31) << 16) |
				((((p >> 5) & 63) * 255 / 63) << 8) | ((p & 31) * 255 / 31);
		}
		BITMAPINFO bitmap = {0};
		bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmap.bmiHeader.biWidth = PREVIEW_WIDTH;
		bitmap.bmiHeader.biHeight = -PREVIEW_HEIGHT;
		bitmap.bmiHeader.biPlanes = 1;
		bitmap.bmiHeader.biBitCount = 32;
		bitmap.bmiHeader.biCompression = BI_RGB;
		StretchDIBits(dc, 0, 0, PREVIEW_WIDTH * 2, PREVIEW_HEIGHT * 2, 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, window_pixels, &bitmap, DIB_RGB_COLORS, SRCCOPY);
		EndPaint(window, &paint);
		return 0;
	}
	case WM_DESTROY: KillTimer(window, 1); PostQuitMessage(0); return 0;
	}
	return DefWindowProc(window, message, wparam, lparam);
}
static void bepio_preview_window(uint16_t *pixels, lv_obj_t *root_obj)
{
	window_frame = pixels;
	window_root = root_obj;
	HINSTANCE instance = GetModuleHandle(NULL);
	WNDCLASS wc = {0};
	wc.lpfnWndProc = preview_proc;
	wc.hInstance = instance;
	wc.lpszClassName = "BepioMessagesPreview";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	assert(RegisterClass(&wc));
	RECT area = {0, 0, PREVIEW_WIDTH * 2, PREVIEW_HEIGHT * 2};
	DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	AdjustWindowRect(&area, style, FALSE);
	HWND window = CreateWindow(wc.lpszClassName, "Bepio Messages / MOCK / mouse wheel to scroll", style,
		CW_USEDEFAULT, CW_USEDEFAULT, area.right - area.left, area.bottom - area.top,
		NULL, NULL, instance, NULL);
	assert(window);
	lv_indev_t *pointer = lv_indev_create();
	lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(pointer, pointer_read);
	bepio_navigation_attach_input(pointer);
	ShowWindow(window, SW_SHOW);
	SetTimer(window, 1, 16, NULL);
	MSG message;
	while (GetMessage(&message, NULL, 0, 0) > 0) {
		TranslateMessage(&message); DispatchMessage(&message);
	}
	lv_indev_delete(pointer);
}
#endif
