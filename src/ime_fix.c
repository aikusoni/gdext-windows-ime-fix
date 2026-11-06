// Keeps committed Windows IME text from being dropped in Godot 4.7.2. See README.md.
// Godot drops the last queued key event when a composition starts, meaning the key that started it. The new Microsoft
// Korean IME commits a syllable and starts the next in one go, so that last event is the committed letter. A blank
// character put in front of the start is dropped instead. Records nothing and reads no typed text.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

// The stable part of Godot's gdextension_interface.h, written out so that no header has to be generated.
typedef struct {
	int minimum_initialization_level;
	void *userdata;
	void (*initialize)(void *userdata, int level);
	void (*deinitialize)(void *userdata, int level);
} FixInitialization;
enum { LEVEL_SCENE = 2 };

static HWND window = NULL;
static WNDPROC godot_proc = NULL;
// The last key message handed to Godot was a character, so it lies at the end of Godot's key queue.
static BOOL char_last = FALSE;

static LRESULT CALLBACK fix_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	switch (message) {
		case WM_IME_STARTCOMPOSITION:
			if (char_last) CallWindowProcW(godot_proc, hwnd, WM_CHAR, 0, 0);
			char_last = FALSE;
			break;
		case WM_CHAR:
			char_last = TRUE;
			break;
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			char_last = FALSE;
			break;
		default:
			break;
	}
	return CallWindowProcW(godot_proc, hwnd, message, wparam, lparam);
}

static BOOL CALLBACK find_window(HWND hwnd, LPARAM result) {
	WCHAR name[32];
	if (GetClassNameW(hwnd, name, 32) > 0 && lstrcmpW(name, L"Engine") == 0) {
		*(HWND *)result = hwnd;
		return FALSE;
	}
	return TRUE;
}

static void hook(void *userdata, int level) {
	(void)userdata;
	if (level != LEVEL_SCENE || window != NULL) return;
	HWND found = NULL;
	EnumThreadWindows(GetCurrentThreadId(), find_window, (LPARAM)&found);
	if (found == NULL) return;
	window = found;
	godot_proc = (WNDPROC)SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)fix_proc);
}

static void unhook(void *userdata, int level) {
	(void)userdata;
	if (level != LEVEL_SCENE) return;
	// Only ours to take back while nothing else stands in front of it.
	if (window != NULL && IsWindow(window) && (WNDPROC)GetWindowLongPtrW(window, GWLP_WNDPROC) == fix_proc) {
		SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)godot_proc);
	}
	window = NULL;
}

__declspec(dllexport) uint8_t ime_fix_init(void *get_proc_address, void *library, FixInitialization *result) {
	(void)get_proc_address;
	(void)library;
	result->minimum_initialization_level = LEVEL_SCENE;
	result->userdata = NULL;
	result->initialize = hook;
	result->deinitialize = unhook;
	return 1;
}
