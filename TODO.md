# Display SLStudio - TODO List

## Hardware Improvements
- [ ] **Backlight Control Investigation**: Find the correct GPIO pin for hardware backlight control on this specific XIAO ESP32-S3 + Round Display revision.
    - *Context*: Current "Blackout" mode only performs digital sleep (screen is black, but backlight stays on in 'ON' switch position).
    - *Pins already tested*: 6 (SCL conflict), 43 (D6), 44 (D7), 45 (Internal).
    - *Goal*: Enable total power shutdown of the display for maximum battery life during flight.

## UI/UX Refinements
- [ ] Tune flight detection thresholds (Altitude > 5m, Speed > 3m/s) based on real flight data.
- [ ] Add visual feedback (e.g., a small icon) when system detects it is in "Flight Mode".

## Telemetry
- [ ] Expand MAVLink message support if needed for additional sensors.
- [ ] Monitor MeshCore link reliability in "Blackout" mode.
