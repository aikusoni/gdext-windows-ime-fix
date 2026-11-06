// A probe, not a feature. It stands in front of the Godot window's message handler and writes down every input
// method message it sees, so a run says whether the Windows input method delivers composition through WM_IME_*.
// It uses no Godot call beyond the entry point, so it needs no generated bindings.
//
// IME_PROBE_MODE:     "observe" (default) passes every message on to Godot; "take" keeps composition messages from it;
//                         "fix" passes everything and keeps a committed letter from being lost (see probe_proc).
// IME_PROBE_LOG:      where to append; "ime_probe_native.log" in the working folder by default.
// IME_PROBE_SELFTEST: "1" posts one WM_CHAR 'A' once hooked, to show the hook passes input on. "2" plays, a second
//                         later, what the new Microsoft IME sends between two syllables: a committed letter, then the
//                         next composition's start, all before Godot reads its key queue. "3" plays the same with
//                         a pause between the letter and the start, so Godot reads its queue in between.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ime_guard.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// The stable part of Godot's gdextension_interface.h, written out so that no header has to be generated.
typedef struct {
	int minimum_initialization_level;
	void *userdata;
	void (*initialize)(void *userdata, int level);
	void (*deinitialize)(void *userdata, int level);
} ProbeInitialization;
enum { LEVEL_SCENE = 2 };

static HWND window = NULL;
static WNDPROC godot_proc = NULL;
static FILE *out = NULL;
static BOOL taking = FALSE;
static BOOL fixing = FALSE;
// The last key message handed to Godot was a character, so it lies at the end of Godot's key queue.
static BOOL char_last = FALSE;
static BOOL selftest_paused = FALSE;
#define SELFTEST_TIMER 0x1109
#define SELFTEST_LATER_TIMER 0x110A

static void stamp(void) {
	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	const uint64_t ticks = ((uint64_t)now.dwHighDateTime << 32) | now.dwLowDateTime;
	fprintf(out, "%llu.%03llu ", (ticks / 10000000ULL) - 11644473600ULL, (ticks / 10000ULL) % 1000ULL);
}

// A UTF-16 run as UTF-8 and as code units, since a composition's exact units are what is being asked about.
static void write_wide(const char *label, const WCHAR *text, int units) {
	char utf8[512];
	const int size = WideCharToMultiByte(CP_UTF8, 0, text, units, utf8, (int)sizeof(utf8) - 1, NULL, NULL);
	utf8[size > 0 ? size : 0] = 0;
	fprintf(out, " %s=[%s] units=%d", label, utf8, units);
	for (int index = 0; index < units && index < 16; index++) fprintf(out, " %04X", (unsigned)text[index]);
}

static void write_composition(HIMC context, DWORD which, const char *label) {
	WCHAR text[128];
	const LONG bytes = ImmGetCompositionStringW(context, which, text, (DWORD)sizeof(text));
	if (bytes < 0) fprintf(out, " %s=error(%ld)", label, bytes);
	else write_wide(label, text, bytes / (int)sizeof(WCHAR));
}

static void log_composition(LPARAM flags) {
	stamp();
	fprintf(out, "WM_IME_COMPOSITION flags=%08llX", (unsigned long long)flags);
	HIMC context = ImmGetContext(window);
	if (context == NULL) fprintf(out, " context=null");
	else {
		write_composition(context, GCS_COMPSTR, "comp");
		write_composition(context, GCS_RESULTSTR, "result");
		fprintf(out, " cursor=%ld", ImmGetCompositionStringW(context, GCS_CURSORPOS, NULL, 0));
		ImmReleaseContext(window, context);
	}
	fprintf(out, "\n");
	fflush(out);
}

static void log_plain(const char *name, WPARAM wparam, LPARAM lparam) {
	stamp();
	fprintf(out, "%s w=%llX l=%llX\n", name, (unsigned long long)wparam, (unsigned long long)lparam);
	fflush(out);
}

static void log_state(const char *why) {
	HIMC context = ImmGetContext(window);
	DWORD conversion = 0, sentence = 0;
	WCHAR description[128] = {0};
	WCHAR layout[KL_NAMELENGTH] = {0};
	const HKL keyboard = GetKeyboardLayout(0);
	const UINT described = ImmGetDescriptionW(keyboard, description, 127);
	GetKeyboardLayoutNameW(layout);
	stamp();
	fprintf(out, "STATE why=%s hkl=%p context=%p open=%d", why, (void *)keyboard, (void *)context, context ? (int)ImmGetOpenStatus(context) : -1);
	if (context) {
		ImmGetConversionStatus(context, &conversion, &sentence);
		fprintf(out, " conversion=%lX sentence=%lX", conversion, sentence);
	}
	write_wide("layout", layout, lstrlenW(layout));
	write_wide("description", description, (int)described);
	fprintf(out, " is_ime=%d\n", (int)ImmIsIME(keyboard));
	fflush(out);
	if (context) ImmReleaseContext(window, context);
}

static LRESULT CALLBACK probe_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	switch (message) {
		case WM_TIMER:
			if (wparam == SELFTEST_LATER_TIMER) {
				KillTimer(hwnd, SELFTEST_LATER_TIMER);
				SendMessageW(hwnd, WM_IME_STARTCOMPOSITION, 0, 0);
				SendMessageW(hwnd, WM_IME_ENDCOMPOSITION, 0, 0);
				SendMessageW(hwnd, WM_CHAR, 0xB098, 1);
				return 0;
			}
			if (wparam == SELFTEST_TIMER) {
				KillTimer(hwnd, SELFTEST_TIMER);
				stamp();
				fprintf(out, "SELFTEST two syllables\n");
				fflush(out);
				SendMessageW(hwnd, WM_IME_STARTCOMPOSITION, 0, 0);
				SendMessageW(hwnd, WM_IME_ENDCOMPOSITION, 0, 0);
				SendMessageW(hwnd, WM_CHAR, 0xAC00, 1);
				if (selftest_paused) {
					SetTimer(hwnd, SELFTEST_LATER_TIMER, 400, NULL);
					return 0;
				}
				SendMessageW(hwnd, WM_IME_STARTCOMPOSITION, 0, 0);
				SendMessageW(hwnd, WM_IME_ENDCOMPOSITION, 0, 0);
				SendMessageW(hwnd, WM_CHAR, 0xB098, 1);
				return 0;
			}
			break;
		case WM_IME_SETCONTEXT: log_plain("WM_IME_SETCONTEXT", wparam, lparam); break;
		case WM_IME_STARTCOMPOSITION:
			log_plain("WM_IME_STARTCOMPOSITION", wparam, lparam);
			log_state("composition");
			if (taking) return 0;
			// Godot 4.7.2 drops the last queued key event when a composition starts, meaning the key that started it. The new
			// IME commits a syllable and starts the next in one go, so that last event is the committed letter. A blank
			// character in front of it is dropped instead while the window's IME is active.
			if (fixing && char_last) protect_committed_char(hwnd, godot_proc);
			char_last = FALSE;
			break;
		case WM_IME_COMPOSITION:
			log_composition(lparam);
			if (taking) return 0;
			break;
		case WM_IME_ENDCOMPOSITION:
			log_plain("WM_IME_ENDCOMPOSITION", wparam, lparam);
			if (taking) return 0;
			break;
		case WM_IME_NOTIFY: log_plain("WM_IME_NOTIFY", wparam, lparam); break;
		case WM_IME_REQUEST: log_plain("WM_IME_REQUEST", wparam, lparam); break;
		case WM_IME_CHAR: log_plain("WM_IME_CHAR", wparam, lparam); break;
		case WM_CHAR:
			log_plain("WM_CHAR", wparam, lparam);
			char_last = TRUE;
			break;
		case WM_UNICHAR: log_plain("WM_UNICHAR", wparam, lparam); break;
		case WM_KEYDOWN:
			log_plain("WM_KEYDOWN", wparam, lparam);
			// The input method claimed the key: the state then, whether or not any composition message follows.
			if (wparam == VK_PROCESSKEY) log_state("processkey");
			char_last = FALSE;
			break;
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			char_last = FALSE;
			break;
		case WM_INPUTLANGCHANGE:
			log_plain("WM_INPUTLANGCHANGE", wparam, lparam);
			log_state("language");
			break;
		case WM_SETFOCUS:
			log_plain("WM_SETFOCUS", wparam, lparam);
			log_state("focus");
			break;
		case WM_KILLFOCUS: log_plain("WM_KILLFOCUS", wparam, lparam); break;
		default: break;
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
	char value[512];
	const DWORD path = GetEnvironmentVariableA("IME_PROBE_LOG", value, sizeof(value));
	out = _fsopen(path > 0 && path < sizeof(value) ? value : "ime_probe_native.log", "ab", 0x40);
	if (out == NULL) return;
	const DWORD mode = GetEnvironmentVariableA("IME_PROBE_MODE", value, sizeof(value));
	taking = mode > 0 && mode < sizeof(value) && lstrcmpA(value, "take") == 0;
	fixing = mode > 0 && mode < sizeof(value) && lstrcmpA(value, "fix") == 0;
	stamp();
	fprintf(out, "START mode=%s thread=%lu\n", taking ? "take" : fixing ? "fix" : "observe", GetCurrentThreadId());
	HWND found = NULL;
	EnumThreadWindows(GetCurrentThreadId(), find_window, (LPARAM)&found);
	if (found == NULL) {
		// The window is made on the main thread. With none yet (loaded too early), say so: this probe does not wait.
		stamp();
		fprintf(out, "NO_WINDOW\n");
		fflush(out);
		return;
	}
	window = found;
	godot_proc = (WNDPROC)SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)probe_proc);
	stamp();
	fprintf(out, "HOOKED window=%p\n", (void *)window);
	log_state("hook");
	const DWORD selftest = GetEnvironmentVariableA("IME_PROBE_SELFTEST", value, sizeof(value));
	if (selftest == 1 && value[0] == '1') PostMessageW(window, WM_CHAR, 'A', 1);
	if (selftest == 1 && (value[0] == '2' || value[0] == '3')) {
		selftest_paused = value[0] == '3';
		SetTimer(window, SELFTEST_TIMER, 1000, NULL);
	}
}

static void unhook(void *userdata, int level) {
	(void)userdata;
	if (level != LEVEL_SCENE) return;
	// Only ours to take back while nothing else stands in front of it.
	if (window != NULL && IsWindow(window) && (WNDPROC)GetWindowLongPtrW(window, GWLP_WNDPROC) == probe_proc) {
		SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)godot_proc);
	}
	window = NULL;
	if (out != NULL) {
		stamp();
		fprintf(out, "END\n");
		fclose(out);
		out = NULL;
	}
}

__declspec(dllexport) uint8_t ime_probe_init(void *get_proc_address, void *library, ProbeInitialization *result) {
	(void)get_proc_address;
	(void)library;
	result->minimum_initialization_level = LEVEL_SCENE;
	result->userdata = NULL;
	result->initialize = hook;
	result->deinitialize = unhook;
	return 1;
}
