/***************************************************************************
    Libretro Based Input Handling.

    Populates keys array with user input.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once

#include <stdint.h>

class Input
{
public:
    enum presses
    {
        LEFT  = 0,
        RIGHT = 1,
        UP    = 2,
        DOWN  = 3,
        ACCEL = 4,
        BRAKE = 5,
        GEAR1 = 6,
        GEAR2 = 7,

        START = 8,
        COIN  = 9,
        VIEWPOINT = 10,
        
        PAUSE = 11,
        STEP  = 12,
        TIMER = 13,
        MENU  = 14
    };

    bool keys[15];
    bool keys_old[15];

    /* Has gamepad been found? */
    bool gamepad;

    /* Use analog controls */
    int analog;

    /* Latch last key press for redefines */
    int key_press;

    /* Latch last joystick button press for redefines */
    int16_t joy_button;

    /* Analog Controls */
    int a_wheel;
    int a_accel;
    int a_brake;

    Input(void);
    ~Input(void);

    void init(int, int*, int*, const int, int*, int*);
    void close();


    void handle_joy_axis(int, int, int);
    void frame_done();
    bool is_pressed(presses p);
    bool is_pressed_clear(presses p);
    bool has_pressed(presses p);
    void handle_key(const int, const bool);
    void handle_joy(const uint8_t, const bool);

private:
    static const int CENTRE = 0x80;

    /* Digital Dead Zone */
    static const int DIGITAL_DEAD = 3200;

    /* Configurations for keyboard and joypad */
    int* pad_config;
    int* key_config;
    int* axis;

    int wheel_zone;
    int wheel_dead;
    int pedals_dead;
};

extern Input input;
