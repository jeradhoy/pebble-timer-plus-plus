// @file timer.c
// @brief Data and controls for timer
//
// Contains data and all functions for setting and accessing
// a timer. Also saves and loads timers between closing and reopening.
//
// @author Eric D. Phillips
// @data October 26, 2015
// @bugs No known bugs

#include "timer.h"
#include "utility.h"
#include <stdlib.h>
#include <string.h>

#define PERSIST_VERSION 4
#define PERSIST_VERSION_KEY 4342896
#define PERSIST_TIMER_KEY_V2_DATA 58734
#define PERSIST_TIMER_KEY 58736
#define PERSIST_SLOTS_KEY 58740
#define PERSIST_POMODORO_KEY 58741
#define PERSIST_POMODORO_SLOTS_KEY 58742
#define VIBRATION_LENGTH_MS 30000
// legacy persistent storage
#define PERSIST_TIMER_KEY_V1_LEGACY 3456

// Vibration sequence
static const uint32_t vibe_sequence[] = {150, 200, 300};
static const VibePattern vibe_pattern = {
  .durations = vibe_sequence,
  .num_segments = ARRAY_LENGTH(vibe_sequence),
};

// Main data structure
Timer timer_data;

// Timer slots data
TimerSlotsData timer_slots_data;

// Pomodoro slots data
PomodoroSlotsData pomodoro_slots_data;

// Pomodoro data
PomodoroData pomodoro_data;


////////////////////////////////////////////////////////////////////////////////////////////////////
// API Functions
//

// Get timer value divided into time parts
void timer_get_time_parts(uint16_t *hr, uint16_t *min, uint16_t *sec) {
  int64_t value = timer_get_value_ms();
  (*hr) = value / MSEC_IN_HR;
  (*min) = value % MSEC_IN_HR / MSEC_IN_MIN;
  (*sec) = value % MSEC_IN_MIN / MSEC_IN_SEC;
}

// Get the timer time in milliseconds assuming the following conditions
// 1. when the timer is running, start_ms represents the epoch when it was started
// 2. when it is paused, start_ms represents the negative of the time is has been running
int64_t timer_get_value_ms(void) {
  int64_t value = timer_data.length_ms - epoch() +
    (((timer_data.start_ms + epoch() - 1) % epoch()) + 1);
  if (value < 0) {
    return -value;
  }
  return value;
}

// Get the total timer time in milliseconds
int64_t timer_get_length_ms(void) {
  return timer_data.length_ms;
}

// Check if the timer is vibrating
bool timer_is_vibrating(void) {
  return timer_is_chrono() && !timer_is_paused() && timer_data.can_vibrate;
}

// Check if timer is in stopwatch mode
bool timer_is_chrono(void) {
  // see timer_get_timer_parts for explanation of equation
  return timer_data.length_ms - (int64_t)epoch() +
    ((timer_data.start_ms + (int64_t)epoch() - 1) % (int64_t)epoch() + 1) <= 0;
}

// Check if timer or stopwatch is paused
bool timer_is_paused(void) {
  return timer_data.start_ms <= 0;
}

// Three long vibrations pattern (used for pomodoro and repeat timer)
static const uint32_t triple_long_vibe[] = {400, 200, 400, 200, 400};
static const VibePattern triple_long_pattern = {
  .durations = triple_long_vibe,
  .num_segments = 5,
};

// Check if the timer is elapsed and vibrate if this is the first call after elapsing
void timer_check_elapsed(void) {
  if (timer_is_chrono() && !timer_is_paused() && timer_data.can_vibrate) {
    // Check for pomodoro mode first
    if (pomodoro_data.is_active) {
      pomodoro_handle_timer_complete();
      vibes_enqueue_custom_pattern(triple_long_pattern);
      return;
    }
    // Check for repeat timer setting (non-pomodoro)
    if (timer_data.repeat_enabled && timer_data.base_length_ms > 0) {
      vibes_enqueue_custom_pattern(triple_long_pattern);
      // Restart timer immediately
      timer_data.length_ms = timer_data.base_length_ms;
      timer_data.start_ms = epoch();
      timer_data.can_vibrate = true;
      timer_data.auto_snooze_count = 0;
      return;
    }
    if (timer_data.is_repeating && timer_data.repeat_count > 1) {
      timer_data.repeat_count--;
      timer_increment(timer_data.base_length_ms);
      vibes_long_pulse(); // Vibrate briefly on repeat
      return;
    }
    // stop vibration after certain duration
    if (timer_get_value_ms() > VIBRATION_LENGTH_MS) {
      timer_data.can_vibrate = false;
      if (timer_data.auto_snooze_count < 5) {
        timer_data.auto_snooze_count++;
        timer_increment(SNOOZE_INCREMENT_MS);
      }
    } else {
      // vibrate
      vibes_enqueue_custom_pattern(vibe_pattern);
    }
  }
}

// Increment timer value currently being edited
void timer_increment(int64_t increment) {
  // if in paused stopwatch mode, rewind to previous time
  if (timer_is_chrono() && timer_data.start_ms) {
    timer_rewind();
    return;
  }
  // identify increment class
  int64_t interval;
  if (llabs(increment) < MSEC_IN_MIN) {
    interval = MSEC_IN_MIN;
  } else if (llabs(increment) < MSEC_IN_HR) {
    interval = MSEC_IN_HR;
  } else {
    interval = MSEC_IN_HR * 100;
  }
  // calculate new time by incrementing with wrapping
  int64_t ls_bit = (timer_data.length_ms + timer_data.start_ms) % interval;
  int64_t step = (ls_bit + interval + increment) % interval - ls_bit;
  if (timer_data.start_ms) {
    timer_data.start_ms += step;
    if (timer_data.start_ms > 0) {
      timer_data.length_ms += timer_data.start_ms;
      timer_data.start_ms = 0;
    }
  } else {
    timer_data.length_ms += step;
  }
  // if at zero, remove any leftover milliseconds
  if (timer_get_value_ms() < MSEC_IN_SEC) {
    timer_reset();
  }
  // enable vibration
  if (timer_data.length_ms) {
    timer_data.can_vibrate = true;
  }
}

// Increment stopwatch (chrono) value currently being edited by adjusting start time
void timer_increment_chrono(int64_t increment) {
  // adjust start time to effectively add time to the stopwatch
  timer_data.start_ms -= increment;
}

// Toggle play pause state for timer
void timer_toggle_play_pause(void) {
  if (timer_data.start_ms > 0) {
    timer_data.start_ms -= epoch();
  } else {
    timer_data.start_ms += epoch();
  }
}

//! Rewind the timer back to its original value
void timer_rewind(void) {
  timer_data.start_ms = 0;  // this also pauses the timer
  // enable vibration
  if (timer_data.length_ms) {
    timer_data.can_vibrate = true;
  }
}

// Restart the timer from its original value
void timer_restart(void) {
  if (timer_data.base_length_ms > 0) {
      // Countdown: restore to base length
      timer_data.length_ms = timer_data.base_length_ms;
  } else {
      // Chrono: reset to 0
      timer_data.length_ms = 0;
  }

  if (timer_is_paused()) {
      // If paused, remain paused (start_ms <= 0 means paused)
      timer_data.start_ms = 0;
  } else {
      // If running, restart running from now
      timer_data.start_ms = epoch();
  }

  // enable vibration if length > 0
  if (timer_data.length_ms > 0) {
    timer_data.can_vibrate = true;
  } else {
    timer_data.can_vibrate = false;
  }

  timer_data.auto_snooze_count = 0;
}

// Reset the timer to zero
void timer_reset(void) {
  timer_data.length_ms = 0;
  timer_data.base_length_ms = 0;
  timer_data.start_ms = 0;  // Paused state
  // disable vibration
  timer_data.can_vibrate = false;
  timer_data.auto_snooze_count = 0;
  timer_data.is_repeating = false;
  timer_data.repeat_count = 0;
}

// Save the timer to persistent storage
void timer_persist_store(void) {
// write out current persistent data version
  persist_write_int(PERSIST_VERSION_KEY, PERSIST_VERSION);
  // Always write to the new V3 key
  persist_write_data(PERSIST_TIMER_KEY, &timer_data, sizeof(timer_data));
  // Save slots and pomodoro data
  persist_write_data(PERSIST_SLOTS_KEY, &timer_slots_data, sizeof(timer_slots_data));
  persist_write_data(PERSIST_POMODORO_KEY, &pomodoro_data, sizeof(pomodoro_data));
  persist_write_data(PERSIST_POMODORO_SLOTS_KEY, &pomodoro_slots_data, sizeof(pomodoro_slots_data));

  // Clean up old keys if they still exist
  if (persist_exists(PERSIST_TIMER_KEY_V2_DATA)) {
    persist_delete(PERSIST_TIMER_KEY_V2_DATA);
  }
  if (persist_exists(PERSIST_TIMER_KEY_V1_LEGACY)) {
    persist_delete(PERSIST_TIMER_KEY_V1_LEGACY);
  }
}

// Reset the auto snooze count
void timer_reset_auto_snooze(void) {
  timer_data.auto_snooze_count = 0;
}

// Read the timer from persistent storage
void timer_persist_read(void) {
  // --- Destructive Update Logic ---
  // 1. If the old V2 key exists, delete it.
  if (persist_exists(PERSIST_TIMER_KEY_V2_DATA)) {
    persist_delete(PERSIST_TIMER_KEY_V2_DATA);
  }
  // 2. If the old V1 key exists, delete it.
  if (persist_exists(PERSIST_TIMER_KEY_V1_LEGACY)) {
    persist_delete(PERSIST_TIMER_KEY_V1_LEGACY);
  }

  // --- Now, just try to load the new (V3) data ---
  if (persist_exists(PERSIST_TIMER_KEY)) {
    // User has run the new app before, load their new V3 data
    persist_read_data(PERSIST_TIMER_KEY, &timer_data, sizeof(timer_data));
  } else {
    // First time running V3 (or no data saved), reset to default
    timer_reset();
  }

  // Load timer slots
  if (persist_exists(PERSIST_SLOTS_KEY)) {
    persist_read_data(PERSIST_SLOTS_KEY, &timer_slots_data, sizeof(timer_slots_data));
  } else {
    memset(&timer_slots_data, 0, sizeof(timer_slots_data));
  }

  // Load pomodoro settings
  if (persist_exists(PERSIST_POMODORO_KEY)) {
    persist_read_data(PERSIST_POMODORO_KEY, &pomodoro_data, sizeof(pomodoro_data));
  } else {
    pomodoro_data.work_time_ms = POMODORO_DEFAULT_WORK_MS;
    pomodoro_data.break_time_ms = POMODORO_DEFAULT_BREAK_MS;
    pomodoro_data.is_work_phase = true;
    pomodoro_data.completed_cycles = 0;
    pomodoro_data.is_active = false;
  }

  // Load pomodoro slots
  if (persist_exists(PERSIST_POMODORO_SLOTS_KEY)) {
    persist_read_data(PERSIST_POMODORO_SLOTS_KEY, &pomodoro_slots_data, sizeof(pomodoro_slots_data));
  } else {
    memset(&pomodoro_slots_data, 0, sizeof(pomodoro_slots_data));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer Slots Functions
//

// Save current timer to a slot
void timer_slots_save_current(uint8_t slot_index) {
  if (slot_index < TIMER_SLOT_COUNT) {
    timer_slots_data.slots[slot_index].length_ms = timer_data.length_ms;
    persist_write_data(PERSIST_SLOTS_KEY, &timer_slots_data, sizeof(timer_slots_data));
  }
}

// Load a timer from a slot
void timer_slots_load(uint8_t slot_index) {
  if (slot_index < TIMER_SLOT_COUNT && timer_slots_data.slots[slot_index].length_ms > 0) {
    timer_data.length_ms = timer_slots_data.slots[slot_index].length_ms;
    timer_data.base_length_ms = timer_slots_data.slots[slot_index].length_ms;
    timer_data.start_ms = 0;  // Paused state
    timer_data.can_vibrate = true;
    timer_data.auto_snooze_count = 0;
  }
}

// Delete a timer from a slot
void timer_slots_delete(uint8_t slot_index) {
  if (slot_index < TIMER_SLOT_COUNT) {
    timer_slots_data.slots[slot_index].length_ms = 0;
    persist_write_data(PERSIST_SLOTS_KEY, &timer_slots_data, sizeof(timer_slots_data));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Pomodoro Functions
//

// Start pomodoro mode
void pomodoro_start(void) {
  pomodoro_data.is_active = true;
  pomodoro_data.is_work_phase = true;
  pomodoro_data.completed_cycles = 0;

  // Set timer to work duration
  timer_data.length_ms = pomodoro_data.work_time_ms;
  timer_data.base_length_ms = pomodoro_data.work_time_ms;
  timer_data.start_ms = epoch();
  timer_data.can_vibrate = true;
  timer_data.auto_snooze_count = 0;

  // Save pomodoro settings
  persist_write_data(PERSIST_POMODORO_KEY, &pomodoro_data, sizeof(pomodoro_data));
}

// Stop pomodoro mode
void pomodoro_stop(void) {
  pomodoro_data.is_active = false;
  persist_write_data(PERSIST_POMODORO_KEY, &pomodoro_data, sizeof(pomodoro_data));
}

// Handle timer completion in pomodoro mode (switch phases)
void pomodoro_handle_timer_complete(void) {
  if (!pomodoro_data.is_active) return;

  if (pomodoro_data.is_work_phase) {
    // Work phase complete, start break
    pomodoro_data.is_work_phase = false;
    pomodoro_data.completed_cycles++;

    timer_data.length_ms = pomodoro_data.break_time_ms;
    timer_data.base_length_ms = pomodoro_data.break_time_ms;
  } else {
    // Break phase complete, start work
    pomodoro_data.is_work_phase = true;

    timer_data.length_ms = pomodoro_data.work_time_ms;
    timer_data.base_length_ms = pomodoro_data.work_time_ms;
  }

  // Restart timer
  timer_data.start_ms = epoch();
  timer_data.can_vibrate = true;
  timer_data.auto_snooze_count = 0;
}

// Skip to next pomodoro phase
void pomodoro_skip_to_next(void) {
  if (!pomodoro_data.is_active) return;

  if (pomodoro_data.is_work_phase) {
    // Skip to break
    pomodoro_data.is_work_phase = false;
    pomodoro_data.completed_cycles++;
    timer_data.length_ms = pomodoro_data.break_time_ms;
    timer_data.base_length_ms = pomodoro_data.break_time_ms;
  } else {
    // Skip to work
    pomodoro_data.is_work_phase = true;
    timer_data.length_ms = pomodoro_data.work_time_ms;
    timer_data.base_length_ms = pomodoro_data.work_time_ms;
  }

  // Start the new phase
  timer_data.start_ms = epoch();
  timer_data.can_vibrate = true;
  timer_data.auto_snooze_count = 0;
}

// Skip to previous pomodoro phase
void pomodoro_skip_to_previous(void) {
  if (!pomodoro_data.is_active) return;

  if (pomodoro_data.is_work_phase) {
    // Go back to break (decrement cycle count if > 0)
    pomodoro_data.is_work_phase = false;
    if (pomodoro_data.completed_cycles > 0) {
      pomodoro_data.completed_cycles--;
    }
    timer_data.length_ms = pomodoro_data.break_time_ms;
    timer_data.base_length_ms = pomodoro_data.break_time_ms;
  } else {
    // Go back to work
    pomodoro_data.is_work_phase = true;
    timer_data.length_ms = pomodoro_data.work_time_ms;
    timer_data.base_length_ms = pomodoro_data.work_time_ms;
  }

  // Start the phase
  timer_data.start_ms = epoch();
  timer_data.can_vibrate = true;
  timer_data.auto_snooze_count = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Pomodoro Slots Functions
//

// Save pomodoro to a slot
void pomodoro_slots_save(uint8_t slot_index, int64_t work_ms, int64_t break_ms) {
  if (slot_index < POMODORO_SLOT_COUNT) {
    pomodoro_slots_data.slots[slot_index].work_time_ms = work_ms;
    pomodoro_slots_data.slots[slot_index].break_time_ms = break_ms;
    persist_write_data(PERSIST_POMODORO_SLOTS_KEY, &pomodoro_slots_data, sizeof(pomodoro_slots_data));
  }
}

// Load and start a pomodoro from a slot
void pomodoro_slots_load(uint8_t slot_index) {
  if (slot_index < POMODORO_SLOT_COUNT &&
      pomodoro_slots_data.slots[slot_index].work_time_ms > 0) {
    // Set pomodoro data from slot
    pomodoro_data.work_time_ms = pomodoro_slots_data.slots[slot_index].work_time_ms;
    pomodoro_data.break_time_ms = pomodoro_slots_data.slots[slot_index].break_time_ms;
    pomodoro_data.is_active = true;
    pomodoro_data.is_work_phase = true;
    pomodoro_data.completed_cycles = 0;

    // Set timer to work duration but leave paused (start_ms = 0)
    timer_data.length_ms = pomodoro_data.work_time_ms;
    timer_data.base_length_ms = pomodoro_data.work_time_ms;
    timer_data.start_ms = 0;  // Paused - user will start manually
    timer_data.can_vibrate = true;
    timer_data.auto_snooze_count = 0;

    // Save pomodoro settings
    persist_write_data(PERSIST_POMODORO_KEY, &pomodoro_data, sizeof(pomodoro_data));
  }
}

// Delete a pomodoro from a slot
void pomodoro_slots_delete(uint8_t slot_index) {
  if (slot_index < POMODORO_SLOT_COUNT) {
    pomodoro_slots_data.slots[slot_index].work_time_ms = 0;
    pomodoro_slots_data.slots[slot_index].break_time_ms = 0;
    persist_write_data(PERSIST_POMODORO_SLOTS_KEY, &pomodoro_slots_data, sizeof(pomodoro_slots_data));
  }
}

// Count filled pomodoro slots
uint8_t pomodoro_slots_count_filled(void) {
  uint8_t count = 0;
  for (int i = 0; i < POMODORO_SLOT_COUNT; i++) {
    if (pomodoro_slots_data.slots[i].work_time_ms > 0) {
      count++;
    }
  }
  return count;
}

// Get slot index for selection (skipping empty slots)
int8_t pomodoro_slots_get_index_for_selection(uint8_t selection) {
  uint8_t count = 0;
  for (int i = 0; i < POMODORO_SLOT_COUNT; i++) {
    if (pomodoro_slots_data.slots[i].work_time_ms > 0) {
      if (count == selection) {
        return i;
      }
      count++;
    }
  }
  return -1;
}

// Get first empty pomodoro slot
int8_t pomodoro_slots_get_first_empty(void) {
  for (int i = 0; i < POMODORO_SLOT_COUNT; i++) {
    if (pomodoro_slots_data.slots[i].work_time_ms == 0) {
      return i;
    }
  }
  return -1;
}
