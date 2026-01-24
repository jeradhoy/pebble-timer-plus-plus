# Timer++

A beautiful, feature-rich timer app for Pebble smartwatches with support for countdown timers, stopwatch mode, and Pomodoro technique.

## Features

### Timer & Stopwatch
- **Countdown Timer**: Set hours, minutes, and seconds with intuitive controls
- **Stopwatch Mode**: Start from 0:00 to use as a stopwatch
- **Background Operation**: Timer runs in the background using WakeUp API - no need to keep the app open
- **Vibration Alerts**: Get notified when your timer completes

### Stored Timers
- **Save up to 5 timers** for quick access
- Long press select to open the settings menu
- Scroll down to view saved timers or add new ones
- Long press on a saved timer to delete it

### Repeat Timer
- **Auto-repeat mode**: Toggle "Repeat: On/Off" in the settings menu
- When enabled, timers automatically restart after completion
- Brief vibration (3 pulses) signals each repeat cycle
- Perfect for interval training, meditation, or any repeating task

### Pomodoro Timer
- **Built-in Pomodoro support** with customizable work and break durations
- Save up to 5 Pomodoro configurations (e.g., 25/5, 50/10)
- **Phase indicators**: Shows current phase (WORK/BREAK) and cycle count
- **Skip phases**: Long press up/down to skip to previous/next phase
- Works while timer is running or paused

### Controls

#### Main Screen
- **Up/Down**: Adjust time (hours/minutes/seconds based on selection)
- **Select**: Start/pause timer, move between time fields
- **Long Press Select**: Open settings menu (when paused) or reset timer (when running)
- **Back**: Exit app (timer continues in background)

#### During Pomodoro
- **Long Press Up**: Skip to previous phase
- **Long Press Down**: Skip to next phase

#### Settings Menu
- **Up/Down**: Navigate menu items
- **Select**: Choose item (Reset, saved timer, saved pomodoro, or add new)
- **Long Press**: Delete saved timer/pomodoro
- **Back**: Return to timer

## Building

Requires the Pebble SDK.

```bash
pebble build
pebble install --phone <PHONE_IP>
```

## Credits

Based on [Timer+](https://github.com/YclepticStudios/pebble-timer-plus) by Eric Phillips.

Timer++ modifications by Jerad Hoy, including:
- Stored timers feature
- Pomodoro timer support
- Repeat timer mode
- Settings menu with animations
- Phase skip controls
- UI improvements

## License

MIT License - See [LICENSE.md](LICENSE.md) for details.
