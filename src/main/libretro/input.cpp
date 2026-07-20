/***************************************************************************
    Libretro Based Input Handling.

    Populates keys array with user input.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <cstdlib> /* abs */
#include <string.h>
#include "input.hpp"

Input input;

Input::Input(void)
{
}

Input::~Input(void)
{
}

void Input::init(int pad_id, int* key_config, int* pad_config, int analog, int* axis, int* analog_settings)
{
    this->key_config  = key_config;
    this->pad_config  = pad_config;
    this->analog      = analog;
    this->axis        = axis;
    this->wheel_zone  = analog_settings[0];
    this->wheel_dead  = analog_settings[1];
    this->pedals_dead = analog_settings[2];
    this->gamepad     = analog;

    a_wheel = CENTRE;
}

void Input::close()
{
}

/* Detect whether a key press change has occurred */
bool Input::has_pressed(presses p)
{
    return keys[p] && !keys_old[p];
}

/* Detect whether key is still pressed */
bool Input::is_pressed(presses p)
{
    return keys[p];
}

/* Detect whether pressed and clear the press */
bool Input::is_pressed_clear(presses p)
{
    bool pressed = keys[p];
    keys[p] = false;
    return pressed;
}

/* Denote that a frame has been done by copying key presses into previous array */
void Input::frame_done()
{
    memcpy(&keys_old, &keys, sizeof(keys));
}

void Input::handle_key(const int key, const bool is_pressed)
{
    /* Redefinable Key Input */
    if (key == key_config[0])
        keys[UP] = is_pressed;

    else if (key == key_config[1])
        keys[DOWN] = is_pressed;

    else if (key == key_config[2])
        keys[LEFT] = is_pressed;

    else if (key == key_config[3])
        keys[RIGHT] = is_pressed;

    if (key == key_config[4])
        keys[ACCEL] = is_pressed;

    if (key == key_config[5])
        keys[BRAKE] = is_pressed;

    if (key == key_config[6])
        keys[GEAR1] = is_pressed;

    if (key == key_config[7])
        keys[GEAR2] = is_pressed;

    if (key == key_config[8])
        keys[START] = is_pressed;

    if (key == key_config[9])
        keys[COIN] = is_pressed;

    if (key == key_config[10])
        keys[MENU] = is_pressed;

    if (key == key_config[11])
        keys[VIEWPOINT] = is_pressed;
}


void Input::handle_joy_axis(int wheel_axis, int accel_axis, int brake_axis)
{
   /* Analog Controls */

   /* Steering */
   /* OutRun requires values between 0x48 and 0xb8. */
   int percentage_adjust = ((wheel_zone) << 8) / 100;         
   int adjustedw = wheel_axis + ((wheel_axis * percentage_adjust) >> 8);

   /* Make 0 hard left, and 0x80 centre value. */
   adjustedw = ((adjustedw + (1 << 15)) >> 9);
   adjustedw += 0x40; /* Centre */

   if (adjustedw < 0x40)
       adjustedw = 0x40;
   else if (adjustedw > 0xC0)
       adjustedw = 0xC0;

   /* Remove Dead Zone */
   if (wheel_dead)
   {
       if (std::abs(CENTRE - adjustedw) <= wheel_dead)
           adjustedw = CENTRE;
   }
   a_wheel = adjustedw;

   /* Accelerator [Single Axis] */
   /* Scale input to be in the range of 0 to 0x7F */
   int adjusteda = accel_axis/256;          
   adjusteda += (adjusteda >> 2);
   a_accel = adjusteda;

   /* Brake [Single Axis] */
   /* Scale input to be in the range of 0 to 0x7F */
   int adjustedb = 0x7F - ((-brake_axis + (1 << 15)) >> 9);
   adjustedb += (adjustedb >> 2);
   a_brake = adjustedb;
}

void Input::handle_joy(const uint8_t button, const bool is_pressed)
{
    if (button == pad_config[0])
        keys[ACCEL] = is_pressed;

    if (button == pad_config[1])
        keys[BRAKE] = is_pressed;

    if (button == pad_config[2])
        keys[GEAR1] = is_pressed;

    if (button == pad_config[3])
        keys[GEAR2] = is_pressed;

    if (button == pad_config[4])
        keys[START] = is_pressed;

    if (button == pad_config[5])
        keys[COIN] = is_pressed;

    if (button == pad_config[6])
        keys[MENU] = is_pressed;

    if (button == pad_config[7])
        keys[VIEWPOINT] = is_pressed;
}
