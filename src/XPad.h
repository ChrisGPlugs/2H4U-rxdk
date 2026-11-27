//Xbox Joy Deadzone
#ifndef __XPAD_H__
#define __XPAD_H__

#include <SDL3/SDL.h>

#define JOY_DEADZONE 4500
// Analog triggers (0..32767) count as pressed above this.
#define JOY_TRIGGER_THRESHOLD 8000

////////////////////
//Our Xbox buttons//
////////////////////
enum
{
    JOY_BTTN_A = 0,
    JOY_BTTN_B,
    JOY_BTTN_X,
    JOY_BTTN_Y,
	JOY_BTTN_BLACK,
    JOY_BTTN_WHITE,
	JOY_BTTN_LTRIG,
    JOY_BTTN_RTRIG,
    JOY_BTTN_START,
    JOY_BTTN_BACK,
	JOY_BTTN_LSTICK,
    JOY_BTTN_RSTICK,
	JOY_BTTN_TOTAL
};

//////////////////////
//Our Xbox joystick //
//////////////////////
enum
{
    JOY_AXIS_LX = 0,
    JOY_AXIS_LY,
    JOY_AXIS_RX,
    JOY_AXIS_RY,
    JOY_AXIS_TOTAL
};
#endif