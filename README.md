# gdext-windows-ime-fix

Godot 4.7.2 Windows 창에서 새 Microsoft 한국어 입력기로 입력할 때 확정 글자가 사라지는 문제를 막는 GDExtension이다. 엔진은 수정하지 않는다.

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

확장이 게임 창의 메시지 처리기 앞에 선다. 직전 키 메시지가 `WM_CHAR`이면 `WM_IME_STARTCOMPOSITION`을 Godot에 넘기기 전에 빈 문자(`WM_CHAR` 0)를 먼저 넣는다. Godot는 빈 문자를 지우고 확정 글자는 남는다.

Godot 호출은 진입점뿐이므로 생성 헤더와 godot-cpp가 필요 없다.

## 구성

- `src/ime_probe.c`: 확장. 메시지를 파일에 기록한다.
- `project/`: 입력칸 하나로 된 시험 프로젝트. DLL이 없거나 Windows가 아니면 확장을 불러오지 않는다.
- `build.bat`: Visual Studio 2019 Community의 C 컴파일러로 빌드한다. 다른 판은 파일 안의 경로를 수정한다.
- `run_probe.bat`: 시험 창을 띄운다. 환경 변수 `GODOT_EXE`에 Godot 4.7 실행 파일 경로를 지정한다.

`IME_PROBE_MODE`로 동작을 고른다.

| 값 | 동작 |
|---|---|
| `observe` (기본) | 메시지를 기록하고 전부 Godot에 넘긴다 |
| `fix` | 위의 고침을 적용한다 |
| `take` | 조합 메시지를 Godot에 넘기지 않는다. 입력칸에 글자가 들어가지 않는다 |

`IME_PROBE_SELFTEST`는 메시지 순서를 자동으로 보낸다. `2`는 같은 프레임, `3`은 간격을 둔 순서다. 시험 프로젝트에 `-- --autotest --slow`를 주면 프레임을 늦춘다.

기록 파일 `logs`에는 입력한 글자가 그대로 남는다.

## 확인 범위

Godot 4.7.2, Windows 11, Compatibility 렌더러, PC 한 대에서 확인했다.

| 입력 | `observe` | `fix` |
|---|---|---|
| 실제 입력 `가나다 스페이스 한글 Enter` | `다 글` | `가나다 한글` |
| 자기 시험 같은 프레임 (보통, 느린 프레임) | `나` | `가나` |
| 자기 시험 간격 있음 (보통, 느린 프레임) | `가나` | `가나` |

DLL이 없으면 오류 줄 없이 실행된다.

확인하지 않은 범위: 받침이 든 글자와 조합 중 백스페이스, 일본어와 그 밖의 입력기, 이전 버전 Microsoft 입력기, 다른 PC와 Windows 판, 다른 Godot 판, 내보낸 빌드, 창이 여러 개인 경우.
