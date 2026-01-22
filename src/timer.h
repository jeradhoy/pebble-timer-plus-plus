//! @file timer.h
//! @brief Data and controls for timer
//!
//! Contains data and all functions for setting and accessing
//! a timer. Also saves and loads timers between closing and reopening.
//!
//! @author Eric D. Phillips
//! @data October 26, 2015
//! @bugs No known bugs

#include <pebble.h>

#define SNOOZE_INCREMENT_MS (MSEC_IN_MIN * 5)

// Timer slots
#define TIMER_SLOT_COUNT 5

// Pomodoro slots
#define POMODORO_SLOT_COUNT 5

// Pomodoro defaults
#define POMODORO_DEFAULT_WORK_MS  (25 * MSEC_IN_MIN)
#define POMODORO_DEFAULT_BREAK_MS (5 * MSEC_IN_MIN)

// Main data structure
typedef struct {
  int64_t     length_ms;      //< Length of timer in milliseconds
  int64_t     start_ms;       //< The start epoch of the timer in milliseconds
  int64_t     base_length_ms; //< Used to track the original timer value (for later repeating).
  bool        elapsed;        //< Used to start the vibration if first time as elapsed
  bool        can_vibrate;    //< Flag used to tell when the timer has completed
  bool        reset_on_init;  //< Flag to indicate if the timer should be reset on next initialization
  int8_t      auto_snooze_count; //< Count of how many times the timer has auto-snoozed
  bool        is_repeating;   //< A flag to indicate that the timer is in repeat mode
  uint8_t     repeat_count;   //< A counter to track the number of times the timer has repeated
} Timer;
extern Timer timer_data;

// Stored timer slot
typedef struct {
  int64_t length_ms;    //< Timer duration in milliseconds
} TimerSlot;

// Timer slots data
typedef struct {
  TimerSlot slots[TIMER_SLOT_COUNT];
  uint8_t   selected_slot;  //< Currently highlighted slot (0-4)
} TimerSlotsData;
extern TimerSlotsData timer_slots_data;

// Stored pomodoro slot (work/break pair)
typedef struct {
  int64_t work_time_ms;   //< Work duration in milliseconds
  int64_t break_time_ms;  //< Break duration in milliseconds
} PomodoroSlot;

// Pomodoro slots data
typedef struct {
  PomodoroSlot slots[POMODORO_SLOT_COUNT];
} PomodoroSlotsData;
extern PomodoroSlotsData pomodoro_slots_data;

// Pomodoro timer configuration (runtime state)
typedef struct {
  int64_t work_time_ms;      //< Work duration (default: 25 min)
  int64_t break_time_ms;     //< Break duration (default: 5 min)
  bool    is_work_phase;     //< true = work, false = break
  uint8_t completed_cycles;  //< Number of completed work periods
  bool    is_active;         //< Whether pomodoro mode is active
} PomodoroData;
extern PomodoroData pomodoro_data;


//! Get timer value
//! @param hr A pointer to where to store the hour value of the timer
//! @param min A pointer to where to store the minute value of the timer
//! @param sec A pointer to where to store the second value of the timer
void timer_get_time_parts(uint16_t *hr, uint16_t *min, uint16_t *sec);

//! Get the timer time in milliseconds
//! @return The current value of the timer in milliseconds
int64_t timer_get_value_ms(void);

//! Get the total timer time in milliseconds
//! @return The total value of the timer in milliseconds
int64_t timer_get_length_ms(void);

//! Check if the timer is vibrating
//! @return True if the timer is currently vibrating
bool timer_is_vibrating(void);

//! Check if timer is in stopwatch mode
//! @return True if it is counting up as a stopwatch
bool timer_is_chrono(void);

//! Check if timer or stopwatch is paused
//! @return True if the timer is paused
bool timer_is_paused(void);

//! Check if the timer is elapsed and vibrate if this is the first call after elapsing
void timer_check_elapsed(void);

//! Increment timer value currently being edited
//! @param increment The amount to increment by
void timer_increment(int64_t increment);

//! Increment stopwatch (chrono) value currently being edited by adjusting start time
//! @param increment The amount to increment by
void timer_increment_chrono(int64_t increment);

//! Toggle play pause state for timer
void timer_toggle_play_pause(void);

//! Rewind the timer back to its original value
void timer_rewind(void);

//! Restart the timer from its original value
void timer_restart(void);

//! Reset the timer to zero
void timer_reset(void);

//! Save the timer to persistent storage
void timer_persist_store(void);

//! Read the timer from persistent storage
void timer_persist_read(void);

//! Reset the auto snooze count
void timer_reset_auto_snooze(void);

// Timer slots functions
//! Save current timer to a slot
void timer_slots_save_current(uint8_t slot_index);

//! Load a timer from a slot
void timer_slots_load(uint8_t slot_index);

//! Delete a timer from a slot
void timer_slots_delete(uint8_t slot_index);

// Pomodoro functions
//! Start pomodoro mode
void pomodoro_start(void);

//! Stop pomodoro mode
void pomodoro_stop(void);

//! Handle timer completion in pomodoro mode (switch phases)
void pomodoro_handle_timer_complete(void);

// Pomodoro slots functions
//! Save pomodoro to a slot
void pomodoro_slots_save(uint8_t slot_index, int64_t work_ms, int64_t break_ms);

//! Load and start a pomodoro from a slot
void pomodoro_slots_load(uint8_t slot_index);

//! Delete a pomodoro from a slot
void pomodoro_slots_delete(uint8_t slot_index);

//! Count filled pomodoro slots
uint8_t pomodoro_slots_count_filled(void);

//! Get slot index for selection (skipping empty slots)
int8_t pomodoro_slots_get_index_for_selection(uint8_t selection);

//! Get first empty pomodoro slot
int8_t pomodoro_slots_get_first_empty(void);
