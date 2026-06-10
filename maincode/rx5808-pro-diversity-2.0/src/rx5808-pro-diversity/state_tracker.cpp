#include <Arduino.h>
#include <stdlib.h>

#include "state_tracker.h"

#include "channels.h"
#include "receiver.h"
#include "settings.h"
#include "ui.h"
#include "pstr_helper.h"


using StateMachine::TrackerStateHandler;


void TrackerStateHandler::onEnter() {
    filteredDiff = 0;
    lastError = 0;
    errorIntegral = 0;
    moveDirection = MoveDirection::STOP;
    servoPwm = TRACKER_SERVO_STOP_US;
    targetPwm = TRACKER_SERVO_STOP_US;
    panServo.attach(PIN_TRACKER_SERVO);
    panServo.writeMicroseconds(servoPwm);
    moveTimer.reset();
    servoTimer.reset();
}

void TrackerStateHandler::onExit() {
    panServo.detach();
}

void TrackerStateHandler::onUpdate() {
#ifdef USE_DIVERSITY
    if (Receiver::isRssiStable()) {
        const int16_t calibratedDiff =
            (int16_t)Receiver::rssiA - (int16_t)Receiver::rssiB;
        filteredDiff = (filteredDiff + calibratedDiff) / 2;

        updateDirection();
        updateServoTargetPwm();
    }

    smoothServo();
    Ui::needUpdate();
#endif
}

void TrackerStateHandler::updateDirection() {
    const int16_t absDiff = abs(filteredDiff);
    MoveDirection nextDirection = moveDirection;

    if (absDiff <= TRACKER_RSSI_STOP_THRESHOLD) {
        moveDirection = MoveDirection::STOP;
        lastError = filteredDiff;
        errorIntegral = 0;
        return;
    }

    if (filteredDiff >= TRACKER_RSSI_MOVE_THRESHOLD) {
        nextDirection = MoveDirection::LEFT;
    } else if (filteredDiff <= -TRACKER_RSSI_MOVE_THRESHOLD) {
        nextDirection = MoveDirection::RIGHT;
    }

    if (nextDirection != moveDirection) {
        lastError = filteredDiff;
        errorIntegral = 0;
    }

    moveDirection = nextDirection;
}

void TrackerStateHandler::updateServoTargetPwm() {
    if (moveDirection == MoveDirection::STOP) {
        targetPwm = TRACKER_SERVO_STOP_US;
        return;
    }

    if (!moveTimer.hasTicked())
        return;

    const uint16_t offset = speedOffset();

    if (moveDirection == MoveDirection::LEFT) {
        targetPwm = TRACKER_SERVO_STOP_US + offset;
        if (targetPwm < TRACKER_SERVO_LEFT_MIN_US)
            targetPwm = TRACKER_SERVO_LEFT_MIN_US;
        if (targetPwm > TRACKER_SERVO_LEFT_MAX_US)
            targetPwm = TRACKER_SERVO_LEFT_MAX_US;
    } else {
        targetPwm = TRACKER_SERVO_STOP_US - offset;
        if (targetPwm > TRACKER_SERVO_RIGHT_MIN_US)
            targetPwm = TRACKER_SERVO_RIGHT_MIN_US;
        if (targetPwm < TRACKER_SERVO_RIGHT_MAX_US)
            targetPwm = TRACKER_SERVO_RIGHT_MAX_US;
    }

    moveTimer.reset();
}

void TrackerStateHandler::smoothServo() {
    if (servoPwm == targetPwm || !servoTimer.hasTicked())
        return;

    if (servoPwm < targetPwm) {
        servoPwm =
            servoPwm + TRACKER_SERVO_SMOOTH_STEP_US < targetPwm ?
            servoPwm + TRACKER_SERVO_SMOOTH_STEP_US :
            targetPwm;
    } else {
        servoPwm =
            servoPwm > targetPwm + TRACKER_SERVO_SMOOTH_STEP_US ?
            servoPwm - TRACKER_SERVO_SMOOTH_STEP_US :
            targetPwm;
    }

    panServo.writeMicroseconds(servoPwm);
    servoTimer.reset();
}

uint16_t TrackerStateHandler::speedOffset() {
    const int16_t error = abs(filteredDiff);
    const int16_t derivative = error - abs(lastError);

    errorIntegral += error;
    if (errorIntegral > TRACKER_PID_INTEGRAL_MAX)
        errorIntegral = TRACKER_PID_INTEGRAL_MAX;

    int32_t pidOutput =
        ((int32_t)TRACKER_PID_KP * error) +
        ((int32_t)TRACKER_PID_KI * errorIntegral) +
        ((int32_t)TRACKER_PID_KD * derivative);

    lastError = filteredDiff;

    if (pidOutput < TRACKER_PID_SCALE)
        pidOutput = TRACKER_PID_SCALE;

    uint16_t offset = pidOutput / TRACKER_PID_SCALE;
    if (offset < 1)
        offset = 1;
    if (offset > TRACKER_MAX_SPEED_OFFSET_US)
        offset = TRACKER_MAX_SPEED_OFFSET_US;

    return offset;
}

void TrackerStateHandler::onInitialDraw() {
    Ui::clear();
    onUpdateDraw();
}

void TrackerStateHandler::onUpdateDraw() {
    Ui::clear();

#ifdef USE_DIVERSITY
    drawRssiText();
    drawWinnerText();
    drawDirectionGraphic();
#else
    Ui::display.setTextSize(1);
    Ui::display.setTextColor(WHITE);
    Ui::display.setCursor(0, 0);
    Ui::display.print(PSTR2("Tracker needs"));
    Ui::display.setCursor(0, 10);
    Ui::display.print(PSTR2("USE_DIVERSITY"));
#endif

    Ui::needDisplay();
}

void TrackerStateHandler::drawRssiText() {
#ifdef USE_DIVERSITY
    Ui::display.setTextSize(1);
    Ui::display.setTextColor(WHITE);
    Ui::display.setCursor(0, 0);
    Ui::display.print(PSTR2("A:"));
    Ui::display.print(Receiver::rssiA);
    Ui::display.print(PSTR2("/"));
    Ui::display.print(Receiver::rssiARaw);

    Ui::display.setCursor(64, 0);
    Ui::display.print(PSTR2("B:"));
    Ui::display.print(Receiver::rssiB);
    Ui::display.print(PSTR2("/"));
    Ui::display.print(Receiver::rssiBRaw);
#endif
}

void TrackerStateHandler::drawWinnerText() {
    Ui::display.setCursor(0, 10);

    if (filteredDiff > TRACKER_RSSI_STOP_THRESHOLD) {
        Ui::display.print(PSTR2("A LEFT"));
    } else if (filteredDiff < -TRACKER_RSSI_STOP_THRESHOLD) {
        Ui::display.print(PSTR2("B RIGHT"));
    } else {
        Ui::display.print(PSTR2("BALANCED"));
    }

    Ui::display.setCursor(76, 10);
    Ui::display.print(Channels::getName(Receiver::activeChannel));
}

void TrackerStateHandler::drawDirectionGraphic() {
    const uint8_t y = 24;
    const uint8_t centerX = SCREEN_WIDTH_MID;

    Ui::display.setCursor(0, 21);
    Ui::display.print(PSTR2("PWM:"));
    Ui::display.print(servoPwm);

    Ui::display.drawFastHLine(46, y, SCREEN_WIDTH - 58, WHITE);
    Ui::display.drawFastVLine(centerX, y - 4, 9, WHITE);

    if (moveDirection == MoveDirection::LEFT) {
        Ui::display.fillTriangle(48, y, 56, y - 4, 56, y + 4, WHITE);
        Ui::display.drawFastHLine(56, y, centerX - 56, WHITE);
    } else if (moveDirection == MoveDirection::RIGHT) {
        Ui::display.fillTriangle(
            SCREEN_WIDTH - 12,
            y,
            SCREEN_WIDTH - 22,
            y - 4,
            SCREEN_WIDTH - 22,
            y + 4,
            WHITE
        );
        Ui::display.drawFastHLine(
            centerX + 1,
            y,
            SCREEN_WIDTH - centerX - 23,
            WHITE
        );
    } else {
        Ui::display.fillRect(centerX - 2, y - 2, 5, 5, WHITE);
    }
}

void TrackerStateHandler::onButtonChange(
    Button button,
    Buttons::PressType pressType
) {
    if (button == Button::SAVE && pressType == Buttons::PressType::SHORT) {
        servoPwm = TRACKER_SERVO_STOP_US;
        targetPwm = TRACKER_SERVO_STOP_US;
        panServo.writeMicroseconds(servoPwm);
        moveDirection = MoveDirection::STOP;
        filteredDiff = 0;
        lastError = 0;
        errorIntegral = 0;
        Ui::needUpdate();
    }
}
