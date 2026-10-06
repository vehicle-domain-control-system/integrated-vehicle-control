# Pi Vision — 라즈베리파이 카메라 사람 수 인식 → CIS 전송

Raspberry Pi Camera Module 3로 사람 수(0~5)를 세어 OpenSDA USB 시리얼(UART)로 CIS(S32K144)에 보낸다.
이 링크는 **CIS 모듈 내부 통신**이다. 프레임 형식은 `pi_protocol.py` 맨 위 설명과
`firmware/CIS/src/pi_protocol.h`에 있으며, 두 구현은 서로 같은 형식을 쓴다.

> 영상은 메모리에서만 처리하고 파일로 저장하거나 외부로 보내지 않는다 (CIS-SYS-FUN-024).
> 보내는 것은 사람 수와 품질 정보뿐이다.

## 구성

| 파일 | 역할 |
|---|---|
| `vision_sender.py` | 실행 스크립트. 판정 + 100 ms마다 전송 + CIS 응답 수신 |
| `person_counter.py` | 카메라(`picamera2`) 입력과 사람 검출기(YOLO / HOG) |
| `pi_protocol.py` | 프레임 만들기·해석, CRC16 |
| `tests/test_pi_protocol.py` | 프로토콜 단위 시험 (`python3 -m unittest discover -s tests`) |

## 라즈베리파이 설치

드라이버는 필요 없다. OpenSDA USB는 Linux 기본 드라이버(`cdc_acm`)로 `/dev/ttyACM0` 같은 시리얼 포트가 된다.

```bash
sudo apt update
sudo apt install -y python3-picamera2 python3-opencv python3-serial python3-numpy
sudo usermod -aG dialout $USER        # 시리얼 포트 사용 권한 (적용하려면 재로그인)
```

카메라가 보이는지 먼저 확인한다.

```bash
rpicam-hello --list-cameras           # 구버전 OS: libcamera-hello --list-cameras
```

### Raspberry Pi 4 기준 권장 사항

- **64비트 Raspberry Pi OS(Bookworm)**를 쓴다. YOLO(ultralytics)가 쓰는 라이브러리가 64비트 환경에서만 설치된다.
- 전원은 **5V 3A** 어댑터를 쓴다. CIS 보드(OpenSDA)도 Pi의 USB에서 전원을 받고, 검출 중에는 CPU 부하가 커서 전원이 약하면 느려지거나 재부팅한다.
- Pi 4 CPU로 YOLO를 돌리면 **초당 몇 장** 수준이라 속도가 중요하다. CIS는 **1초 넘은 판정을 쓰지 않으므로**
  (`STALE`) 판정이 초당 2번 아래로 떨어지면 값이 계속 무효로 깜빡인다. 그래서:
  - 로그의 `detect=…ms`로 실제 속도를 확인한다. **600 ms를 넘으면 스크립트가 경고**한다.
  - 느리면 `--imgsz 256`처럼 입력을 줄이거나, 아래 NCNN 변환을 쓰거나, `--detector hog`로 낮춘다.
  - 기본값(`--fps 3`, 사진 3장 중앙값, 2장 모이면 결과)은 느린 검출기에 맞춘 값이다.

더 정확한 검출기(YOLO)를 쓰려면 (처음 한 번 인터넷 필요, 설치에 시간이 오래 걸린다):

```bash
python3 -m venv --system-site-packages ~/visionenv     # apt로 설치한 picamera2를 함께 쓰기 위해
source ~/visionenv/bin/activate
pip install ultralytics
```

Pi 4에서 속도를 더 내려면 NCNN 형식으로 변환해 쓴다 (ultralytics가 제공하는 방식이며, 얼마나 빨라지는지는 실측이 필요하다):

```bash
yolo export model=yolov8n.pt format=ncnn imgsz=320     # yolov8n_ncnn_model 폴더가 생긴다
python3 vision_sender.py --port auto --model yolov8n_ncnn_model --imgsz 320
```

검출기 비교:

| 검출기 | 장점 | 단점 |
|---|---|---|
| `yolo` (ultralytics, 기본 우선) | 앉은 사람·일부만 보이는 사람도 잘 잡음 | 설치가 무겁고 Pi 4에서는 느림 |
| `hog` (OpenCV 내장) | 설치 없음, 가벼움 | 서 있는 전신 위주라 차량 실내에서는 많이 놓침 |

## 연결

EVB의 OpenSDA USB(J7)를 Pi의 USB 포트에 연결한다. 포트 확인:

```bash
ls /dev/serial/by-id/                 # 포트 이름이 바뀌지 않는 경로
dmesg | tail
```

## 실행

```bash
python3 vision_sender.py --port auto                         # 카메라 + 검출기
python3 vision_sender.py --port /dev/ttyACM0 --detector hog  # 포트 지정, HOG
python3 vision_sender.py --port auto --simulate 0,1,2,3,4,5,6,u,x   # 카메라 없이 시험
python3 vision_sender.py --dry-run --simulate 0,2             # 시리얼 포트도 없이
```

시뮬레이션 값: `0`~`5` 사람 수, `6` 이상 5명 초과(범위 밖), `u` 모름, `n` 준비 안 됨, `x` 카메라 고장.
한 값을 `--sim-period` 초(기본 3) 동안 유지하고 다음 값으로 넘어간다. 실제와 같이 0.3초마다 새 판정(`SEQ` 증가)을 내서 CIS가 값을 오래된 것으로 보지 않는다.

주요 옵션: `--fps`(초당 판정 횟수, 기본 3), `--window`(중앙값에 쓰는 사진 수, 기본 3),
`--min-samples`(결과를 내기 전에 모으는 사진 수, 기본 2), `--width/--height`(기본 640x480),
`--model`/`--imgsz`/`--conf`(YOLO 가중치·입력 크기·신뢰도),
`--source`(`picamera`, USB 카메라 번호 `0`, 또는 동영상 파일 — PC에서 검출기를 시험할 때).

## 동작 규칙

- **판정과 전송은 분리**된다. 사람 수는 초당 몇 번 판정하고, 프레임은 100 ms마다 보낸다.
  판정 사이의 프레임은 같은 판정(`SEQ` 그대로)을 반복하며 `AGE`만 커진다.
  CIS는 이것으로 새 판정과 반복을 구별한다.
- 사진 몇 장의 **중앙값**을 쓴다. 한 장의 오검출이나 미검출이 결과를 바꾸지 않는다.
- 사진이 모이기 전에는 `NOT_READY`로 보내고 **"사람 없음(0)"으로 보내지 않는다**.
- 6명 이상이면 `OUT_OF_RANGE`(0~5명만 지원). 카메라 오류는 `FAULT=카메라`, 검출기 오류는 `FAULT=검출기`.
- 프로그램을 다시 시작하면 `SESSION`이 바뀌어 CIS가 이전 판정을 버린다.
- CIS가 500 ms마다 보내는 전원 허용 상태(`PERMISSION`)를 받아 값이 바뀔 때 출력한다.
  (차량 전원 허용은 아직 CIS에 연결되지 않아 `UNKNOWN`으로 온다.)

## PC에서 Pi 역할 대신 시험하기

개발 중에는 EVB를 PC에 연결해 S32DS로 디버그하므로 Pi를 연결할 수 없다.
이때는 같은 스크립트를 PC에서 돌려 Pi 역할을 대신한다.

```bash
pip install pyserial
python3 vision_sender.py --port COM5 --simulate 0,1,2,3,4,5,6,u,x       # Windows
python3 vision_sender.py --port /dev/cu.usbmodemXXXX --simulate 0,2     # macOS
```

CIS 쪽 디버거 변수: `g_occ_count`, `g_occ_presence`(0 없음, 1 있음, 255 모름), `g_occ_valid`,
`g_occ_seq`, `g_occ_age_ms`, `g_occ_reason`, `g_pi_link_up`, `g_pi_frames_ok`, `g_pi_bad_crc`,
`g_pi_overruns`(받다가 놓친 바이트 수).

## 시험 항목 (보드 연결 후)

| 시험 | 기대 |
|---|---|
| 시뮬레이션 `0,1,2,3,4,5` | `g_occ_count`가 따라가고 `g_occ_valid`=1, 0명일 때 `g_occ_presence`=0 |
| 시뮬레이션 `6` | `g_occ_valid`=0, `g_occ_reason`=2(범위 밖) |
| 시뮬레이션 `u` / `n` / `x` | `g_occ_valid`=0, 사유 6 / 1 / 4 (사람 없음으로 바뀌지 않음) |
| 스크립트 중지 | 0.3 s 뒤 `g_pi_link_up`=0, `g_occ_valid`=0 |
| 스크립트 재시작 | 새 판정부터 다시 유효, 이전 값 재사용 없음 |
| 카메라로 사람 1~3명 | 판정이 바뀌는 데 1~2초 이내, 로그 `detect=`가 600 ms 아래 |
| 오래 실행 | `g_pi_bad_crc`, `g_pi_overruns`가 거의 늘지 않음 |
