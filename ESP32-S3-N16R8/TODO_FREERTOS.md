# FreeRTOS Conversion TODO

## Approved Plan Steps
1. [ ] Update `platformio.ini` to espidf framework.
2. [ ] Create `src/main.c` with app_main() + tasks (Relay, RGB, Sensor).
3. [ ] Update `include/pins.h` for C/FreeRTOS.
4. [ ] Update `architecture.md` with task diagram.
5. [ ] Update `README.md` with espidf notes.
6. [ ] Test build: `pio run`
7. [ ] Test upload/monitor.

*Tasks pinned to cores, stacks: Relay 2KB, RGB 2KB, Sensor 4KB.*

