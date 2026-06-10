#ifndef STATE_TRACKER_H
#define STATE_TRACKER_H

#include <stdint.h>
#include <Servo.h>

#include "settings.h"
#include "state.h"
#include "timer.h"


namespace StateMachine {
    class TrackerStateHandler : public StateMachine::StateHandler {
        private:
            enum class MoveDirection : int8_t {
                LEFT = -1,
                STOP = 0,
                RIGHT = 1
            };

            Servo panServo;
            Timer moveTimer = Timer(TRACKER_MOVE_INTERVAL);
            Timer servoTimer = Timer(TRACKER_SERVO_SMOOTH_INTERVAL);
            int16_t filteredDiff = 0;
            int16_t lastError = 0;
            int16_t errorIntegral = 0;
            uint16_t servoPwm = TRACKER_SERVO_STOP_US;
            uint16_t targetPwm = TRACKER_SERVO_STOP_US;
            MoveDirection moveDirection = MoveDirection::STOP;

            void updateDirection();
            void updateServoTargetPwm();
            void smoothServo();
            uint16_t speedOffset();

            void drawRssiText();
            void drawWinnerText();
            void drawDirectionGraphic();

        public:
            void onEnter();
            void onExit();
            void onUpdate();

            void onInitialDraw();
            void onUpdateDraw();
            void onButtonChange(Button button, Buttons::PressType pressType);
    };
}

#endif
