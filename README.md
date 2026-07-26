# Timer++

A feature-rich timer app for Pebble smartwatches with countdown timers, stopwatch mode, saved timers,
repeat timers, and Pomodoro support.

|                             Aplite                              |                      Basalt                       |                      Chalk                      |                             Diorite                              |                      Emery                      |                             Flint                              |                      Gabbro                       |
| :-------------------------------------------------------------: | :-----------------------------------------------: | :---------------------------------------------: | :--------------------------------------------------------------: | :---------------------------------------------: | :------------------------------------------------------------: | :-----------------------------------------------: |
| ![Aplite](assets/screenshots/aplite_diorite_flint_animated.gif) | ![Basalt](assets/screenshots/basalt_animated.gif) | ![Chalk](assets/screenshots/chalk_animated.gif) | ![Diorite](assets/screenshots/aplite_diorite_flint_animated.gif) | ![Emery](assets/screenshots/emery_animated.gif) | ![Flint](assets/screenshots/aplite_diorite_flint_animated.gif) | ![Gabbro](assets/screenshots/gabbro_animated.gif) |

## Features

- Countdown timer with hours, minutes, and seconds.
- Stopwatch mode when started from `0:00`.
- Background operation using the WakeUp API.
- Vibration alerts when timers complete.
- Up to 5 saved timers for quick access.
- Repeat timer mode for intervals, meditation, or recurring reminders.
- Pomodoro mode with customizable work/break durations and saved presets.
- Phase indicators and long-press phase skipping.

## Controls

- `Up/Down`: Adjust the selected time field.
- `Select`: Start/pause timer or advance between time fields.
- `Long Press Select`: Open settings while paused, reset while running, or delete selected saved items in settings.
- `Back`: Exit the app while leaving active timers running in the background.
- `Long Press Up/Down`: Skip to previous/next Pomodoro phase.

## Building

Requires the Pebble SDK.

```sh
pebble build
pebble install --phone <PHONE_IP>
```

### VS Code

To configure Visual Studio Code with formatting and intellisense run the following commands. This
will need to be repeated any time the build configuration changes.

1. Ensure `bear` is installed: `sudo apt install bear`.
2. Ensure a clean build so all commands will be captured: `pebble clean`.
3. Create the build directory: `mkdir -p build`.
4. Generate the `compile_commands.json` file:
   `bear --output build/compile_commands.json -- pebble build`.
5. Run the `Show Recommended Extensions` command and install the suggested extensions.
6. Run `clangd: Restart language server` for changes to take effect.

## Credits

Based on [Timer+](https://github.com/YclepticStudios/pebble-timer-plus) by Eric Phillips.

Timer++ modifications by Jerad Hoy include saved timers, Pomodoro support, repeat timers, settings
menu enhancements, phase skip controls, and Pebble Time 2 support.

## License

MIT License. See [LICENSE.md](LICENSE.md) for details.
