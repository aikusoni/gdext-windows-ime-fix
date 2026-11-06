#ifndef IME_GUARD_H
#define IME_GUARD_H

#include <imm.h>

// Godot 4.7.2 detaches the window's input context when it disables IME.
static void protect_committed_char(HWND hwnd, WNDPROC next_proc) {
	HIMC context = ImmGetContext(hwnd);
	if (context != NULL) {
		ImmReleaseContext(hwnd, context);
		CallWindowProcW(next_proc, hwnd, WM_CHAR, 0, 0);
	}
}

#endif
