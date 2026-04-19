# Fix ESP32 Boot Issue - Step-by-Step TODO

**Goal:** Resolve bootloader-only boot (no user app) by verifying build/upload.

1. [] Build verbose: `pio run -v` → Check for compile/linker errors, firmware.bin size/location.
2. [] Add flash config to platformio.ini if needed (edit_file), rebuild.
3. [] Erase flash: `pio run --target erase`
4. [] Upload: `pio run --target upload` (confirm COM port first).
5. [] Monitor: `pio device monitor` → Expect banner "Relay + RGB + Improved EZO-pH/EC Test"
6. [] If fails: Test minimal blink, check partitions with esptool.

Progress: Updated after each step.
