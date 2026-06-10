/*
  Button Test Sketch for rx5808-pro-diversity

  Open Serial Monitor at 115200 baud.
  Buttons are expected to use INPUT_PULLUP:
  - Not pressed = HIGH
  - Pressed     = LOW
*/

#define PIN_BUTTON_UP    2
#define PIN_BUTTON_DOWN  3
#define PIN_BUTTON_MODE  4
#define PIN_BUTTON_SAVE  5

#define DEBOUNCE_MS 50

struct ButtonTest {
  const char *name;
  uint8_t pin;
  bool stablePressed;
  bool lastRawPressed;
  unsigned long lastChangeMs;
  unsigned long pressedAtMs;
};

ButtonTest buttons[] = {
  { "UP",   PIN_BUTTON_UP,   false, false, 0, 0 },
  { "DOWN", PIN_BUTTON_DOWN, false, false, 0, 0 },
  { "MODE", PIN_BUTTON_MODE, false, false, 0, 0 },
  { "SAVE", PIN_BUTTON_SAVE, false, false, 0, 0 },
};

const uint8_t buttonCount = sizeof(buttons) / sizeof(buttons[0]);

void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < buttonCount; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }

  Serial.println();
  Serial.println("Button test started");
  Serial.println("Press each button. You should see PRESS and RELEASE events.");
  Serial.println("Using INPUT_PULLUP: pressed = LOW");
  Serial.println();
}

void loop() {
  unsigned long now = millis();

  for (uint8_t i = 0; i < buttonCount; i++) {
    ButtonTest &btn = buttons[i];

    bool rawPressed = digitalRead(btn.pin) == LOW;

    if (rawPressed != btn.lastRawPressed) {
      btn.lastRawPressed = rawPressed;
      btn.lastChangeMs = now;
    }

    if ((now - btn.lastChangeMs) >= DEBOUNCE_MS && rawPressed != btn.stablePressed) {
      btn.stablePressed = rawPressed;

      if (btn.stablePressed) {
        btn.pressedAtMs = now;
        Serial.print(btn.name);
        Serial.println(" PRESS");
      } else {
        unsigned long duration = now - btn.pressedAtMs;

        Serial.print(btn.name);
        Serial.print(" RELEASE after ");
        Serial.print(duration);
        Serial.println(" ms");

        if (duration < 500) {
          Serial.println("  -> SHORT press");
        } else if (duration < 2000) {
          Serial.println("  -> LONG press");
        } else {
          Serial.println("  -> VERY LONG / HOLD");
        }
      }
    }
  }
}