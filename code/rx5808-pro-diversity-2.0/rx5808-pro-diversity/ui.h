#ifndef UI_H
#define UI_H


#include <stdint.h>

#include "settings.h"

#ifdef OLED_128x32_ADAFRUIT_SCREENS
    #ifndef SSD1306_128_32
        #define SSD1306_128_32
    #endif
#elif defined(OLED_128x64_ADAFRUIT_SCREENS)
    #ifndef SSD1306_128_64
        #define SSD1306_128_64
    #endif
#endif

#include <Adafruit_SSD1306.h>
#include "settings_internal.h"


#define SCREEN_WIDTH 128
#ifdef OLED_128x32_ADAFRUIT_SCREENS
    #define SCREEN_HEIGHT 32
#else
    #define SCREEN_HEIGHT 64
#endif

#define SCREEN_WIDTH_MID ((SCREEN_WIDTH / 2) - 1)
#define SCREEN_HEIGHT_MID ((SCREEN_HEIGHT / 2) - 1)

#define CHAR_WIDTH 5
#define CHAR_HEIGHT 7


namespace Ui {
    extern OLED_CLASS display;
    extern bool shouldDrawUpdate;
    extern bool shouldDisplay;
    extern bool shouldFullRedraw;

    void setup();
    void update();

    void drawGraph(
        const uint8_t data[],
        const uint8_t dataSize,
        const uint8_t dataScale,
        const uint8_t x,
        const uint8_t y,
        const uint8_t w,
        const uint8_t h
    );

    void drawDashedHLine(const int x, const int y, const int w, const int step);
    void drawDashedVLine(const int x, const int y, const int w, const int step);

    void clear();
    void clearRect(const int x, const int y, const int w, const int h);

    void needUpdate();
    void needDisplay();
    void needFullRedraw();
}

#endif
