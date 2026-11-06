# gdext-windows-ime-fix

Godot 4.7.2 Windows 창에서 새 Microsoft 한국어 입력기로 입력할 때 확정 글자가 사라지는 문제를 막는 GDExtension이다. 엔진은 수정하지 않는다.

## 내 게임에 적용하기

1. [최신 릴리스의 Windows x86-64용 ZIP](https://github.com/aikusoni/gdext-windows-ime-fix/releases/latest/download/windows-ime-fix-x86_64.zip)을 내려받아 압축을 푼다.
2. 압축을 푼 `addons` 폴더를 게임의 `project.godot` 옆에 복사한다. 이미 `addons`가 있으면 그 안에 `windows_ime_fix` 폴더만 복사한다.
3. Godot 편집기에서 프로젝트를 다시 열면 확장을 자동으로 불러온다.

```text
내 게임/
├── project.godot
└── addons/windows_ime_fix/
    ├── windows_ime_fix.dll
    ├── windows_ime_fix.gdextension
    └── LICENSE
```

### 한글 입력 확인

게임을 별도 창으로 실행하고 `LineEdit` 입력칸을 클릭한다. Windows의 새 Microsoft 한국어 입력기를 한글 모드로 전환한 뒤 **가나다 → 스페이스 → 한글 → Enter** 순서로 직접 입력한다. 마지막 음절까지 확정했을 때 기대하는 문자열은 **`가나다 한글`**이다.

여전히 `다 글`만 남는다면 편집기의 **출력(Output)** 패널에서 확장 로드 오류를 확인한다. DLL과 설정 파일이 같은 폴더에 있는지, 파일 확장자가 `.gdextension.txt`로 저장되지는 않았는지, 실행 중인 Godot가 Windows x86-64용인지 확인한다.

## 문제

`가나다`를 입력하면 입력칸에 `다`만 남는다. 뒤에 다음 음절이 이어진 음절은 확정되면서 사라진다.

## 원인

Godot 4.7.2 `platform/windows/display_server_windows.cpp`의 `WM_IME_STARTCOMPOSITION` 처리에 `key_event_pos--`가 있다. 조합을 시작한 키를 키 큐에서 지우는 코드다.

새 입력기는 한 음절을 확정한 직후 다음 조합을 시작한다.

```
WM_IME_COMPOSITION (확정 글자) → WM_IME_ENDCOMPOSITION → WM_CHAR → WM_IME_STARTCOMPOSITION
```

Godot는 키 큐를 프레임마다 한 번 읽는다. `WM_CHAR`와 다음 `WM_IME_STARTCOMPOSITION`이 같은 프레임 안에 오면, 큐 맨 뒤의 확정 글자가 지워진다. 그 사이에 큐를 읽으면 글자는 남는다.

입력기는 `WM_IME_COMPOSITION`으로 조합 글자와 확정 글자를 모두 전달한다.

## 고침

확장은 조합 시작 메시지(`WM_IME_STARTCOMPOSITION`)를 받을 때, 직전 키 메시지가 `WM_CHAR`이고 창에 IME 입력 컨텍스트가 연결돼 있을 때만 빈 문자(`WM_CHAR` 0)를 먼저 보낸다. Godot는 빈 문자를 지우고 확정 글자는 남긴다. Godot 4.7.2는 창의 IME를 끌 때 입력 컨텍스트를 분리하므로, 채팅 입력을 끝낸 뒤 늦게 도착한 조합 시작 메시지는 그대로 전달된다.

Godot 호출은 진입점뿐이므로 생성 헤더와 godot-cpp가 필요 없다.

## 구성

- `src/ime_fix.c`: 고침만 담은 확장. 아무것도 기록하지 않는다. 진입점은 `ime_fix_init`, 산출물은 `build/windows_ime_fix.dll`이다.
- `src/ime_probe.c`: 시험용 확장. 메시지와 입력한 글자를 파일에 기록한다.
- `project/`: 입력칸 하나로 된 시험 프로젝트. DLL이 없거나 Windows가 아니면 확장을 불러오지 않는다.
- `build.bat`: Visual C++의 x64 도구로 게임용 DLL과 시험용 DLL을 빌드한다.
- `run_probe.bat`: 시험 창을 띄운다. 환경 변수 `GODOT_EXE`에 Godot 4.7 실행 파일 경로를 지정한다.

## 시험 프로젝트로 비교하기

소스에서 빌드하려면 Visual Studio의 **C++를 사용한 데스크톱 개발** 워크로드와 Windows SDK를 설치한다. 기본 경로는 Visual Studio 2019 Community 기준이다. 다른 설치를 사용한다면 PowerShell의 `$env:VCVARS64`에 해당 설치의 `VC\Auxiliary\Build\vcvars64.bat` 전체 경로를 지정한다.

저장소 폴더의 PowerShell에서 아래 실행 파일 경로를 실제 Godot 4.7.2 경로로 바꾸고 실행한다. 빌드 마지막 줄에 `IME_PROBE_BUILD_OK`가 나오면 두 DLL이 준비된 것이다. 각 창에서 같은 문구를 입력하고 창을 닫은 뒤 콘솔에서 키를 눌러 다음 실행으로 넘어간다.

```powershell
.\build.bat
$env:GODOT_EXE = 'C:\Tools\Godot\Godot.exe'
$env:IME_PROBE_MODE = 'observe'
.\run_probe.bat
$env:IME_PROBE_MODE = 'fix'
.\run_probe.bat
```

`run_probe.bat`에서 모드를 생략하면 `fix`로 실행한다. 시험용 DLL을 직접 로드할 때의 기본값은 `observe`다.

| 값 | 동작 |
|---|---|
| `observe` | 메시지를 기록하고 전부 Godot에 넘긴다 |
| `fix` | 위의 고침을 적용한다 |
| `take` | 조합 메시지를 Godot에 넘기지 않는다. 입력칸에 글자가 들어가지 않는다 |

`IME_PROBE_SELFTEST`는 메시지 순서를 자동으로 보낸다. `2`는 같은 프레임, `3`은 간격을 둔 순서다. 시험 프로젝트에 `-- --autotest --slow`를 주면 프레임을 늦춘다.

`logs/observe_godot.log`와 `logs/fix_godot.log`의 `text_submitted` 줄을 비교하면 Enter로 확정한 문자열을 볼 수 있다. **시험 창에 입력한 글자는 로그에 그대로 남으므로 시험 문구만 입력한다.**

### 게임용 DLL 자동 테스트

Windows x86-64에서 Python 3와 Godot 4.7.2를 준비한다. 저장소 폴더에서 아래 명령을 실행한다.

```powershell
.\build.bat
$env:GODOT_EXE = 'C:\Tools\Godot\Godot.exe'  # 설치한 Godot 실행 파일 경로
python tests/test_chat.py
```

시험 창에 Windows 한글 확정 메시지를 보내고 입력칸을 닫은 뒤 지연된 조합 시작 메시지를 보낸다. `PASS`와 함께 입력 결과 `가나다 한글`, 닫은 뒤 키 누름 `0`이 출력되면 통과다. 실제 자판과 입력기를 확인할 때는 위의 수동 입력 절차를 사용한다.

## 확인 범위

Godot 4.7.2, Windows 11, Compatibility 렌더러, PC 한 대에서 확인했다.

| 입력 | `observe` | `fix` |
|---|---|---|
| 실제 입력 `가나다 스페이스 한글 Enter` | `다 글` | `가나다 한글` |
| 자기 시험 같은 프레임 (보통, 느린 프레임) | `나` | `가나` |
| 자기 시험 간격 있음 (보통, 느린 프레임) | `가나` | `가나` |

받침이 든 글자(`값`, `없다`), 조합 중 백스페이스(`닭`에서 `달`), Enter 뒤 이어 입력은 `fix`에서 실제 입력으로 확인했다.

### 게임 조작에 미치는 영향

Godot 4.7.2의 별도 게임 창에 같은 Windows 메시지를 보내고, 게임용 DLL을 설치한 경우와 설치하지 않은 경우의 `_input()` 이벤트를 비교했다. W, Space, Esc, 왼쪽 방향키 메시지와 마우스 왼쪽 버튼의 누름·뗌은 두 경우에 같았다.

채팅 입력칸(`LineEdit`)에서 음절을 연속으로 확정하고, 포커스를 해제해 입력칸을 숨긴 뒤 지연된 조합 시작 메시지를 보냈다. 마지막 열은 입력칸을 닫은 뒤 게임의 `_input()`에 전달된 가짜 물리 Pause 키 누름 횟수다.

| 게임용 DLL | 같은 프레임에 연속 입력 | 음절 사이에 프레임 간격을 둔 입력 | 닫은 뒤 가짜 Pause |
|---|---|---|---|
| 미설치 | `다 글` | `가나다 한글` | 0회 |
| 수정 전 | `가나다 한글` | `가나다 한글` | 1회 |
| 수정 후 | `가나다 한글` | `가나다 한글` | 0회 |

표의 시험은 Windows 메시지 순서를 재생했다. 별도 시험에서는 수정한 게임용 DLL을 로드한 창에서 Microsoft 한국어 입력기에 두벌식 키를 자동 입력하고, Enter로 확정한 문자열이 `가나다 한글`인지 확인했다. 게임패드와 여러 키를 동시에 누르는 단축키는 실기기 시험 범위 밖이다.

DLL이 없으면 오류 줄 없이 실행된다.

확인하지 않은 범위: 일본어와 그 밖의 입력기, 이전 버전 Microsoft 입력기, 다른 PC와 Windows 판, 다른 Godot 판, 내보낸 빌드, 창이 여러 개인 경우.

## 라이선스

이 프로젝트는 [MIT 라이선스](LICENSE)로 배포한다. 확장 DLL을 게임에 포함해 배포할 때는 라이선스 파일을 함께 제공하면 된다.
