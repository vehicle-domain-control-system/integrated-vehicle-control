# BCM 노드 통합 폴더: 알려진 결함

보드 시험(2026-10-29, S32K144 평가 보드 + S32K344 디버그 노드)에서 찾은 결함이다. 이 폴더의 코드는 보드에서 확인한 판 그대로 두었고 고치지 않았다. 고칠 때는 원본(BCM 원본, `bcm_net`)에서 고친 뒤 이 폴더를 다시 만든다(README 6절).

경로 표기: `<RTD>`는 S32K1 RTD 3.0.0 설치 폴더의 `eclipse/plugins`다. 나머지 경로는 이 폴더 기준이다.

## BCM-1 실내등 PWM duty 단위가 RTD와 다르다

- **상태:** 열림
- **증상:** 밝기 1~24 %는 실제 duty가 약 4배로 들어가고, 25 % 이상은 PWM에 반영되지 않는다. 밝기 100 % 명령은 DONE으로 끝나지만 LED가 켜지지 않는다.
- **근거:**
  - 앱 쪽 duty 계산: `src/bcm/bcm_port_config.h:20` `BCM_PORT_PWM_DUTY_FULL 0x8000u`, `src/bcm/bcm_port_bench144.c:21-30` `PctToDuty()`가 `pct * 0x8000 / 100`을 `Ftm_Pwm_Ip_UpdatePwmDutyCycleChannel()`에 넘긴다.
  - 이 값은 `bcm_net`에서 정한 것이 아니라 **BCM 원본에서 왔다**: 원본 `src/port/bcm_port_config.h:17` `BCM_PORT_PWM_DUTY_FULL 0x8000u`, 원본 `src/port/bcm_port_rtd.c:47-56`(`PctToDuty`, `SetDuty`)와 같다.
  - RTD IP 계층은 duty를 주기 tick으로 받는다.
    - `<RTD>/Pwm_TS_T40D2M30I0R0/src/Ftm_Pwm_Ip.c:1594`: `uint16 FirstEdge = DutyCycle;` 변환 없이 tick으로 쓴다.
    - 같은 파일 `:1634`: `(void) Ftm_Pwm_Ip_UpdatePwmChannel(...)`로 넘기고 반환값을 버린다.
    - 같은 파일 `:1940`: `if (((FirstEdge <= FtmPeriod) && (SecondEdge <= FtmPeriod)) || (0U == FtmPeriod))`
    - 같은 파일 `:1947`: 아니면 `RetStatus = FTM_PWM_IP_STATUS_ERROR;`이다. 이 경우 CnV 쓰기(`:1969`)와 소프트웨어 트리거를 하지 않는다. 즉 주기를 넘는 값은 **잘리지 않고 거절된다**(CnV는 이전 값 그대로).
    - 주기는 `s32ds/bcm144_board_tested.mex:654` `PwmFtmPeriod 8000`이다.
  - 0x8000을 100 %로 보는 것은 AUTOSAR Pwm 계층의 관례다. `<RTD>/Pwm_TS_T40D2M30I0R0/src/Pwm_Ipw.c:483`이 `Period * DutyCycle / 0x8000U`로 tick을 만들어 IP 계층에 넘긴다. 이 빌드는 그 계층을 쓰지 않는다.
  - RTD 헤더 주석은 IP 함수도 0~0x8000을 받는 것처럼 적고 있다(`<RTD>/Pwm_TS_T40D2M30I0R0/include/Ftm_Pwm_Ip.h:408`).
  - 계산값(주기 8000 tick):

    | 밝기 | 넘긴 값 | 결과 |
    |---|---|---|
    | 1 % | 327 | 약 4 % |
    | 20 % | 6,553 | 약 82 % |
    | 24 % | 7,864 | 약 98 % |
    | 25 % | 8,192 | 거절(바뀌지 않음) |
    | 100 % | 32,768 | 거절(바뀌지 않음) |

- **보드에서 본 것:**
  - 밝기 100 %·빨강(`INTERIOR_LIGHT_COLOR=0xFF0000`): 두 번 보냈고(COMMAND_ID 1, 2) 두 번 모두 BCM_RESULT RESULT 2(DONE), REASON 0이었다. 두 번째 뒤에 LED가 꺼진 채인 것을 확인했다.
  - 밝기 20 %·빨강: LED가 켜졌다.
- **아직 시험하지 않은 것:** 밝기 표(1, 6, 12, 24, 30, 50 %)는 명령을 보냈지만 LED 관찰을 기록하지 않았다. 빨강 외의 색도 관찰을 기록하지 않았다.
- **원인:** 확인됨(RTD 소스). 보드 관찰(100 %에서 꺼짐, 20 %에서 켜짐)과 맞는다.
- **영향:** 밝기 25 % 이상 명령이 LED에 반영되지 않는다. 24 % 이하는 실제보다 밝다. 채널마다 색 비율 × 밝기로 계산하므로(`src/bcm/bcm/bcm_light.c`), 실내등 종류 1~3(GOODBYE, WARNING, FAULT)의 고정색이나 섞은 색도 채널 값이 24 %를 넘는 채널은 반영되지 않아 색이 달라진다. 앱은 이를 모르고 DONE과 APPLIED로 보고한다(BCM-2).
- **고칠 곳과 안:** BCM 원본 `src/port/bcm_port_rtd.c`의 `PctToDuty()`와, 같은 함수를 옮긴 `bcm_net/src/bcm_port_bench144.c`.
  - 100 % 기준을 상수 `0x8000` 대신 Ftm 구성 주기에서 가져온다. 예: `Ftm_Pwm_Ip_UserCfg0.InstanceCfg->PwmPeriod`(`<RTD>/Pwm_TS_T40D2M30I0R0/include/Ftm_Pwm_Ip_Types.h:418`, `:535`), 또는 RTD가 초기화 때 채우는 `Ftm_Pwm_Ip_aPeriod[인스턴스]`(`Ftm_Pwm_Ip.h:202`).
  - 계산: `duty = pct * period / 100`(pct ≤ 100이면 period를 넘지 않는다).
  - 그다음 `BCM_PORT_PWM_DUTY_FULL`을 없앤다(원본과 `bcm_net` 두 곳).
- **고친 뒤 확인 방법:**
  - 밝기 0/1/25/50/100 %에서 FTM0 CnV(또는 `Ftm_Pwm_Ip_aDutyCycle[0][0]`)가 0/80/2000/4000/8000인지 디버거로 본다. 채널 R·G·B 모두 본다.
  - 경계: 0 %는 출력이 꺼진 채이고, 100 %는 CnV가 주기 값 8000으로 MOD(주기 - 1 = 7999, `Ftm_Pwm_Ip.c:1015`)보다 커서 출력이 계속 켜진 채인지 본다(edge-aligned).
  - LED: 같은 색에서 밝기가 단계마다 달라지는지 눈으로 본다.
  - 344 디버그 노드로 시험할 때: 줄마다 `TX_DONE ... COMMAND_ID=n`의 n이 바뀌는지 본다. 같은 원 명령(DOMAIN BOOT, COMMAND_ID)은 BCM에 한 번만 전달되므로, 번호가 안 바뀐 줄은 시험 결과로 세지 않는다.

## BCM-2 BcmPort_LightApply()가 늘 true를 돌려준다

- **상태:** 열림
- **증상:** RTD 호출이 실패해도 BCM_LIGHT_STATE가 적용됨(APPLY_RESULT 0)으로 보고하고, 조명 명령 결과가 DONE이 된다. 조명 적용 고장(LIGHT_APPLY, 고장 비트 6)이 생기지 않는다.
- **근거:**
  - `src/bcm/bcm_port_bench144.c:117-124`: `SetDuty()` 세 번 뒤 무조건 `return true;`다. 원본 `src/port/bcm_port_rtd.c:179-192`도 같다.
  - `Ftm_Pwm_Ip_UpdatePwmDutyCycleChannel()`은 `void`다(`<RTD>/Pwm_TS_T40D2M30I0R0/include/Ftm_Pwm_Ip.h:414`). 안에서 `Ftm_Pwm_Ip_UpdatePwmChannel()`의 반환값(`Ftm_Pwm_Ip_StatusType`, `Ftm_Pwm_Ip.h:263`)을 버린다(`Ftm_Pwm_Ip.c:1634`).
  - 앱은 이 반환값으로 적용 결과를 정한다: `src/bcm/bcm/bcm_app.c:457` `BcmLight_OnApplied(BcmPort_LightApply(...))`, `src/bcm/bcm/bcm_light.c:145-180`(`BcmLight_OnApplied`: true면 APPLIED, false면 applyFault), `:182-195`(`BcmLight_Tick`: DONE 또는 FAILED 결과).
  - 보드: 밝기 100 % 명령에서 LED가 꺼진 채인데 BCM_RESULT가 DONE이었다(BCM-1).
- **원인:** 확인됨(코드).
- **영향:** DOMAIN이 받는 조명 결과와 상태가 실제 출력과 다를 수 있다. BCM-1이 고쳐져도 RTD 쪽 실패(잘못된 인스턴스, 주기 밖 값)를 알 수 없다.
- **고칠 곳과 안:** BCM 원본 `src/port/bcm_port_rtd.c`와 `bcm_net/src/bcm_port_bench144.c`의 `SetDuty()`·`BcmPort_LightApply()`.
  - RTD 반환값을 돌려준다. `Ftm_Pwm_Ip_UpdatePwmDutyCycleChannel()`은 값을 돌려주지 않는다. 그래서 상태를 돌려주는 `Ftm_Pwm_Ip_UpdatePwmChannel(인스턴스, 채널, duty, 0, TRUE)`(edge-aligned에서 FirstEdge가 duty)를 쓰고 `FTM_PWM_IP_STATUS_SUCCESS`인지 본다.
  - 또는 호출 전에 `duty <= 주기`를 검사해 넘으면 false를 돌려준다.
  - 두 경우 모두 `SetDuty()`가 bool을 돌려주고, `BcmPort_LightApply()`는 세 채널이 모두 성공했을 때만 true를 돌려준다.
  - `Ftm_Pwm_Ip_UpdatePwmChannel()`을 쓰면 `UpdatePwmDutyCycleChannel()`이 하던 채널 상태 갱신(`Ftm_Pwm_Ip.c:1606-1622`)이 빠지므로, 그 차이를 확인한 뒤 고른다.
- **고친 뒤 확인 방법:**
  - 실패를 일부러 만든다(예: 시험 빌드에서 주기보다 큰 duty를 넘김). BCM_RESULT가 RESULT 5(FAILED)·REASON 14(FAULT), BCM_LIGHT_STATE의 APPLY_RESULT가 1(FAILED), BCM_STATUS의 FAULT_MASK에 비트 6(0x40)이 서고 ECU가 2(DEGRADED)인지 본다.
  - 정상 명령에서는 BCM-1 확인 방법의 밝기 표가 그대로 나오는지 본다.
  - 344 시험에서는 BCM-1과 같이 줄마다 COMMAND_ID가 바뀌는지 본다.
