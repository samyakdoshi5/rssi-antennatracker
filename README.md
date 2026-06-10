# RX5808 Diversity Antenna Tracker

Arduino sketch for an RX5808/RX58xx diversity receiver adapted into an RSSI-based pan tracker. The current build targets a 128x32 SSD1306 OLED and adds a tracker mode for two directional antennas mounted on a continuous-rotation servo.

## Hardware

- Arduino-compatible AVR board
- Two RX5808/RX58xx receiver modules
- 128x32 SSD1306 I2C OLED
- Continuous-rotation 360 servo for pan movement
- Four buttons: Up, Down, Mode, Save
- Active buzzer

## Pin Map

| Function | Pin |
| --- | --- |
| Button Up | `2` |
| Button Down | `3` |
| Button Mode | `4` |
| Button Save | `5` |
| Buzzer | `6` |
| Tracker servo | `7` |
| SPI data | `10` |
| SPI slave select | `11` |
| SPI clock | `12` |
| Board LED | `13` |
| Receiver A LED/control | `A0` |
| Receiver B LED/control | `A1` |
| Receiver A RSSI | `A7` |
| Receiver B RSSI | `A6` |

## Display

The sketch is configured for:

```cpp
#define OLED_128x32_ADAFRUIT_SCREENS
```

The old screensaver has been removed. The UI is compressed for the 128x32 OLED.

## Modes

- `Search`: normal channel search view.
- `Band Scan`: scans channels and draws RSSI history.
- `Tracker`: compares calibrated A/B RSSI and drives the pan servo.
- `Settings`: RSSI calibration entry point.

Hold `MODE` to open the menu. In tracker mode, short-press `SAVE` to stop/recenter the servo command.

## Tracker Mode

Tracker mode assumes:

- Receiver `A` antenna points left.
- Receiver `B` antenna points right.
- A stronger calibrated `A` RSSI means the target is left.
- A stronger calibrated `B` RSSI means the target is right.

The tracker display shows:

```text
A:cal/raw    B:cal/raw
winner       channel
PWM:us       direction graphic
```

Example:

```text
A:72/184     B:65/211
A LEFT       R1
PWM:1560     <----
```

The tracker compares calibrated RSSI percentages, not raw ADC readings. This matters because two receiver modules often have different raw RSSI ranges.

## RSSI Calibration

Run RSSI calibration from `Settings` before using tracker mode.

The calibration stores independent min/max values for both modules:

- `rssiAMin`, `rssiAMax`
- `rssiBMin`, `rssiBMax`

The receiver code maps raw readings into calibrated `0..100` RSSI values. Tracker mode uses those calibrated values for decisions and only displays raw values for diagnosis.

## Servo Control

The tracker is set up for a continuous-rotation servo using microsecond pulses:

```cpp
#define TRACKER_SERVO_STOP_US 1500
#define TRACKER_SERVO_LEFT_MIN_US 1505
#define TRACKER_SERVO_LEFT_MAX_US 1850
#define TRACKER_SERVO_RIGHT_MIN_US 1495
#define TRACKER_SERVO_RIGHT_MAX_US 1150
```

Tune `TRACKER_SERVO_STOP_US` first. If the servo creeps while stopped, adjust it slightly, for example `1490`, `1510`, etc.

The min values define the deadband edge. Once the tracker decides to move, it will command at least:

- `TRACKER_SERVO_LEFT_MIN_US` when moving left
- `TRACKER_SERVO_RIGHT_MIN_US` when moving right

The max values cap top speed in each direction.

## Tracker Tuning

Main tracker parameters are in `settings.h`.

```cpp
#define TRACKER_RSSI_MOVE_THRESHOLD 10
#define TRACKER_RSSI_STOP_THRESHOLD 5
#define TRACKER_MOVE_INTERVAL 20
#define TRACKER_MAX_SPEED_OFFSET_US 350
#define TRACKER_SERVO_SMOOTH_INTERVAL 5
#define TRACKER_SERVO_SMOOTH_STEP_US 70
```

- `TRACKER_RSSI_MOVE_THRESHOLD`: calibrated RSSI percent difference needed to start moving.
- `TRACKER_RSSI_STOP_THRESHOLD`: calibrated RSSI percent difference where movement stops. Keep below move threshold for hysteresis.
- `TRACKER_MOVE_INTERVAL`: PID target-speed update interval in ms.
- `TRACKER_MAX_SPEED_OFFSET_US`: max speed offset from neutral before direction caps.
- `TRACKER_SERVO_SMOOTH_INTERVAL`: how often the output pulse slews toward the target.
- `TRACKER_SERVO_SMOOTH_STEP_US`: max microsecond change per smoothing tick.

PID values:

```cpp
#define TRACKER_PID_KP 120
#define TRACKER_PID_KI 0
#define TRACKER_PID_KD 80
#define TRACKER_PID_SCALE 16
#define TRACKER_PID_INTEGRAL_MAX 80
```

- Raise `KP` for faster response to RSSI error.
- Keep `KI` at `0` unless you need persistent bias correction.
- Raise `KD` to damp overshoot when the error is shrinking.
- Raise `TRACKER_PID_SCALE` to make the whole PID output less aggressive.

## Serial Debug

Serial RSSI output is enabled:

```cpp
#define USE_SERIAL_OUT
```

Open Serial Monitor at `250000` baud. Output is tab-separated:

```text
channel    rssiA    rssiA_raw    rssiB    rssiB_raw
```

## Build Notes

Required Arduino libraries:

- `Adafruit_SSD1306`
- `Adafruit_GFX`
- `Servo`

The sketch uses placement `new` through the standard `<new>` header. Do not add a custom placement `operator new`; newer Arduino cores already provide it and duplicate definitions will fail linking.

## Quick Tuning Order

1. Tune `TRACKER_SERVO_STOP_US` until the servo does not creep.
2. Set `TRACKER_SERVO_LEFT_MIN_US` and `TRACKER_SERVO_RIGHT_MIN_US` just outside the servo deadband.
3. Run RSSI calibration.
4. If it chases noise, raise `TRACKER_RSSI_MOVE_THRESHOLD` and `TRACKER_RSSI_STOP_THRESHOLD`.
5. If it reacts too slowly, raise `TRACKER_PID_KP` or `TRACKER_MAX_SPEED_OFFSET_US`.
6. If it overshoots, raise `TRACKER_PID_KD` or lower `TRACKER_PID_KP`.
