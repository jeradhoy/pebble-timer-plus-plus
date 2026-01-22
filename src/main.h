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
  ControlModeSetupMenu,           // In setup mode, scrolling between options
  ControlModeSlotSelect,          // Selecting stored timer slot
  ControlModePomodoroMenu,        // Pomodoro work/break selection
  ControlModePomodoroEditWork,    // Editing pomodoro work time
  ControlModePomodoroEditBreak,   // Editing pomodoro break time
  // Pomodoro creation modes (using timer-style UI)
  ControlModePomodoroCreateWorkHr,   // Creating pomodoro - editing work hours
  ControlModePomodoroCreateWorkMin,  // Creating pomodoro - editing work minutes
  ControlModePomodoroCreateWorkSec,  // Creating pomodoro - editing work seconds
  ControlModePomodoroCreateBreakHr,  // Creating pomodoro - editing break hours
  ControlModePomodoroCreateBreakMin, // Creating pomodoro - editing break minutes
  ControlModePomodoroCreateBreakSec  // Creating pomodoro - editing break seconds
} ControlMode;

//! Get the current control mode of the app
//! @return The current ControlMode
ControlMode main_get_control_mode(void);

//! Get the current setup menu selection index
//! Negative = pomodoro section, 0 = Reset, positive = timers/add new
int8_t main_get_setup_selection(void);

//! Get the temp work time being edited for new pomodoro (in ms)
int64_t main_get_temp_pomodoro_work(void);

//! Get the temp break time being edited for new pomodoro (in ms)
int64_t main_get_temp_pomodoro_break(void);
