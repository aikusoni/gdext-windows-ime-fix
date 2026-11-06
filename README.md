# gdext-windows-ime-fix

Godot 4.7.2 Windows 창에서 새 Microsoft 한국어 입력기로 글을 칠 때 확정 글자가 빠지는 문제를, 엔진을 고치지 않고 작은 GDExtension으로 막는 시험이에요. 아직 시험 단계이고 완성품이 아니에요.

## 문제

`가나다`를 치면 입력칸에 `다`만 남아요. 조합 중인 글자는 보이는데, 다음 음절이 이어지는 음절은 확정되면서 사라져요.

## 원인

Godot 4.7.2 `platform/windows/display_server_windows.cpp`의 `WM_IME_STARTCOMPOSITION` 처리에 `key_event_pos--`가 있어요. 조합을 시작한 키를 큐에서 지우려는 코드예요.

새 입력기는 한 음절을 확정하자마자 다음 조합을 시작해요.

```
WM_IME_COMPOSITION (확정 글자) → WM_IME_ENDCOMPOSITION → WM_CHAR 가 → WM_IME_STARTCOMPOSITION
```

Godot는 키 큐를 프레임마다 한 번 읽어요. 그래서 그 사이에 온 `STARTCOMPOSITION`이 큐 맨 뒤의 `WM_CHAR`, 곧 방금 확정한 글자를 지워요. 다음 조합이 이어지지 않은 음절만 살아남아요.

입력기 자체는 문제가 없어요. 이 시험에서 `WM_IME_COMPOSITION`이 조합 글자와 확정 글자를 빠짐없이 줬어요. 그래서 TSF는 필요 없어 보여요.

## 고치는 방식

확장이 게임 창의 메시지 처리기 앞에 서요. 직전 키 메시지가 `WM_CHAR`일 때 `WM_IME_STARTCOMPOSITION`을 Godot에 넘기기 전에, 빈 문자 이벤트(`WM_CHAR` 0)를 하나 먼저 넣어요. Godot는 그 빈 문자를 지우고, 확정 글자는 남아요.

코드는 `src/ime_probe.c`의 `probe_proc`에 있고 몇 줄이에요. Godot 호출은 진입점뿐이라 생성 헤더나 godot-cpp가 필요 없어요.

## 구성

- `src/ime_probe.c`: 확장. 메시지를 파일에 적는 일도 해요.
- `project/`: 입력칸 하나짜리 시험 프로젝트. DLL이 없거나 Windows가 아니면 확장을 불러오지 않고 그냥 돌아요.
- `build.bat`: Visual Studio 2019 Community의 C 컴파일러로 빌드해요. 다른 판이면 안의 경로를 고쳐 주세요.
- `run_probe.bat`: 시험 창을 띄워요. 환경 변수 `GODOT_EXE`에 Godot 4.7 실행 파일 경로를 넣어 주세요.

`IME_PROBE_MODE`로 동작을 골라요.

| 값 | 동작 |
|---|---|
| `observe` (기본) | 메시지를 적기만 하고 전부 Godot에 넘겨요 |
| `fix` | 위의 수정을 켜요 |
| `take` | 조합 메시지를 Godot에 안 넘겨요. 입력칸에 글자가 안 들어가는 것이 정상이에요 |

`logs`에는 친 글자가 그대로 남아요. 저장소에 올리지 마세요(`.gitignore`에 있어요).

## 확인한 범위

Godot 4.7.2, Windows 11, Compatibility 렌더러, PC 한 대 기준이에요.

- `observe`에서 `가나다 스페이스 한글 Enter`가 `다 글`이 돼요(실제 입력).
- `fix`에서 같은 입력이 `가나다 한글`이 돼요(실제 입력).
- 같은 메시지 순서를 자동으로 보내는 자기 시험(`IME_PROBE_SELFTEST=2`)에서 `observe`는 `나`, `fix`는 `가나`예요.
- 시험용 `WM_CHAR`가 한 번만 들어가요. DLL이 없어도 오류 줄 없이 돌아가요.

## 아직 안 본 범위

- 받침이 든 글자와 조합 중 백스페이스 (실제 입력 재시험 대기)
- 조합 중인 글자가 입력칸에 제대로 보이는지
- 일본어 입력기와 그 밖의 입력기, 이전 버전 Microsoft 입력기에서 수정이 해가 없는지
- 다른 PC, 다른 Windows 판, 다른 Godot 판
- 내보낸(export) 빌드
- 창이 여러 개일 때. 지금은 메인 창 하나만 가로채요.
- `fix`의 빈 문자가 키 큐가 가득 찬 때 어떻게 되는지

원인 설명은 위 소스와 메시지 기록을 맞춰 본 것이에요. Godot 쪽에 같은 보고나 고침이 있는지는 찾아보지 않았어요. 엔진에서 고치는 것이 근본 해결이고, 고쳐지면 이 확장은 필요 없어요.

## 정할 것

라이선스를 아직 정하지 않았어요. 저장소 주인이 정할 항목이에요. 정하기 전까지 다른 곳에 쓰거나 공개하지 마세요.
