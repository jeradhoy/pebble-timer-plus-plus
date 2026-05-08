// @file main.c
// @brief Main logic for Timer++
//
// Contains the higher level logic code
//
// @author Eric D. Phillips
// @date August 27, 2015
// @bugs No known bugs

#include <pebble.h>
#include "main.h"
#include "drawing.h"
#include "timer.h"
#include "utility.h"
#include <stdlib.h>

// Main constants
#define BUTTON_HOLD_REPEAT_MS 100
#define LONG_PRESS_REPEAT_THRESHOLD 7  // Repeats before pomodoro phase skip (~700ms)

// Main data structure
static struct {
  Window      *window;        //< The base window for the application
  Layer       *layer;         //< The base layer on which everything will be drawn
  ControlMode control_mode;   //< The current control mode of the timer
  int8_t      setup_selection; //< Current selection in setup menu (negative=pomodoro, 0=Reset, positive=timers)
  AppTimer    *app_timer;     //< The AppTimer to keep the screen refreshing
  int64_t     temp_pomodoro_work;  //< Temporary work time during pomodoro creation
  int64_t     temp_pomodoro_break; //< Temporary break time during pomodoro creation
  uint8_t     up_repeat_count;    //< Count of up button repeats for long press detection
  uint8_t     down_repeat_count;  //< Count of down button repeats for long press detection
} main_data;

// Function declarations
static void prv_app_timer_callback(void *data);


////////////////////////////////////////////////////////////////////////////////////////////////////
// Private Functions
//

// Rewind timer if button is clicked to stop vibration
static bool main_timer_rewind(void) {
  // check if timer is vibrating
  if (timer_is_vibrating()) {
    vibes_cancel();
    main_data.control_mode = ControlModeEditSec;
    timer_rewind();
    drawing_update();
    return true;
  }
  return false;
}

// Helper to perform reset
static void prv_perform_reset(void) {
  timer_reset();
  pomodoro_stop();
  main_data.control_mode = ControlModeEditMin;
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Helper to count filled timer slots
static uint8_t prv_count_filled_slots(void) {
  uint8_t count = 0;
  for (int i = 0; i < TIMER_SLOT_COUNT; i++) {
    if (timer_slots_data.slots[i].length_ms > 0) {
      count++;
    }
  }
  return count;
}

// Helper to get actual slot index from selection index
static int8_t prv_get_slot_index_for_selection(uint8_t selection) {
  uint8_t count = 0;
  for (int i = 0; i < TIMER_SLOT_COUNT; i++) {
    if (timer_slots_data.slots[i].length_ms > 0) {
      if (count == selection) {
        return i;
      }
      count++;
    }
  }
  return -1;  // Selection is "+ Add New"
}

// Helper to get first empty slot index
static int8_t prv_get_first_empty_slot(void) {
  for (int i = 0; i < TIMER_SLOT_COUNT; i++) {
    if (timer_slots_data.slots[i].length_ms == 0) {
      return i;
    }
  }
  return -1;  // No empty slots
}

// Helper to check if in any pomodoro work creation mode
static bool prv_is_pomodoro_work_mode(void) {
  return main_data.control_mode == ControlModePomodoroCreateWorkHr ||
         main_data.control_mode == ControlModePomodoroCreateWorkMin ||
         main_data.control_mode == ControlModePomodoroCreateWorkSec;
}

// Helper to check if in any pomodoro break creation mode
static bool prv_is_pomodoro_break_mode(void) {
  return main_data.control_mode == ControlModePomodoroCreateBreakHr ||
         main_data.control_mode == ControlModePomodoroCreateBreakMin ||
         main_data.control_mode == ControlModePomodoroCreateBreakSec;
}

// Helper to check if in any pomodoro creation mode
static bool prv_is_pomodoro_create_mode(void) {
  return prv_is_pomodoro_work_mode() || prv_is_pomodoro_break_mode();
}

// Helper to get time parts from milliseconds
static void prv_get_time_parts_from_ms(int64_t ms, uint16_t *hr, uint16_t *min, uint16_t *sec) {
  *hr = ms / MSEC_IN_HR;
  *min = (ms % MSEC_IN_HR) / MSEC_IN_MIN;
  *sec = (ms % MSEC_IN_MIN) / MSEC_IN_SEC;
}

// Helper to increment pomodoro time with wrapping
static void prv_increment_pomodoro_time(int64_t *time_ms, int64_t increment) {
  // Determine interval for wrapping
  int64_t interval;
  if (llabs(increment) >= MSEC_IN_HR) {
    interval = 100 * MSEC_IN_HR;  // Max 99 hours
  } else if (llabs(increment) >= MSEC_IN_MIN) {
    interval = MSEC_IN_HR;
  } else {
    interval = MSEC_IN_MIN;
  }

  // Calculate current component value
  int64_t component = (*time_ms) % interval;
  int64_t rest = (*time_ms) - component;

  // Increment with wrapping
  component += increment;
  if (component < 0) {
    component += interval;
  } else if (component >= interval) {
    component -= interval;
  }

  *time_ms = rest + component;

  // Ensure minimum of 1 minute
  if (*time_ms < MSEC_IN_MIN) {
    *time_ms = MSEC_IN_MIN;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
//

// Get the current control mode of the timer
ControlMode main_get_control_mode(void) {
  return main_data.control_mode;
}

// Get the current setup menu selection
int8_t main_get_setup_selection(void) {
  return main_data.setup_selection;
}

// Get temp work time for pomodoro creation
int64_t main_get_temp_pomodoro_work(void) {
  return main_data.temp_pomodoro_work;
}

// Get temp break time for pomodoro creation
int64_t main_get_temp_pomodoro_break(void) {
  return main_data.temp_pomodoro_break;
}

// Background layer update procedure
static void prv_layer_update_proc_handler(Layer *layer, GContext *ctx) {
  // render the timer's visuals
  drawing_render(layer, ctx);
}

// Back click handler
static void prv_back_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // cancel vibrations
  vibes_cancel();

  // Handle pomodoro work creation modes - navigate back through fields or exit
  if (main_data.control_mode == ControlModePomodoroCreateWorkMin) {
    uint16_t hr, min, sec;
    prv_get_time_parts_from_ms(main_data.temp_pomodoro_work, &hr, &min, &sec);
    if (hr > 0) {
      main_data.control_mode = ControlModePomodoroCreateWorkHr;
    } else {
      main_data.control_mode = ControlModeSetupMenu;  // Exit to menu
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }
  if (main_data.control_mode == ControlModePomodoroCreateWorkHr) {
    main_data.control_mode = ControlModeSetupMenu;  // Exit to menu
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }
  if (main_data.control_mode == ControlModePomodoroCreateWorkSec) {
    main_data.control_mode = ControlModePomodoroCreateWorkMin;
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Handle pomodoro break creation modes - navigate back to work or through fields
  if (main_data.control_mode == ControlModePomodoroCreateBreakMin) {
    uint16_t hr, min, sec;
    prv_get_time_parts_from_ms(main_data.temp_pomodoro_break, &hr, &min, &sec);
    if (hr > 0) {
      main_data.control_mode = ControlModePomodoroCreateBreakHr;
    } else {
      // Go back to work time editing (at seconds)
      main_data.control_mode = ControlModePomodoroCreateWorkSec;
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }
  if (main_data.control_mode == ControlModePomodoroCreateBreakHr) {
    main_data.control_mode = ControlModePomodoroCreateWorkSec;  // Back to work
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }
  if (main_data.control_mode == ControlModePomodoroCreateBreakSec) {
    main_data.control_mode = ControlModePomodoroCreateBreakMin;
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Handle setup menu modes - exit back to edit mode
  if (main_data.control_mode == ControlModeSetupMenu ||
      main_data.control_mode == ControlModeSlotSelect ||
      main_data.control_mode == ControlModePomodoroMenu ||
      main_data.control_mode == ControlModePomodoroEditWork ||
      main_data.control_mode == ControlModePomodoroEditBreak) {
    main_data.control_mode = ControlModeEditMin;
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // get time parts
  uint16_t hr, min, sec;
  timer_get_time_parts(&hr, &min, &sec);
  // change control mode
  if ((hr && main_data.control_mode == ControlModeEditMin) ||
      main_data.control_mode == ControlModeEditSec) {
    main_data.control_mode--;
  } else {
    window_stack_pop(true);
  }
  // refresh
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Up click handler
static void prv_up_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // Track repeat count for long press detection
  if (click_recognizer_is_repeating(recognizer)) {
    main_data.up_repeat_count++;
  } else {
    main_data.up_repeat_count = 0;
  }

  // Check for pomodoro phase skip on long press
  if (main_data.up_repeat_count == LONG_PRESS_REPEAT_THRESHOLD &&
      pomodoro_data.is_active &&
      (main_data.control_mode == ControlModeCounting ||
       main_data.control_mode == ControlModeEditHr ||
       main_data.control_mode == ControlModeEditMin ||
       main_data.control_mode == ControlModeEditSec)) {
    pomodoro_skip_to_previous();
    // Keep paused if was paused
    if (main_data.control_mode != ControlModeCounting) {
      timer_data.start_ms = 0;
    }
    vibes_short_pulse();
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // After phase skip triggered, ignore further repeats
  if (main_data.up_repeat_count > LONG_PRESS_REPEAT_THRESHOLD && pomodoro_data.is_active) {
    return;
  }

  // Handle setup menu navigation
  // Menu structure: negative=pomodoro section, 0=Reset, positive=timer section, then Repeat toggle
  if (main_data.control_mode == ControlModeSetupMenu) {
    uint8_t filled_timers = prv_count_filled_slots();
    uint8_t filled_pomodoros = pomodoro_slots_count_filled();
    // Timer section max: filled timers + Add New (if room) + Repeat toggle
    int8_t max_timer_selection = filled_timers + ((filled_timers < TIMER_SLOT_COUNT) ? 1 : 0) + 1;
    // Pomodoro section min: -(filled pomodoros + Add New if room)
    int8_t min_pomodoro_selection = -(filled_pomodoros + ((filled_pomodoros < POMODORO_SLOT_COUNT) ? 1 : 0));

    if (main_data.setup_selection > min_pomodoro_selection) {
      main_data.setup_selection--;
    } else {
      main_data.setup_selection = max_timer_selection;  // Wrap to bottom of timer section
    }
    drawing_start_menu_scroll_animation(true);  // Scrolling up
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Handle pomodoro work time creation - increment based on current field
  if (prv_is_pomodoro_work_mode()) {
    int64_t increment;
    if (main_data.control_mode == ControlModePomodoroCreateWorkHr) {
      increment = MSEC_IN_HR;
    } else if (main_data.control_mode == ControlModePomodoroCreateWorkMin) {
      increment = MSEC_IN_MIN;
      // Check if we should switch to hour mode (wrapping from 59 to 00 adds an hour)
      uint16_t hr, min, sec;
      prv_get_time_parts_from_ms(main_data.temp_pomodoro_work, &hr, &min, &sec);
      if (min == 59 && hr == 0) {
        // Set to 1:00:XX (keep seconds, reset minutes, add hour)
        main_data.temp_pomodoro_work = MSEC_IN_HR + (sec * MSEC_IN_SEC);
        main_data.control_mode = ControlModePomodoroCreateWorkHr;
        drawing_update();
        layer_mark_dirty(main_data.layer);
        return;
      }
    } else {
      increment = MSEC_IN_SEC;
    }
    prv_increment_pomodoro_time(&main_data.temp_pomodoro_work, increment);
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Handle pomodoro break time creation - increment based on current field
  if (prv_is_pomodoro_break_mode()) {
    int64_t increment;
    if (main_data.control_mode == ControlModePomodoroCreateBreakHr) {
      increment = MSEC_IN_HR;
    } else if (main_data.control_mode == ControlModePomodoroCreateBreakMin) {
      increment = MSEC_IN_MIN;
      // Check if we should switch to hour mode
      uint16_t hr, min, sec;
      prv_get_time_parts_from_ms(main_data.temp_pomodoro_break, &hr, &min, &sec);
      if (min == 59 && hr == 0) {
        // Set to 1:00:XX (keep seconds, reset minutes, add hour)
        main_data.temp_pomodoro_break = MSEC_IN_HR + (sec * MSEC_IN_SEC);
        main_data.control_mode = ControlModePomodoroCreateBreakHr;
        drawing_update();
        layer_mark_dirty(main_data.layer);
        return;
      }
    } else {
      increment = MSEC_IN_SEC;
    }
    prv_increment_pomodoro_time(&main_data.temp_pomodoro_break, increment);
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Handle pomodoro menu - increase work/break time (legacy, kept for compatibility)
  if (main_data.control_mode == ControlModePomodoroMenu ||
      main_data.control_mode == ControlModePomodoroEditWork) {
    main_data.control_mode = ControlModePomodoroEditWork;
    pomodoro_data.work_time_ms += MSEC_IN_MIN;
    if (pomodoro_data.work_time_ms > 60 * MSEC_IN_MIN) {
      pomodoro_data.work_time_ms = MSEC_IN_MIN;  // Wrap around
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  if (main_data.control_mode == ControlModePomodoroEditBreak) {
    pomodoro_data.break_time_ms += MSEC_IN_MIN;
    if (pomodoro_data.break_time_ms > 30 * MSEC_IN_MIN) {
      pomodoro_data.break_time_ms = MSEC_IN_MIN;  // Wrap around
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // rewind timer if clicked while timer is going off
  if (main_timer_rewind() || main_data.control_mode == ControlModeCounting) {
    return;
  }
  // increment timer
  int64_t increment;
  if (main_data.control_mode == ControlModeEditHr) {
    increment = MSEC_IN_HR;
  } else if (main_data.control_mode == ControlModeEditMin) {
    increment = MSEC_IN_MIN;
  } else {
    increment = MSEC_IN_SEC;
  }
  // get starting time components
  uint16_t o_hr, o_min, o_sec;
  timer_get_time_parts(&o_hr, &o_min, &o_sec);
  // increment timer
  timer_increment(increment);
  // compare final time parts and switch into edit hr mode
  uint16_t n_hr, n_min, n_sec;
  timer_get_time_parts(&n_hr, &n_min, &n_sec);
  if (o_min > n_min && !o_hr) {
    timer_increment(MSEC_IN_HR);
    main_data.control_mode = ControlModeEditHr;
  }
  // check if switched out of ControlModeEditHr
  if (timer_get_value_ms() / MSEC_IN_HR == 0 && main_data.control_mode == ControlModeEditHr) {
    main_data.control_mode = ControlModeEditMin;
  }
  // animate and refresh
  if (!click_recognizer_is_repeating(recognizer)) {
    drawing_start_bounce_animation(true);
  }
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Select click handler
static void prv_select_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // Handle unified setup menu selection
  if (main_data.control_mode == ControlModeSetupMenu) {
    uint8_t filled_timers = prv_count_filled_slots();
    uint8_t filled_pomodoros = pomodoro_slots_count_filled();
    bool has_add_timer = (filled_timers < TIMER_SLOT_COUNT);
    int8_t repeat_toggle_index = filled_timers + (has_add_timer ? 1 : 0) + 1;

    if (main_data.setup_selection == 0) {
      // Reset selected
      prv_perform_reset();
      return;
    } else if (main_data.setup_selection == repeat_toggle_index) {
      // Repeat toggle selected - toggle the setting
      timer_data.repeat_enabled = !timer_data.repeat_enabled;
      vibes_short_pulse();  // Haptic feedback
      drawing_update();
      layer_mark_dirty(main_data.layer);
      return;
    } else if (main_data.setup_selection > 0) {
      // Timer section (positive indices)
      if (main_data.setup_selection <= filled_timers) {
        // A saved timer is selected - load it
        int8_t slot_index = prv_get_slot_index_for_selection(main_data.setup_selection - 1);
        if (slot_index >= 0) {
          timer_slots_load(slot_index);
        }
        main_data.control_mode = ControlModeEditMin;
      } else if (has_add_timer && main_data.setup_selection == filled_timers + 1) {
        // "+ Add New" timer selected - save current timer
        int8_t empty_slot = prv_get_first_empty_slot();
        if (empty_slot >= 0) {
          timer_slots_save_current(empty_slot);
        }
        main_data.control_mode = ControlModeEditMin;
      }
      drawing_update();
      layer_mark_dirty(main_data.layer);
      return;
    } else {
      // Pomodoro section (negative indices)
      // -1 to -N = saved pomodoros, -(N+1) = Add New (if room)
      int8_t pomo_index = -main_data.setup_selection - 1;  // -1 -> 0, -2 -> 1, etc.

      if (pomo_index < filled_pomodoros) {
        // A saved pomodoro is selected - load it paused
        int8_t slot_index = pomodoro_slots_get_index_for_selection(pomo_index);
        if (slot_index >= 0) {
          pomodoro_slots_load(slot_index);
          main_data.control_mode = ControlModeEditSec;  // Paused, user starts manually
        }
        drawing_update();
        layer_mark_dirty(main_data.layer);
        return;
      } else if (filled_pomodoros < POMODORO_SLOT_COUNT) {
        // "+ Add New" pomodoro selected - enter creation mode
        main_data.temp_pomodoro_work = POMODORO_DEFAULT_WORK_MS;
        main_data.temp_pomodoro_break = POMODORO_DEFAULT_BREAK_MS;
        main_data.control_mode = ControlModePomodoroCreateWorkMin;  // Start at minutes
        drawing_update();
        layer_mark_dirty(main_data.layer);
        return;
      }
    }
  }

  // Handle pomodoro work time creation - navigate through fields then to break
  if (main_data.control_mode == ControlModePomodoroCreateWorkHr) {
    main_data.control_mode = ControlModePomodoroCreateWorkMin;
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }
  if (main_data.control_mode == ControlModePomodoroCreateWorkMin) {
    main_data.control_mode = ControlModePomodoroCreateWorkSec;
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }
  if (main_data.control_mode == ControlModePomodoroCreateWorkSec) {
    // Move to break time - start at appropriate field based on break duration
    uint16_t hr, min, sec;
    prv_get_time_parts_from_ms(main_data.temp_pomodoro_break, &hr, &min, &sec);
    if (hr > 0) {
      main_data.control_mode = ControlModePomodoroCreateBreakHr;
    } else {
      main_data.control_mode = ControlModePomodoroCreateBreakMin;
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Handle pomodoro break time creation - navigate through fields then save
  if (main_data.control_mode == ControlModePomodoroCreateBreakHr) {
    main_data.control_mode = ControlModePomodoroCreateBreakMin;
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }
  if (main_data.control_mode == ControlModePomodoroCreateBreakMin) {
    main_data.control_mode = ControlModePomodoroCreateBreakSec;
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }
  if (main_data.control_mode == ControlModePomodoroCreateBreakSec) {
    // Save the new pomodoro
    int8_t empty_slot = pomodoro_slots_get_first_empty();
    if (empty_slot >= 0) {
      pomodoro_slots_save(empty_slot, main_data.temp_pomodoro_work, main_data.temp_pomodoro_break);
    }
    // Set and start the pomodoro
    pomodoro_data.work_time_ms = main_data.temp_pomodoro_work;
    pomodoro_data.break_time_ms = main_data.temp_pomodoro_break;
    pomodoro_start();
    main_data.control_mode = ControlModeCounting;
    if (!main_data.app_timer) {
      prv_app_timer_callback(NULL);
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Handle pomodoro menu - start pomodoro or switch editing (legacy)
  if (main_data.control_mode == ControlModePomodoroMenu) {
    pomodoro_start();
    main_data.control_mode = ControlModeCounting;
    if (!main_data.app_timer) {
      prv_app_timer_callback(NULL);
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  if (main_data.control_mode == ControlModePomodoroEditWork) {
    main_data.control_mode = ControlModePomodoroEditBreak;
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  if (main_data.control_mode == ControlModePomodoroEditBreak) {
    pomodoro_start();
    main_data.control_mode = ControlModeCounting;
    if (!main_data.app_timer) {
      prv_app_timer_callback(NULL);
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // rewind timer if clicked while timer is going off
  if (main_timer_rewind()) {
    return;
  }
  // change timer mode
  switch (main_data.control_mode) {
    case ControlModeNew:
    case ControlModeEditHr:
      main_data.control_mode = ControlModeEditMin;
      break;
    case ControlModeEditMin:
      main_data.control_mode = ControlModeEditSec;
      break;
    case ControlModeEditSec:
      main_data.control_mode = ControlModeCounting;
      // Save base_length_ms when starting timer
      timer_data.base_length_ms = timer_data.length_ms;
      timer_toggle_play_pause();
      if (!main_data.app_timer) {
        prv_app_timer_callback(NULL);
      }
      break;
    case ControlModeCounting:
      main_data.control_mode = ControlModeEditSec;
      timer_toggle_play_pause();
      break;
    default:
      break;
  }
  // refresh
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Select raw click handler
static void prv_select_raw_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // stop vibration
  vibes_cancel();
  // animate and refresh
  drawing_start_reset_animation();
  layer_mark_dirty(main_data.layer);
}

// Select long click handler
static void prv_select_long_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // If in pomodoro creation mode, cancel and go back to setup menu
  if (prv_is_pomodoro_create_mode()) {
    main_data.control_mode = ControlModeSetupMenu;
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // If in setup menu, handle deletion
  if (main_data.control_mode == ControlModeSetupMenu) {
    uint8_t filled_timers = prv_count_filled_slots();
    uint8_t filled_pomodoros = pomodoro_slots_count_filled();

    // Check if selection is a saved timer (positive, not Add New)
    if (main_data.setup_selection >= 1 && main_data.setup_selection <= filled_timers) {
      int8_t slot_index = prv_get_slot_index_for_selection(main_data.setup_selection - 1);
      if (slot_index >= 0) {
        timer_slots_delete(slot_index);
        // Adjust selection if we deleted the last item
        uint8_t new_filled = prv_count_filled_slots();
        int8_t max_selection = new_filled + ((new_filled < TIMER_SLOT_COUNT) ? 1 : 0);
        if (main_data.setup_selection > max_selection) {
          main_data.setup_selection = max_selection;
        }
        vibes_short_pulse();  // Feedback for deletion
      }
      drawing_update();
      layer_mark_dirty(main_data.layer);
      return;
    }

    // Check if selection is a saved pomodoro (negative, not Add New)
    // -1 to -N = saved pomodoros, -(N+1) = Add New
    if (main_data.setup_selection < 0) {
      int8_t pomo_index = -main_data.setup_selection - 1;  // -1 -> 0, -2 -> 1, etc.

      if (pomo_index < filled_pomodoros) {
        // This is a saved pomodoro - delete it
        int8_t slot_index = pomodoro_slots_get_index_for_selection(pomo_index);
        if (slot_index >= 0) {
          pomodoro_slots_delete(slot_index);
          // Adjust selection if needed
          uint8_t new_filled = pomodoro_slots_count_filled();
          int8_t min_selection = -(new_filled + ((new_filled < POMODORO_SLOT_COUNT) ? 1 : 0));
          if (main_data.setup_selection < min_selection) {
            main_data.setup_selection = min_selection;
          }
          vibes_short_pulse();  // Feedback for deletion
        }
        drawing_update();
        layer_mark_dirty(main_data.layer);
        return;
      }
    }
    return;
  }

  // If in counting mode (running), reset timer to original value but stay paused
  if (main_data.control_mode == ControlModeCounting) {
    if (pomodoro_data.is_active) {
      // In pomodoro mode - reset current phase timer but stay in pomodoro mode
      if (pomodoro_data.is_work_phase) {
        timer_data.length_ms = pomodoro_data.work_time_ms;
        timer_data.base_length_ms = pomodoro_data.work_time_ms;
      } else {
        timer_data.length_ms = pomodoro_data.break_time_ms;
        timer_data.base_length_ms = pomodoro_data.break_time_ms;
      }
      timer_data.start_ms = 0;  // Paused
      timer_data.can_vibrate = true;
      timer_data.auto_snooze_count = 0;
      main_data.control_mode = ControlModeEditSec;
    } else if (timer_data.base_length_ms > 0) {
      // Reset countdown to original value, paused
      timer_data.length_ms = timer_data.base_length_ms;
      timer_data.start_ms = 0;  // Paused
      timer_data.can_vibrate = true;
      timer_data.auto_snooze_count = 0;
      main_data.control_mode = ControlModeEditSec;
    } else {
      // Chrono mode - just pause and rewind
      timer_rewind();
      main_data.control_mode = ControlModeEditSec;
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Otherwise (editing/paused), enter setup menu
  main_data.control_mode = ControlModeSetupMenu;
  main_data.setup_selection = 0;  // Start at Reset
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Down click handler
static void prv_down_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  // Track repeat count for long press detection
  if (click_recognizer_is_repeating(recognizer)) {
    main_data.down_repeat_count++;
  } else {
    main_data.down_repeat_count = 0;
  }

  // Check for pomodoro phase skip on long press
  if (main_data.down_repeat_count == LONG_PRESS_REPEAT_THRESHOLD &&
      pomodoro_data.is_active &&
      (main_data.control_mode == ControlModeCounting ||
       main_data.control_mode == ControlModeEditHr ||
       main_data.control_mode == ControlModeEditMin ||
       main_data.control_mode == ControlModeEditSec)) {
    pomodoro_skip_to_next();
    // Keep paused if was paused
    if (main_data.control_mode != ControlModeCounting) {
      timer_data.start_ms = 0;
    }
    vibes_short_pulse();
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // After phase skip triggered, ignore further repeats
  if (main_data.down_repeat_count > LONG_PRESS_REPEAT_THRESHOLD && pomodoro_data.is_active) {
    return;
  }

  // Handle setup menu navigation
  // Menu structure: negative=pomodoro section, 0=Reset, positive=timer section, then Repeat toggle
  if (main_data.control_mode == ControlModeSetupMenu) {
    uint8_t filled_timers = prv_count_filled_slots();
    uint8_t filled_pomodoros = pomodoro_slots_count_filled();
    // Timer section max: filled timers + Add New (if room) + Repeat toggle
    int8_t max_timer_selection = filled_timers + ((filled_timers < TIMER_SLOT_COUNT) ? 1 : 0) + 1;
    // Pomodoro section min: -(filled pomodoros + Add New if room)
    int8_t min_pomodoro_selection = -(filled_pomodoros + ((filled_pomodoros < POMODORO_SLOT_COUNT) ? 1 : 0));

    if (main_data.setup_selection < max_timer_selection) {
      main_data.setup_selection++;
    } else {
      main_data.setup_selection = min_pomodoro_selection;  // Wrap to top of pomodoro section
    }
    drawing_start_menu_scroll_animation(false);  // Scrolling down
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Handle pomodoro work time creation - decrement based on current field
  if (prv_is_pomodoro_work_mode()) {
    int64_t increment;
    if (main_data.control_mode == ControlModePomodoroCreateWorkHr) {
      increment = -MSEC_IN_HR;
      // Check if we should exit hour mode
      uint16_t hr, min, sec;
      prv_get_time_parts_from_ms(main_data.temp_pomodoro_work, &hr, &min, &sec);
      if (hr == 1) {
        main_data.temp_pomodoro_work -= MSEC_IN_HR;
        main_data.control_mode = ControlModePomodoroCreateWorkMin;
        drawing_update();
        layer_mark_dirty(main_data.layer);
        return;
      }
    } else if (main_data.control_mode == ControlModePomodoroCreateWorkMin) {
      increment = -MSEC_IN_MIN;
    } else {
      increment = -MSEC_IN_SEC;
    }
    prv_increment_pomodoro_time(&main_data.temp_pomodoro_work, increment);
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Handle pomodoro break time creation - decrement based on current field
  if (prv_is_pomodoro_break_mode()) {
    int64_t increment;
    if (main_data.control_mode == ControlModePomodoroCreateBreakHr) {
      increment = -MSEC_IN_HR;
      // Check if we should exit hour mode
      uint16_t hr, min, sec;
      prv_get_time_parts_from_ms(main_data.temp_pomodoro_break, &hr, &min, &sec);
      if (hr == 1) {
        main_data.temp_pomodoro_break -= MSEC_IN_HR;
        main_data.control_mode = ControlModePomodoroCreateBreakMin;
        drawing_update();
        layer_mark_dirty(main_data.layer);
        return;
      }
    } else if (main_data.control_mode == ControlModePomodoroCreateBreakMin) {
      increment = -MSEC_IN_MIN;
    } else {
      increment = -MSEC_IN_SEC;
    }
    prv_increment_pomodoro_time(&main_data.temp_pomodoro_break, increment);
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // Handle pomodoro menu - decrease work/break time (legacy, kept for compatibility)
  if (main_data.control_mode == ControlModePomodoroMenu ||
      main_data.control_mode == ControlModePomodoroEditWork) {
    main_data.control_mode = ControlModePomodoroEditWork;
    pomodoro_data.work_time_ms -= MSEC_IN_MIN;
    if (pomodoro_data.work_time_ms < MSEC_IN_MIN) {
      pomodoro_data.work_time_ms = 60 * MSEC_IN_MIN;  // Wrap around
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  if (main_data.control_mode == ControlModePomodoroEditBreak) {
    pomodoro_data.break_time_ms -= MSEC_IN_MIN;
    if (pomodoro_data.break_time_ms < MSEC_IN_MIN) {
      pomodoro_data.break_time_ms = 30 * MSEC_IN_MIN;  // Wrap around
    }
    drawing_update();
    layer_mark_dirty(main_data.layer);
    return;
  }

  // rewind timer if clicked while timer is going off
  if (main_timer_rewind() || main_data.control_mode == ControlModeCounting) {
    return;
  }
  // increment timer
  int64_t increment;
  if (main_data.control_mode == ControlModeEditHr) {
    increment = -MSEC_IN_HR;
  } else if (main_data.control_mode == ControlModeEditMin) {
    increment = -MSEC_IN_MIN;
  } else {
    increment = -MSEC_IN_SEC;
  }
  timer_increment(increment);
  // check if switched out of ControlModeEditHr
  if (timer_get_value_ms() / MSEC_IN_HR == 0 && main_data.control_mode == ControlModeEditHr) {
    main_data.control_mode = ControlModeEditMin;
  }
  // animate and refresh
  if (!click_recognizer_is_repeating(recognizer)) {
    drawing_start_bounce_animation(false);
  }
  drawing_update();
  layer_mark_dirty(main_data.layer);
}

// Click configuration provider
static void prv_click_config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, BUTTON_HOLD_REPEAT_MS,
    prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_raw_click_subscribe(BUTTON_ID_SELECT, prv_select_raw_click_handler, NULL, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, BUTTON_HOLD_RESET_MS, prv_select_long_click_handler,
    NULL);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, BUTTON_HOLD_REPEAT_MS,
    prv_down_click_handler);
}

// AppTimer callback
static void prv_app_timer_callback(void *data) {
  // check if timer is complete
  timer_check_elapsed();
  // refresh
  drawing_update();
  layer_mark_dirty(main_data.layer);
  // schedule next call
  main_data.app_timer = NULL;
  if (main_data.control_mode == ControlModeCounting) {
    uint32_t duration = timer_get_value_ms() % MSEC_IN_SEC;
    if (timer_is_chrono()) {
      duration = MSEC_IN_SEC - duration;
    }
    main_data.app_timer = app_timer_register(duration + 5, prv_app_timer_callback, NULL);
  }
}

// TickTimerService callback
static void prv_tick_timer_service_callback(struct tm *tick_time, TimeUnits units_changed) {
  // refresh
  layer_mark_dirty(main_data.layer);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Loading and Unloading
//

// Initialize the program
static void prv_initialize(void) {
  // cancel any existing wakeup events
  wakeup_cancel_all();
  // load timer
  timer_persist_read();
  // set initial states
  if (timer_is_paused()) {
    // get time parts
    uint16_t hr, min, sec;
    timer_get_time_parts(&hr, &min, &sec);
    if (hr) {
      main_data.control_mode = ControlModeEditHr;
    } else {
      main_data.control_mode = ControlModeEditMin;
    }
  } else {
    main_data.control_mode = ControlModeCounting;
  }
  main_data.setup_selection = 0;

  // initialize window
  main_data.window = window_create();
  ASSERT(main_data.window);
  window_set_click_config_provider(main_data.window, prv_click_config_provider);
  Layer *window_root = window_get_root_layer(main_data.window);
  GRect window_bounds = layer_get_bounds(window_root);
#ifdef PBL_SDK_2
  window_set_fullscreen(main_data.window, true);
  window_bounds.size.h = 168;
#endif
  window_stack_push(main_data.window, true);
  // initialize main layer
  main_data.layer = layer_create(window_bounds);
  ASSERT(main_data.layer);
  layer_set_update_proc(main_data.layer, prv_layer_update_proc_handler);
  layer_add_child(window_root, main_data.layer);

  // initialize drawing singleton
  drawing_initialize(main_data.layer);
  // subscribe to tick timer service
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_timer_service_callback);
  // start refreshing
  prv_app_timer_callback(NULL);
}

// Terminate the program
static void prv_terminate(void) {
  // unsubscribe from timer service
  tick_timer_service_unsubscribe();
  // schedule wakeup
  if (!timer_is_chrono() && !timer_is_paused()) {
    time_t wakeup_time = (epoch() + timer_get_value_ms()) / MSEC_IN_SEC;
    wakeup_schedule(wakeup_time, 0, true);
  }
  // destroy
  timer_persist_store();
  drawing_terminate();
  layer_destroy(main_data.layer);
  window_destroy(main_data.window);
}

// Entry point
int main(void) {
  prv_initialize();
  app_event_loop();
  prv_terminate();
}
