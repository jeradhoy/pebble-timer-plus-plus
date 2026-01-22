//! @file main.c
//! @brief Main logic for Timer++
//!
//! Contains the higher level logic code
//!
//! @author Eric D. Phillips
//! @date August 27, 2015
//! @bugs No known bugs

#pragma once
#include <pebble.h>

#define BUTTON_HOLD_RESET_MS 750

// Current control mode
typedef enum {
  ControlModeNew,
  ControlModeEditHr,
  ControlModeEditMin,
  ControlModeEditSec,
  ControlModeCounting,
  ControlModeEditRepeat,
  // Setup menu modes
  ControlModeSetupMenu,         // In setup mode, scrolling between options
  ControlModeSlotSelect,        // Selecting stored timer slot
  ControlModePomodoroMenu,      // Pomodoro work/break selection
  ControlModePomodoroEditWork,  // Editing pomodoro work time
  ControlModePomodoroEditBreak  // Editing pomodoro break time
} ControlMode;

//! Get the current control mode of the app
//! @return The current ControlMode
ControlMode main_get_control_mode(void);

//! Get the current setup menu selection index
//! 0 = Reset, 1-N = saved timers, N+1 = Add New
int8_t main_get_setup_selection(void);
