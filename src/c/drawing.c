// @file drawing.h
// @brief Main drawing code
//
// Contains all the drawing code for this app.
//
// @author Eric D. Phillips
// @date August 29, 2015
// @bugs No known bugs

#include "animation.h"
#include "main.h"
#include "text_render.h"
#include "timer.h"
#include "utility.h"
#include <pebble-scalable/pebble-scalable.h>
#include <pebble.h>

// Drawing constants

// Note for future Eric: scalable works by dividing the absolute pixel
// value by the original Pebble (144x168) axis size you want to scale to.
// [value] / [width or height] * 1000. Your original CIRCLE_RADIUS value
// was 63 for a circle you want to fix inside the x axis, so 63 / 144 * 1000 = 438.

// Progress ring
#ifdef PBL_ROUND
// This is a lower value to simulate the original padding that the fixed 63px radius had
#define CIRCLE_RADIUS scl_y(355)
#else
#define CIRCLE_RADIUS scl_y(375)
#endif
#define ANGLE_CHANGE_ANI_THRESHOLD 348
#define PROGRESS_ANI_DURATION 250
#define MAIN_TEXT_CIRCLE_RADIUS (CIRCLE_RADIUS - scl_y(42))
#define MAIN_TEXT_BOUNDS                                                                           \
  GRect(-MAIN_TEXT_CIRCLE_RADIUS, -MAIN_TEXT_CIRCLE_RADIUS / 2, MAIN_TEXT_CIRCLE_RADIUS * 2,       \
        MAIN_TEXT_CIRCLE_RADIUS)
#define MAIN_TEXT_CIRCLE_RADIUS_EDIT (CIRCLE_RADIUS - scl_y(101))
#define MAIN_TEXT_BOUNDS_EDIT                                                                      \
  GRect(-MAIN_TEXT_CIRCLE_RADIUS_EDIT, -MAIN_TEXT_CIRCLE_RADIUS_EDIT / 2,                          \
        MAIN_TEXT_CIRCLE_RADIUS_EDIT * 2, MAIN_TEXT_CIRCLE_RADIUS_EDIT)
// Main Text
#define TEXT_FIELD_COUNT 5
#define TEXT_FIELD_EDIT_SPACING scl_y(42)
#define TEXT_FIELD_ANI_DURATION 140
// Focus Layer
#define FOCUS_FIELD_BORDER scl_y(30)
#define FOCUS_FIELD_SHRINK_INSET scl_y(18)
#define FOCUS_FIELD_SHRINK_DURATION 80
#define FOCUS_FIELD_ANI_DURATION 150
#define FOCUS_BOUNCE_ANI_HEIGHT scl_y(48)
#define FOCUS_BOUNCE_ANI_DURATION 70
#define FOCUS_BOUNCE_ANI_SETTLE_DURATION 140
// Header Text (different font on Emery and Gabbro)
#if defined(PBL_PLATFORM_APLITE) || defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_CHALK) || \
    defined(PBL_PLATFORM_DIORITE) || defined(PBL_PLATFORM_FLINT)
#define HEADER_Y_OFFSET scl_y(30)
#define FOOTER_Y_OFFSET scl_y(140)
#else
#define HEADER_Y_OFFSET scl_y(53)
#define FOOTER_Y_OFFSET scl_y(160)
#endif
// Fonts
typedef enum {
  ScalableFontLabel,
  ScalableFontTime,
  ScalableFontMenuSmall,
  ScalableFontMenuMedium,
  ScalableFontMenuLarge,
} ScalableFontIds;

// Main drawing state description, used to determine changes in state
typedef struct {
  ControlMode control_mode; //< The timer control mode at that state
  uint8_t hr_digits;        //< The number of digits used by the hours
  uint8_t min_digits;       //< The number of digits used by the minutes
} DrawState;

// Main data
static struct {
  Layer *layer;                        //< The main layer being drawn on, used to force a refresh
  int32_t progress_angle;              //< The current angle of the progress ring
  DrawState draw_state;                //< An arbitrary description of the main drawing state
  GRect text_fields[TEXT_FIELD_COUNT]; //< The number of text fields (hr : min : sec)
  GRect focus_field;                   //< The selection field layer
  GColor fore_color;                   //< Color of text
  GColor mid_color;                    //< Color of center
  GColor ring_color;                   //< Color of ring
  GColor back_color;                   //< Color behind ring
  int32_t menu_scroll_offset;          //< Animated offset for menu scrolling
  GRect pomo_focus_field;              //< Animated focus field for pomodoro creation
} drawing_data;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Focus Layer
//

// Update focus layer drawing state
static void prv_focus_layer_update_state(Layer *layer, GRect hr_bounds, GRect min_bounds,
                                         GRect sec_bounds) {
  // get properties
  GRect bounds = layer_get_bounds(layer);
  // check current control mode
  if (main_get_control_mode() == ControlModeCounting) {
    // calculate the bounds for the focus layer off the screen
    bounds.origin.x = bounds.size.w;
    bounds.origin.y = bounds.size.h / 2 - sec_bounds.size.h / 4;
    bounds.size.w = sec_bounds.size.w;
    bounds.size.h = sec_bounds.size.h / 2;
    // animate the focus layer
    animation_grect_start(&drawing_data.focus_field, bounds, FOCUS_FIELD_ANI_DURATION, 0,
                          CurveSinEaseOut);
  } else {
    // get final bounds when in editing mode
    if (main_get_control_mode() == ControlModeEditHr) {
      bounds = hr_bounds;
    } else if (main_get_control_mode() == ControlModeEditMin) {
      bounds = min_bounds;
    } else {
      bounds = sec_bounds;
    }
    // add border
    bounds = grect_inset(bounds, GEdgeInsets1(-FOCUS_FIELD_BORDER));
    // animate the focus field to those bounds
    animation_grect_start(&drawing_data.focus_field, bounds, FOCUS_FIELD_ANI_DURATION, 0,
                          CurveSinEaseOut);
  }
}

// Draw the focus layer
static void prv_render_focus_layer(GContext *ctx) {
#ifdef PBL_BW
  graphics_fill_rect_grey(ctx, drawing_data.focus_field);
#else
  graphics_context_set_fill_color(ctx, drawing_data.ring_color);
  graphics_fill_rect(ctx, drawing_data.focus_field, 0, GCornerNone);
#endif
}

// Update pomodoro focus layer state based on current mode
static void prv_pomo_focus_update_state(GRect bounds) {
  ControlMode mode = main_get_control_mode();

  // Calculate field positions (must match prv_render_pomodoro_create)
  int16_t center_x = bounds.size.w / 2;
  int16_t center_y = bounds.size.h / 2;
  int16_t field_width = scl_x(250);
  int16_t field_spacing = scl_x(56);
  int16_t field_height = scl_y(190);
  int16_t field_y = center_y - field_height / 2;

  // Get current time to determine layout
  int64_t time_ms;
  bool editing_work = (mode == ControlModePomodoroCreateWorkHr ||
                       mode == ControlModePomodoroCreateWorkMin ||
                       mode == ControlModePomodoroCreateWorkSec);
  if (editing_work) {
    time_ms = main_get_temp_pomodoro_work();
  } else {
    time_ms = main_get_temp_pomodoro_break();
  }
  uint16_t hr = time_ms / MSEC_IN_HR;

  int16_t total_width;
  if (hr > 0) {
    total_width = field_width * 3 + field_spacing * 2;
  } else {
    total_width = field_width * 2 + field_spacing;
  }
  int16_t start_x = center_x - total_width / 2;

  // Calculate target rect based on current mode
  GRect target;
  if (mode == ControlModePomodoroCreateWorkHr || mode == ControlModePomodoroCreateBreakHr) {
    target = GRect(start_x, field_y, field_width, field_height);
  } else if (mode == ControlModePomodoroCreateWorkMin || mode == ControlModePomodoroCreateBreakMin) {
    if (hr > 0) {
      target = GRect(start_x + field_width + field_spacing, field_y, field_width, field_height);
    } else {
      target = GRect(start_x, field_y, field_width, field_height);
    }
  } else {
    // Seconds
    if (hr > 0) {
      target = GRect(start_x + (field_width + field_spacing) * 2, field_y, field_width,
                     field_height);
    } else {
      target = GRect(start_x + field_width + field_spacing, field_y, field_width, field_height);
    }
  }

  // Add border and animate
  target = grect_inset(target, GEdgeInsets1(-FOCUS_FIELD_BORDER));
  animation_grect_start(&drawing_data.pomo_focus_field, target, FOCUS_FIELD_ANI_DURATION, 0,
    CurveSinEaseOut);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sub Texts
//

// Draw header text
static void prv_render_header_text(GContext *ctx, GRect bounds) {
  // calculate bounds
  bounds.origin = grect_center_point(&bounds);
  bounds.origin.x -= CIRCLE_RADIUS;
  bounds.origin.y -= (CIRCLE_RADIUS - HEADER_Y_OFFSET);
  bounds.size.w = CIRCLE_RADIUS * 2;
  bounds.size.h = CIRCLE_RADIUS / 2;
  // draw text
  char *buff;
  if (timer_is_chrono()) {
    buff = "Chrono";
  } else {
    buff = "Timer";
  }
  graphics_draw_text(ctx, buff, scl_get_font(ScalableFontLabel), bounds, GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

// Draw footer text
static void prv_render_footer_text(GContext *ctx, GRect bounds) {
  // calculate bounds
  bounds.origin = grect_center_point(&bounds);
  bounds.origin.x -= CIRCLE_RADIUS;
  bounds.origin.y += FOOTER_Y_OFFSET;
  bounds.size.w = CIRCLE_RADIUS * 2;
  bounds.size.h = CIRCLE_RADIUS - FOOTER_Y_OFFSET;
  // calculate text
  char buff[10];
  // in timer mode, get time
  time_t end_time = epoch() / MSEC_IN_SEC;
  if (main_get_control_mode() != ControlModeCounting && !timer_is_chrono()) {
    end_time += timer_get_value_ms() / MSEC_IN_SEC;
  }
  // format to readable time
  struct tm end_tm = *localtime(&end_time);
  strftime(buff, sizeof(buff), clock_is_24h_style() ? "%H:%M" : "%I:%M", &end_tm);
  if (buff[0] == '0') { // Strip leading zero
    for (uint8_t ii = 1; ii < ARRAY_LENGTH(buff); ii++) {
      buff[ii - 1] = buff[ii];
    }
  }
  // draw text
  graphics_draw_text(ctx, buff, scl_get_font(ScalableFontTime), bounds, GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Main Text
//

// Update main text drawing state
static void prv_main_text_update_state(Layer *layer) {
  // get properties
  GRect bounds = layer_get_bounds(layer);
  bool edit_mode = main_get_control_mode() != ControlModeCounting;
  // calculate time parts
  uint16_t hr, min, sec;
  timer_get_time_parts(&hr, &min, &sec);
  // convert to strings
  char buff[TEXT_FIELD_COUNT][6] = {{'\0'}};
  if (hr) {
    snprintf(buff[0], sizeof(buff[0]), edit_mode ? "%02d" : "%d", hr);
  }
  snprintf(buff[1], sizeof(buff[1]), "%s", hr && !edit_mode ? ":" : "\0");
  snprintf(buff[2], sizeof(buff[2]), (hr || edit_mode) ? "%02d" : "%d", min);
  snprintf(buff[3], sizeof(buff[3]), "%s", edit_mode ? "\0" : ":");
  snprintf(buff[4], sizeof(buff[4]), "%02d", sec);
  // calculate new sizes for all text elements
  char tot_buff[26];
  snprintf(tot_buff, sizeof(tot_buff), "%s%s%s%s%s", buff[0], buff[1], buff[2], buff[3], buff[4]);
  int16_t font_size =
      text_render_get_max_font_size(tot_buff, edit_mode ? MAIN_TEXT_BOUNDS_EDIT : MAIN_TEXT_BOUNDS);
  // calculate new size for each text element
  GRect total_bounds = GRectZero;
  GRect field_bounds[TEXT_FIELD_COUNT];
  for (uint8_t ii = 0; ii < TEXT_FIELD_COUNT; ii++) {
    field_bounds[ii] = text_render_get_content_bounds(buff[ii], font_size);
    // if in edit mode and some fields have content and this one is '\0', then pad it
    if (edit_mode && total_bounds.size.w && field_bounds[ii].size.w == 0) {
      field_bounds[ii].size.w = TEXT_FIELD_EDIT_SPACING;
    }
    total_bounds.size.w += field_bounds[ii].size.w;
  }
  total_bounds.size.h = field_bounds[TEXT_FIELD_COUNT - 1].size.h;
  total_bounds.origin.x = (bounds.size.w - total_bounds.size.w) / 2;
  total_bounds.origin.y = (bounds.size.h - total_bounds.size.h) / 2;
  // calculate positions for all text elements
  field_bounds[0].origin = total_bounds.origin;
  for (uint8_t ii = 0; ii < TEXT_FIELD_COUNT - 1; ii++) {
    field_bounds[ii + 1].origin.x = field_bounds[ii].origin.x + field_bounds[ii].size.w;
    field_bounds[ii + 1].origin.y = total_bounds.origin.y;
  }
  // animate to new positions
  for (uint8_t ii = 0; ii < TEXT_FIELD_COUNT; ii++) {
    animation_grect_start(&drawing_data.text_fields[ii], field_bounds[ii], TEXT_FIELD_ANI_DURATION,
                          0, CurveSinEaseOut);
  }

  // update the focus layers
  prv_focus_layer_update_state(layer, field_bounds[0], field_bounds[2], field_bounds[4]);
}

// Draw main text onto drawing context
static void prv_render_main_text(GContext *ctx, GRect bounds) {
  // get time parts
  uint16_t hr, min, sec;
  timer_get_time_parts(&hr, &min, &sec);
  // convert to strings
  bool edit_mode = main_get_control_mode() != ControlModeCounting;
  char buff[TEXT_FIELD_COUNT][6] = {{'\0'}};
  if (hr) {
    snprintf(buff[0], sizeof(buff[0]), edit_mode ? "%02d" : "%d", hr);
  }
  snprintf(buff[1], sizeof(buff[1]), "%s", hr && !edit_mode ? ":" : "\0");
  snprintf(buff[2], sizeof(buff[2]), (hr || edit_mode) ? "%02d" : "%d", min);
  snprintf(buff[3], sizeof(buff[3]), "%s", edit_mode ? "\0" : ":");
  snprintf(buff[4], sizeof(buff[4]), "%02d", sec);
  // draw the main text elements in their respective bounds
  for (uint8_t ii = 0; ii < TEXT_FIELD_COUNT; ii++) {
    text_render_draw_scalable_text(ctx, buff[ii], drawing_data.text_fields[ii]);
  }
}

// Animation update callback
static void prv_animation_update_callback(void) {
  // refresh
  layer_mark_dirty(drawing_data.layer);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Progress Ring
//

// Draw progress ring
static void prv_render_progress_ring(GContext *ctx, GRect bounds) {
  const int16_t PADDING = 1;
  // calculate ring bounds size
  int32_t gr_angle = atan2_lookup(bounds.size.h, bounds.size.w);
  int32_t radius = (int32_t)bounds.size.h * TRIG_MAX_RATIO / sin_lookup(gr_angle) / 2 + PADDING;
  bounds.origin.x += bounds.size.w / 2 - radius;
  bounds.origin.y += bounds.size.h / 2 - radius;
  bounds.size.w = bounds.size.h = radius * 2;
  // draw ring on context
  int32_t angle_1 = drawing_data.progress_angle;
  int32_t angle_2 = TRIG_MAX_ANGLE;
  graphics_context_set_fill_color(ctx, drawing_data.back_color);
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFillCircle, radius, angle_1, angle_2);
}

// Update the progress ring position based on the current and total values
static void prv_progress_ring_update(void) {
  // calculate new angle
  int32_t new_angle;
  if (timer_is_chrono()) {
    new_angle = TRIG_MAX_ANGLE * (timer_get_value_ms() % MSEC_IN_MIN) / MSEC_IN_MIN;
  } else if (timer_get_length_ms() > 0) {
    new_angle = TRIG_MAX_ANGLE * timer_get_value_ms() / timer_get_length_ms();
  } else {
    new_angle = 0;
  }
  // check if large angle and animate
  animation_stop(&drawing_data.progress_angle);
  if (abs(new_angle - drawing_data.progress_angle) >= ANGLE_CHANGE_ANI_THRESHOLD) {
    animation_int32_start(&drawing_data.progress_angle, new_angle, PROGRESS_ANI_DURATION, 0,
                          CurveSinEaseOut);
  } else {
    drawing_data.progress_angle = new_angle;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Drawing State Changes
//

// Compare two different TextStates, return true if conditions are met for a refresh
static bool prv_text_state_compare(DrawState text_state_1, DrawState text_state_2) {
  return text_state_1.control_mode == text_state_2.control_mode && // if control modes are different
         ((text_state_1.control_mode != ControlModeCounting &&     // if in edit mode
           ((text_state_1.hr_digits && text_state_2.hr_digits) ||
            (!text_state_1.hr_digits && !text_state_2.hr_digits))) ||
          (text_state_1.control_mode == ControlModeCounting && // if in counting mode
           text_state_1.hr_digits == text_state_2.hr_digits &&
           text_state_1.min_digits == text_state_2.min_digits)) &&
         text_state_2.hr_digits < 3; // on first start hr is set to 99 to force refresh
}

// Create a state description
static DrawState prv_draw_state_create(void) {
  // get states
  uint16_t hr, min, sec;
  timer_get_time_parts(&hr, &min, &sec);
  return (DrawState){
      .control_mode = main_get_control_mode(),
      .hr_digits = (uint8_t)(hr > 0) + (uint8_t)(hr > 9) + (uint8_t)(hr > 99),
      .min_digits = (uint8_t)(min > 0) + (uint8_t)(min > 9),
  };
}

// Check for draw state changes and update drawing accordingly
static void prv_update_draw_state(Layer *layer) {
  // check for changes in the states of things
  DrawState cur_draw_state = prv_draw_state_create();
  if (!prv_text_state_compare(cur_draw_state, drawing_data.draw_state)) {
    drawing_data.draw_state = cur_draw_state;
    // update text state
    prv_main_text_update_state(layer);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// API Implementation
//

// Create bounce animation for focus layer
void drawing_start_bounce_animation(bool upward) {
  // get the currently selected elements
  // only animate the position of one focus layer, stacking gives appearance of stretching
  GRect *txt_rect;
  if (main_get_control_mode() == ControlModeEditHr) {
    txt_rect = &drawing_data.text_fields[0];
  } else if (main_get_control_mode() == ControlModeEditMin) {
    txt_rect = &drawing_data.text_fields[2];
  } else {
    txt_rect = &drawing_data.text_fields[4];
  }
  // animate text
  GRect rect_to = (*txt_rect);
  rect_to.origin.y = drawing_data.text_fields[1].origin.y;
  rect_to.origin.y += (upward ? -1 : 1) * FOCUS_BOUNCE_ANI_HEIGHT;
  animation_grect_start(txt_rect, rect_to, FOCUS_BOUNCE_ANI_DURATION, 0, CurveSinEaseIn);
  rect_to.origin.y = drawing_data.text_fields[1].origin.y;
  animation_grect_start(txt_rect, rect_to, FOCUS_BOUNCE_ANI_SETTLE_DURATION,
                        FOCUS_BOUNCE_ANI_DURATION, CurveSinEaseOut);
  // get focus layer desired bounds
  GRect focus_bounds = drawing_data.text_fields[0];
  if (main_get_control_mode() == ControlModeEditMin) {
    focus_bounds = drawing_data.text_fields[2];
  } else if (main_get_control_mode() == ControlModeEditSec) {
    focus_bounds = drawing_data.text_fields[4];
  }
  focus_bounds.origin.y = drawing_data.text_fields[3].origin.y;
  focus_bounds = grect_inset(focus_bounds, GEdgeInsets1(-FOCUS_FIELD_BORDER));
  // animate focus layer
  rect_to = focus_bounds;
  rect_to.origin.y += (upward ? -1 : 0) * FOCUS_BOUNCE_ANI_HEIGHT;
  rect_to.size.h += FOCUS_BOUNCE_ANI_HEIGHT;
  animation_grect_start(&drawing_data.focus_field, rect_to, FOCUS_BOUNCE_ANI_DURATION,
                        FOCUS_BOUNCE_ANI_DURATION, CurveSinEaseIn);
  // return to original position
  animation_grect_start(&drawing_data.focus_field, focus_bounds, FOCUS_BOUNCE_ANI_SETTLE_DURATION,
                        FOCUS_BOUNCE_ANI_DURATION * 2, CurveSinEaseOut);
}

// Create reset animation for focus layer
void drawing_start_reset_animation(void) {
  // create shrunken focus bounds and animate to new bounds
  GRect focus_to_bounds;
  focus_to_bounds = grect_inset(drawing_data.focus_field, GEdgeInsets1(FOCUS_FIELD_SHRINK_INSET));
  // shrinking animation
  animation_grect_start(&drawing_data.focus_field, focus_to_bounds, FOCUS_FIELD_SHRINK_DURATION, 0,
                        CurveLinear);
  // return animation back to original size
  animation_grect_start(&drawing_data.focus_field, drawing_data.focus_field,
                        FOCUS_FIELD_SHRINK_DURATION, BUTTON_HOLD_RESET_MS, CurveLinear);
}

// Menu scroll animation constants
#define MENU_SCROLL_DISTANCE 30
#define MENU_SCROLL_DURATION 120

// Create scroll animation for settings menu
void drawing_start_menu_scroll_animation(bool upward) {
  // Start from an offset and animate back to 0
  drawing_data.menu_scroll_offset = upward ? -MENU_SCROLL_DISTANCE : MENU_SCROLL_DISTANCE;
  animation_int32_start(&drawing_data.menu_scroll_offset, 0, MENU_SCROLL_DURATION, 0, CurveSinEaseOut);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup Menu Rendering
//

// Helper to get menu item text for a given index
// Negative indices = pomodoro section, 0 = Reset, positive = timer section
// Returns false if index is out of range
static bool prv_get_menu_item_text(int8_t index, char *buffer, size_t buf_size) {
  uint8_t filled_timers = 0;
  for (int i = 0; i < TIMER_SLOT_COUNT; i++) {
    if (timer_slots_data.slots[i].length_ms > 0) filled_timers++;
  }
  uint8_t filled_pomodoros = pomodoro_slots_count_filled();

  bool has_add_timer = (filled_timers < TIMER_SLOT_COUNT);
  bool has_add_pomodoro = (filled_pomodoros < POMODORO_SLOT_COUNT);

  // +1 for Repeat toggle at the end
  int8_t max_timer_index = filled_timers + (has_add_timer ? 1 : 0) + 1;
  int8_t min_pomodoro_index = -(filled_pomodoros + (has_add_pomodoro ? 1 : 0));

  // Check bounds
  if (index < min_pomodoro_index || index > max_timer_index) {
    return false;
  }

  if (index == 0) {
    snprintf(buffer, buf_size, "Reset");
    return true;
  } else if (index > 0) {
    // Timer section (positive indices)
    if (index <= filled_timers) {
      // Find the nth filled timer slot
      int count = 0;
      for (int i = 0; i < TIMER_SLOT_COUNT; i++) {
        if (timer_slots_data.slots[i].length_ms > 0) {
          count++;
          if (count == index) {
            int64_t ms = timer_slots_data.slots[i].length_ms;
            int min = (ms / MSEC_IN_MIN) % 60;
            int sec = (ms / MSEC_IN_SEC) % 60;
            int hr = ms / MSEC_IN_HR;
            if (hr > 0) {
              snprintf(buffer, buf_size, "%d:%02d:%02d", hr, min, sec);
            } else {
              snprintf(buffer, buf_size, "%02d:%02d", min, sec);
            }
            return true;
          }
        }
      }
    } else if (has_add_timer && index == filled_timers + 1) {
      snprintf(buffer, buf_size, "+ Save Timer");
      return true;
    } else if (index == max_timer_index) {
      // Repeat toggle is always the last item
      snprintf(buffer, buf_size, "Repeat: %s", timer_data.repeat_enabled ? "On" : "Off");
      return true;
    }
  } else {
    // Pomodoro section (negative indices)
    // -1 to -N = saved pomodoros, -(N+1) = Add New (if room)
    int8_t pomo_index = -index - 1;  // -1 -> 0, -2 -> 1, etc.

    if (pomo_index < filled_pomodoros) {
      // This is a saved pomodoro
      int8_t slot_index = pomodoro_slots_get_index_for_selection(pomo_index);
      if (slot_index >= 0) {
        int64_t work_ms = pomodoro_slots_data.slots[slot_index].work_time_ms;
        int64_t break_ms = pomodoro_slots_data.slots[slot_index].break_time_ms;
        int work_hr = work_ms / MSEC_IN_HR;
        int work_min = (work_ms % MSEC_IN_HR) / MSEC_IN_MIN;
        int break_hr = break_ms / MSEC_IN_HR;
        int break_min = (break_ms % MSEC_IN_HR) / MSEC_IN_MIN;

        // Use "-" separator between work and break times
        if (work_hr > 0 || break_hr > 0) {
          snprintf(buffer, buf_size, "%d:%02d-%d:%02d", work_hr, work_min, break_hr, break_min);
        } else {
          snprintf(buffer, buf_size, "%02d-%02d", work_min, break_min);
        }
        return true;
      }
    } else if (has_add_pomodoro && pomo_index == filled_pomodoros) {
      // This is the Add New option
      snprintf(buffer, buf_size, "+ Add Pomo");
      return true;
    }
  }
  return false;
}

// Render unified setup menu within the circle - scrolling style
// Helper to check if string can be rendered with LECO font (digits, colons, periods only)
static bool prv_is_leco_compatible(const char *str) {
  for (int i = 0; str[i] != '\0'; i++) {
    char c = str[i];
    if (!((c >= '0' && c <= '9') || c == ':' || c == '.')) {
      return false;
    }
  }
  return true;
}

// Helper to check if string is a pomodoro format (work-break with dash)
static bool prv_is_pomodoro_format(const char *str) {
  for (int i = 0; str[i] != '\0'; i++) {
    if (str[i] == '-') return true;
  }
  return false;
}

// Helper to draw pomodoro time with LECO for numbers and system font for dash
static void prv_draw_pomodoro_time(GContext *ctx, const char *str, GRect bounds, bool dimmed) {
  // Find the dash position
  int dash_pos = -1;
  for (int i = 0; str[i] != '\0'; i++) {
    if (str[i] == '-') {
      dash_pos = i;
      break;
    }
  }
  if (dash_pos < 0) return;

  // Split into work and break parts
  char work_str[12], break_str[12];
  strncpy(work_str, str, dash_pos);
  work_str[dash_pos] = '\0';
  strcpy(break_str, str + dash_pos + 1);

  // Calculate widths
  GRect work_bounds = text_render_get_content_bounds(work_str, bounds.size.h);
  GRect break_bounds = text_render_get_content_bounds(break_str, bounds.size.h);
  int16_t dash_width = scl_x(56);
  int16_t total_width = work_bounds.size.w + dash_width + break_bounds.size.w;

  // Center horizontally
  int16_t start_x = bounds.origin.x + (bounds.size.w - total_width) / 2;
  int16_t x_pos = start_x;

  GColor color = dimmed ? PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack) : drawing_data.fore_color;

  // Draw work time with LECO
  GRect work_rect = GRect(x_pos, bounds.origin.y, work_bounds.size.w, bounds.size.h);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  text_render_draw_scalable_text(ctx, work_str, work_rect);
  x_pos += work_bounds.size.w;

  // Draw dash with system font
  GRect dash_rect = GRect(x_pos - scl_x(14), bounds.origin.y - scl_y(12),
                          dash_width + scl_x(28), bounds.size.h + scl_y(24));
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, "-", scl_get_font(ScalableFontMenuMedium),
                     dash_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  x_pos += dash_width;

  // Draw break time with LECO
  GRect break_rect = GRect(x_pos, bounds.origin.y, break_bounds.size.w, bounds.size.h);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  text_render_draw_scalable_text(ctx, break_str, break_rect);
}

static void prv_render_setup_menu(GContext *ctx, GRect bounds) {
  ControlMode mode = main_get_control_mode();
  if (mode != ControlModeSetupMenu) return;

  int8_t selection = main_get_setup_selection();
  int16_t center_x = bounds.size.w / 2;
  int16_t center_y = bounds.size.h / 2;
  int16_t scroll_offset = drawing_data.menu_scroll_offset;
  int16_t dim_w = scl_x(694);
  int16_t dim_h = scl_y(90);
  int16_t dim_text_h = scl_y(149);
  int16_t selected_w = scl_x(764);
  int16_t selected_h = scl_y(120);
  int16_t selected_text_h = scl_y(179);
  int16_t selected_highlight_h = scl_y(131);
  int16_t item_gap = scl_y(250);

  char item_text[20];

  // Draw item ABOVE selection (if exists)
  if (prv_get_menu_item_text(selection - 1, item_text, sizeof(item_text))) {
    GRect above_rect = GRect(center_x - dim_w / 2, center_y - item_gap + scroll_offset, dim_w,
                             dim_h);
    if (prv_is_pomodoro_format(item_text)) {
      prv_draw_pomodoro_time(ctx, item_text, above_rect, true);
    } else if (prv_is_leco_compatible(item_text)) {
      GColor dimmed_color = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
      graphics_context_set_stroke_color(ctx, dimmed_color);
      graphics_context_set_fill_color(ctx, dimmed_color);
      text_render_draw_scalable_text(ctx, item_text, above_rect);
    } else {
      GRect text_rect = GRect(center_x - dim_w / 2, above_rect.origin.y - scl_y(48), dim_w,
                              dim_text_h);
      graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
      graphics_draw_text(ctx, item_text, scl_get_font(ScalableFontMenuMedium),
                         text_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
  }

  // Draw SELECTED item in center (highlighted)
  if (prv_get_menu_item_text(selection, item_text, sizeof(item_text))) {
    if (prv_is_pomodoro_format(item_text)) {
      GRect text_rect = GRect(center_x - selected_w / 2, center_y - selected_h / 2 + scroll_offset,
                              selected_w, selected_h);
      GRect highlight_rect = grect_inset(text_rect, GEdgeInsets1(-FOCUS_FIELD_BORDER));
#ifdef PBL_BW
      graphics_fill_rect_grey(ctx, highlight_rect);
#else
      graphics_context_set_fill_color(ctx, drawing_data.ring_color);
      graphics_fill_rect(ctx, highlight_rect, 0, GCornerNone);
#endif
      prv_draw_pomodoro_time(ctx, item_text, text_rect, false);
    } else if (prv_is_leco_compatible(item_text)) {
      GRect text_rect = GRect(center_x - selected_w / 2, center_y - selected_h / 2 + scroll_offset,
                              selected_w, selected_h);
      GRect highlight_rect = grect_inset(text_rect, GEdgeInsets1(-FOCUS_FIELD_BORDER));
#ifdef PBL_BW
      graphics_fill_rect_grey(ctx, highlight_rect);
#else
      graphics_context_set_fill_color(ctx, drawing_data.ring_color);
      graphics_fill_rect(ctx, highlight_rect, 0, GCornerNone);
#endif
      graphics_context_set_stroke_color(ctx, drawing_data.fore_color);
      graphics_context_set_fill_color(ctx, drawing_data.fore_color);
      text_render_draw_scalable_text(ctx, item_text, text_rect);
    } else {
      GRect text_rect = GRect(center_x - selected_w / 2,
                              center_y - selected_text_h / 2 + scroll_offset, selected_w,
                              selected_text_h);
      GRect highlight_rect = GRect(center_x - selected_w / 2,
                                   center_y - selected_highlight_h / 2 + scroll_offset,
                                   selected_w, selected_highlight_h);
#ifdef PBL_BW
      graphics_fill_rect_grey(ctx, highlight_rect);
#else
      graphics_context_set_fill_color(ctx, drawing_data.ring_color);
      graphics_fill_rect(ctx, highlight_rect, scl_y(24), GCornersAll);
#endif
      graphics_context_set_text_color(ctx, GColorBlack);
      graphics_draw_text(ctx, item_text, scl_get_font(ScalableFontMenuLarge),
                         text_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
  }

  // Draw item BELOW selection (if exists)
  if (prv_get_menu_item_text(selection + 1, item_text, sizeof(item_text))) {
    GRect below_rect = GRect(center_x - dim_w / 2, center_y + item_gap - dim_h + scroll_offset,
                             dim_w, dim_h);
    if (prv_is_pomodoro_format(item_text)) {
      prv_draw_pomodoro_time(ctx, item_text, below_rect, true);
    } else if (prv_is_leco_compatible(item_text)) {
      GColor dimmed_color = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
      graphics_context_set_stroke_color(ctx, dimmed_color);
      graphics_context_set_fill_color(ctx, dimmed_color);
      text_render_draw_scalable_text(ctx, item_text, below_rect);
    } else {
      GRect text_rect = GRect(center_x - dim_w / 2, below_rect.origin.y - scl_y(24), dim_w,
                              dim_text_h);
      graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
      graphics_draw_text(ctx, item_text, scl_get_font(ScalableFontMenuMedium),
                         text_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
  }

  // Show "Hold to Delete" help text if a deletable item is selected
  int filled_timers = 0;
  for (int i = 0; i < TIMER_SLOT_COUNT; i++) {
    if (timer_slots_data.slots[i].length_ms > 0) filled_timers++;
  }
  int filled_pomodoros = (int)pomodoro_slots_count_filled();

  bool is_deletable = false;
  // Check if saved timer (positive index within filled range)
  if ((int)selection >= 1 && (int)selection <= filled_timers) {
    is_deletable = true;
  }
  // Check if saved pomodoro (negative index within filled range)
  if (selection < 0) {
    int pomo_index = -selection - 1;
    if (pomo_index < filled_pomodoros) {
      is_deletable = true;
    }
  }

  if (is_deletable) {
    GRect help_rect = GRect(0, bounds.size.h - scl_y(119), bounds.size.w, scl_y(107));
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    graphics_draw_text(ctx, "Hold to Delete", scl_get_font(ScalableFontMenuSmall),
                       help_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

// Helper to check if in pomodoro work creation mode
static bool prv_is_pomodoro_work_create_mode(ControlMode mode) {
  return mode == ControlModePomodoroCreateWorkHr ||
         mode == ControlModePomodoroCreateWorkMin ||
         mode == ControlModePomodoroCreateWorkSec;
}

// Helper to check if in pomodoro break creation mode
static bool prv_is_pomodoro_break_create_mode(ControlMode mode) {
  return mode == ControlModePomodoroCreateBreakHr ||
         mode == ControlModePomodoroCreateBreakMin ||
         mode == ControlModePomodoroCreateBreakSec;
}

// Helper to check if in any pomodoro creation mode
static bool prv_is_any_pomodoro_create_mode(ControlMode mode) {
  return prv_is_pomodoro_work_create_mode(mode) || prv_is_pomodoro_break_create_mode(mode);
}

// Render pomodoro creation screen (timer-style UI)
static void prv_render_pomodoro_create(GContext *ctx, GRect bounds) {
  ControlMode mode = main_get_control_mode();
  if (!prv_is_any_pomodoro_create_mode(mode)) return;

  int16_t center_x = bounds.size.w / 2;
  int16_t center_y = bounds.size.h / 2;

  // Get temp values
  int64_t work_ms = main_get_temp_pomodoro_work();
  int64_t break_ms = main_get_temp_pomodoro_break();

  // Calculate time parts
  uint16_t work_hr = work_ms / MSEC_IN_HR;
  uint16_t work_min = (work_ms % MSEC_IN_HR) / MSEC_IN_MIN;
  uint16_t work_sec = (work_ms % MSEC_IN_MIN) / MSEC_IN_SEC;
  uint16_t break_hr = break_ms / MSEC_IN_HR;
  uint16_t break_min = (break_ms % MSEC_IN_HR) / MSEC_IN_MIN;
  uint16_t break_sec = (break_ms % MSEC_IN_MIN) / MSEC_IN_SEC;

  bool editing_work = prv_is_pomodoro_work_create_mode(mode);

  // Draw header (Work or Break)
  const char *header = editing_work ? "Work" : "Break";
  GRect header_rect = GRect(center_x - scl_x(347), center_y - CIRCLE_RADIUS + scl_y(60),
                            scl_x(694), scl_y(149));
  graphics_context_set_text_color(ctx, drawing_data.fore_color);
  graphics_draw_text(ctx, header, scl_get_font(ScalableFontMenuLarge),
                     header_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // Determine which time to display
  uint16_t hr, min, sec;
  if (editing_work) {
    hr = work_hr; min = work_min; sec = work_sec;
  } else {
    hr = break_hr; min = break_min; sec = break_sec;
  }

  // Calculate field positions - draw each field separately (no colons, like main screen)
  int16_t field_width = scl_x(250);
  int16_t field_spacing = scl_x(56);
  int16_t field_height = scl_y(190);
  int16_t total_width;
  int16_t field_y = center_y - field_height / 2;

  if (hr > 0) {
    // HH MM SS layout
    total_width = field_width * 3 + field_spacing * 2;
  } else {
    // MM SS layout
    total_width = field_width * 2 + field_spacing;
  }

  int16_t start_x = center_x - total_width / 2;
  int16_t x_pos = start_x;

  char field_str[6];

  // Draw animated focus field first (behind text)
#ifdef PBL_BW
  graphics_fill_rect_grey(ctx, drawing_data.pomo_focus_field);
#else
  graphics_context_set_fill_color(ctx, drawing_data.ring_color);
  graphics_fill_rect(ctx, drawing_data.pomo_focus_field, 0, GCornerNone);
#endif

  // Draw hours if present
  if (hr > 0) {
    GRect hr_rect = GRect(x_pos, field_y, field_width, field_height);
    snprintf(field_str, sizeof(field_str), "%02d", hr);
    graphics_context_set_stroke_color(ctx, drawing_data.fore_color);
    graphics_context_set_fill_color(ctx, drawing_data.fore_color);
    text_render_draw_scalable_text(ctx, field_str, hr_rect);
    x_pos += field_width + field_spacing;
  }

  // Draw minutes
  GRect min_rect = GRect(x_pos, field_y, field_width, field_height);
  snprintf(field_str, sizeof(field_str), "%02d", min);
  graphics_context_set_stroke_color(ctx, drawing_data.fore_color);
  graphics_context_set_fill_color(ctx, drawing_data.fore_color);
  text_render_draw_scalable_text(ctx, field_str, min_rect);
  x_pos += field_width + field_spacing;

  // Draw seconds
  GRect sec_rect = GRect(x_pos, field_y, field_width, field_height);
  snprintf(field_str, sizeof(field_str), "%02d", sec);
  graphics_context_set_stroke_color(ctx, drawing_data.fore_color);
  graphics_context_set_fill_color(ctx, drawing_data.fore_color);
  text_render_draw_scalable_text(ctx, field_str, sec_rect);

  // Draw the other time (dimmed) at bottom
  const char *other_label = editing_work ? "Break:" : "Work:";
  char other_time[20];
  if (editing_work) {
    if (break_hr > 0) {
      snprintf(other_time, sizeof(other_time), "%d:%02d:%02d", break_hr, break_min, break_sec);
    } else {
      snprintf(other_time, sizeof(other_time), "%02d:%02d", break_min, break_sec);
    }
  } else {
    if (work_hr > 0) {
      snprintf(other_time, sizeof(other_time), "%d:%02d:%02d", work_hr, work_min, work_sec);
    } else {
      snprintf(other_time, sizeof(other_time), "%02d:%02d", work_min, work_sec);
    }
  }
  char other_str[30];
  snprintf(other_str, sizeof(other_str), "%s %s", other_label, other_time);
  GRect other_rect = GRect(0, center_y + CIRCLE_RADIUS - scl_y(238), bounds.size.w, scl_y(149));
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
  graphics_draw_text(ctx, other_str, scl_get_font(ScalableFontMenuMedium),
                     other_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// Render pomodoro configuration menu
static void prv_render_pomodoro_menu(GContext *ctx, GRect bounds) {
  ControlMode mode = main_get_control_mode();
  if (mode != ControlModePomodoroMenu &&
      mode != ControlModePomodoroEditWork &&
      mode != ControlModePomodoroEditBreak) return;

  int16_t center_y = bounds.size.h / 2;

  // Title
  graphics_context_set_text_color(ctx, GColorWhite);
  GRect title_rect = GRect(0, scl_y(119), bounds.size.w, scl_y(149));
  graphics_draw_text(ctx, "POMODORO", scl_get_font(ScalableFontMenuLarge),
                     title_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // Work time
  char work_text[20];
  int work_min = pomodoro_data.work_time_ms / MSEC_IN_MIN;
  snprintf(work_text, sizeof(work_text), "Work: %d min", work_min);

  GRect work_rect = GRect(scl_x(139), center_y - scl_y(179), bounds.size.w - scl_x(278),
                          scl_y(208));
  if (mode == ControlModePomodoroEditWork) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, work_rect, scl_y(24), GCornersAll);
    graphics_context_set_text_color(ctx, GColorBlack);
  } else {
    graphics_context_set_text_color(ctx, GColorWhite);
  }
  graphics_draw_text(ctx, work_text, scl_get_font(ScalableFontMenuLarge),
                     GRect(work_rect.origin.x, work_rect.origin.y + scl_y(24), work_rect.size.w,
                           work_rect.size.h),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // Break time
  char break_text[20];
  int break_min = pomodoro_data.break_time_ms / MSEC_IN_MIN;
  snprintf(break_text, sizeof(break_text), "Break: %d min", break_min);

  GRect break_rect = GRect(scl_x(139), center_y + scl_y(89), bounds.size.w - scl_x(278),
                           scl_y(208));
  if (mode == ControlModePomodoroEditBreak) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, break_rect, scl_y(24), GCornersAll);
    graphics_context_set_text_color(ctx, GColorBlack);
  } else {
    graphics_context_set_text_color(ctx, GColorWhite);
  }
  graphics_draw_text(ctx, break_text, scl_get_font(ScalableFontMenuLarge),
                     GRect(break_rect.origin.x, break_rect.origin.y + scl_y(24),
                           break_rect.size.w, break_rect.size.h),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // Hint at bottom
  graphics_context_set_text_color(ctx, GColorLightGray);
  GRect hint_rect = GRect(0, bounds.size.h - scl_y(179), bounds.size.w, scl_y(149));
  graphics_draw_text(ctx, "SELECT to start", scl_get_font(ScalableFontMenuMedium),
                     hint_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// Render pomodoro indicator during active timer
static void prv_render_pomodoro_indicator(GContext *ctx, GRect bounds) {
  if (!pomodoro_data.is_active) return;

  // Draw WORK or BREAK indicator at top
  const char *phase_text = pomodoro_data.is_work_phase ? "WORK" : "BREAK";
  graphics_context_set_text_color(ctx, drawing_data.fore_color);
  GRect phase_rect = GRect(5, 5, 60, 20);
  graphics_draw_text(ctx, phase_text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     phase_rect, GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // Draw cycle count at top right
  char cycle_text[10];
  snprintf(cycle_text, sizeof(cycle_text), "x%d", pomodoro_data.completed_cycles);
  GRect cycle_rect = GRect(bounds.size.w - 40, 5, 35, 20);
  graphics_draw_text(ctx, cycle_text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     cycle_rect, GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

// Render everything to the screen
void drawing_render(Layer *layer, GContext *ctx) {
  // get properties
  GRect bounds = layer_get_bounds(layer);
  ControlMode mode = main_get_control_mode();

  // Check if we're in pomodoro menu mode (still uses black background)
  if (mode == ControlModePomodoroMenu ||
      mode == ControlModePomodoroEditWork ||
      mode == ControlModePomodoroEditBreak) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    prv_render_pomodoro_menu(ctx, bounds);
    return;
  }

  // Draw the circular background for normal mode, setup menu, and pomodoro creation
  graphics_context_set_fill_color(ctx, drawing_data.ring_color);
#ifdef PBL_BW
  graphics_fill_rect_grey(ctx, bounds);
#else
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
#endif
  prv_render_progress_ring(ctx, bounds);
  // draw main circle
  graphics_context_set_fill_color(ctx, drawing_data.mid_color);
  graphics_fill_circle(ctx, grect_center_point(&bounds), CIRCLE_RADIUS);

  // If in setup menu, render menu content instead of timer
  if (mode == ControlModeSetupMenu) {
    prv_render_setup_menu(ctx, bounds);
    return;
  }

  // If in pomodoro creation mode, render the timer-style editor
  if (prv_is_any_pomodoro_create_mode(mode)) {
    prv_render_pomodoro_create(ctx, bounds);
    return;
  }

  // Normal timer rendering
  // draw focus layer
  prv_render_focus_layer(ctx);
  // draw main text (drawn as filled and stroked path)
  graphics_context_set_stroke_color(ctx, drawing_data.fore_color);
  graphics_context_set_fill_color(ctx, drawing_data.fore_color);
  prv_render_main_text(ctx, bounds);
  // draw header and footer text
  graphics_context_set_text_color(ctx, drawing_data.fore_color);
  prv_render_header_text(ctx, bounds);
  prv_render_footer_text(ctx, bounds);
  // draw pomodoro indicator if active
  prv_render_pomodoro_indicator(ctx, bounds);
}

// Update the drawing states and recalculate everythings positions
void drawing_update(void) {
  // update drawing state
  prv_update_draw_state(drawing_data.layer);
  // update progress ring angle
  prv_progress_ring_update();
  // update pomodoro focus field if in pomodoro creation mode
  ControlMode mode = main_get_control_mode();
  if (prv_is_any_pomodoro_create_mode(mode)) {
    GRect bounds = layer_get_bounds(drawing_data.layer);
    prv_pomo_focus_update_state(bounds);
  }
}

// Initialize the singleton drawing data
void drawing_initialize(Layer *layer) {
  // get properties
  GRect bounds = layer_get_bounds(layer);
  // set the layer
  drawing_data.layer = layer;
  // set visual states
  drawing_data.progress_angle = 0;
  for (uint8_t ii = 0; ii < TEXT_FIELD_COUNT; ii++) {
    drawing_data.text_fields[ii].origin = grect_center_point(&bounds);
    drawing_data.text_fields[ii].size = GSizeZero;
  }
  drawing_data.focus_field.origin = grect_center_point(&bounds);
  if (main_get_control_mode() == ControlModeCounting) {
    drawing_data.focus_field.origin.x = bounds.size.w;
  }
  drawing_data.focus_field.size = GSizeZero;
  drawing_data.pomo_focus_field.origin = grect_center_point(&bounds);
  drawing_data.pomo_focus_field.size = GSizeZero;
  // set initial draw state to something which guaranties a refresh
  drawing_data.draw_state = (DrawState){
      .hr_digits = 99,
  };
  // set fonts
  GFont font_gothic_14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont font_gothic_18 = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont font_gothic_18_bold = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont font_gothic_24_bold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont font_gothic_28_bold = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont font_bebas_35_bold = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BEBAS_FONT_35));
  // clang-format off
  scl_set_fonts(ScalableFontLabel, {
    .o = font_gothic_24_bold, // Everything else
    .e = font_gothic_28_bold, // Emery (Pebble Time 2*)
    .g = font_gothic_28_bold, // Gabbro (Pebble Round 2)
  });
  scl_set_fonts(ScalableFontTime, {
    .o = font_gothic_28_bold, // Everything else
    .e = font_bebas_35_bold, // Emery (Pebble Time 2*)
    .g = font_bebas_35_bold, // Gabbro (Pebble Round 2)
  });
  scl_set_fonts(ScalableFontMenuSmall, {
    .o = font_gothic_14,
    .e = font_gothic_18,
    .g = font_gothic_18,
  });
  scl_set_fonts(ScalableFontMenuMedium, {
    .o = font_gothic_18_bold,
    .e = font_gothic_24_bold,
    .g = font_gothic_24_bold,
  });
  scl_set_fonts(ScalableFontMenuLarge, {
    .o = font_gothic_24_bold,
    .e = font_gothic_28_bold,
    .g = font_gothic_28_bold,
  });
  // clang-format on
  // set the colors
  drawing_data.fore_color = GColorBlack;
  drawing_data.mid_color = PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite);
  drawing_data.ring_color = PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorWhite);
  drawing_data.back_color = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
  // set animation update callback
  animation_register_update_callback(&prv_animation_update_callback);
}

// Destroy the singleton drawing data
void drawing_terminate(void) { animation_stop_all(); }
