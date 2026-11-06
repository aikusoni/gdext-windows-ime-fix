"""Run the Windows IME regression against the game DLL built by build.bat."""

import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
WM_IME_STARTCOMPOSITION = 0x010D
WM_IME_ENDCOMPOSITION = 0x010E
WM_CHAR = 0x0102
WM_KEYDOWN = 0x0100
VK_F12 = 0x7B


def read_result(path, process):
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        if path.is_file():
            value = path.read_text(encoding="utf-8")
            if value:
                return value.rstrip("\r\n")
        if process.poll() is not None:
            break
        time.sleep(0.02)
    raise RuntimeError(f"Godot did not produce {path.name}")


def check_chat(project, process):
    hwnd = int(read_result(project / "ready.txt", process))
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    user32.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
    user32.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)
    owner = wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
    if owner.value != process.pid:
        raise RuntimeError("Expected the test project's own window")

    def post(message, value=0, flags=0):
        if not user32.PostMessageW(hwnd, message, value, flags):
            raise ctypes.WinError(ctypes.get_last_error())

    phrase = "가나다 한글"
    for char in phrase:
        if char == " ":
            # Let the first word and then the space settle before the next word.
            time.sleep(0.2)
            post(WM_CHAR, ord(char), 1)
            time.sleep(0.2)
        else:
            post(WM_IME_STARTCOMPOSITION)
            post(WM_IME_ENDCOMPOSITION)
            post(WM_CHAR, ord(char), 1)
    time.sleep(0.2)
    (project / "close_chat").touch()
    text = read_result(project / "text.txt", process)
    post(WM_IME_STARTCOMPOSITION)
    post(WM_IME_ENDCOMPOSITION)
    post(WM_KEYDOWN, VK_F12, (0x58 << 16) | 1)  # End marker, after any spurious key.
    presses = read_result(project / "presses.txt", process)
    if text != phrase or presses != "0":
        raise RuntimeError(f"FAIL: text={text!r}, presses_after_close={presses}")
    print(f"PASS: text={text!r}, presses_after_close={presses}")


def main():
    exe = os.environ.get("GODOT_EXE")
    if not exe or not Path(exe).is_file():
        raise SystemExit("Set GODOT_EXE to your Godot 4.7.2 executable.")
    if not (BUILD / "windows_ime_fix.dll").is_file():
        raise SystemExit("Run build.bat first.")
    with tempfile.TemporaryDirectory(prefix="test-chat-", dir=BUILD) as directory:
        project = Path(directory)
        for name in ("windows_ime_fix.dll", "windows_ime_fix.gdextension"):
            shutil.copyfile(BUILD / name, project / name)
        for name in ("project.godot", "main.tscn"):
            shutil.copyfile(ROOT / "project" / name, project / name)
        shutil.copyfile(ROOT / "tests/chat.gd", project / "main.gd")
        args = [exe, "--path", directory, "--rendering-method", "gl_compatibility",
                "--max-fps", "20", "--audio-driver", "Dummy", "--log-file", str(project / "game.log")]
        process = subprocess.Popen(args)
        try:
            check_chat(project, process)
            if process.wait(timeout=10) != 0:
                raise RuntimeError("Godot exited with an error")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()


if __name__ == "__main__":
    main()
