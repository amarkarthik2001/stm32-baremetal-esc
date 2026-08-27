stm32-bluepill-baremetal-telemetry

Bare-metal onboard telemetry firmware for the STM32F103C8T6 (Blue Pill), written directly against the register map — no HAL, no CMSIS drivers.

Target
MCU: STM32F103C8T6
Core: ARM Cortex-M3
Language: C
Board: Blue Pill
What it does
Reads the chip's real supply voltage using the internal VREFINT reference (no external divider needed)
Reads the chip's internal temperature sensor
Reads a raw current-sense voltage on PA2 (needs external shunt/amp hardware to represent real Amps — not converted yet)
Shows live VOLT / CURR / TEMP readings on an SSD1306 OLED (bit-banged I2C)
Blinks the onboard LED (PC13) every 1s as a heartbeat / alive check
Pin Map
Pin	Function
PA2	Current sense (raw ADC voltage, needs external shunt/amp)
PB6/PB7	I2C SCL/SDA — OLED
PC13	Onboard LED (heartbeat)
Timing

Two timebases: SysTick gives a 1ms millis() tick used for the LED blink and telemetry refresh rate. No microsecond timer is needed here since nothing in this build is timing-critical.

Not done yet
Current-to-Amps conversion (needs a real shunt resistor + amplifier, or a module like the ACS712 / INA219, wired to PA2)
Temperature calibration offset (F103 has no factory calibration for the internal sensor — compare against a real thermometer and adjust)
Any protection thresholds (over-voltage, over-current, over-temp)
Disclaimer

VOLT and TEMP are read straight from the chip's own internal sensors, so they work with zero external wiring. CURR is just the raw voltage seen on PA2 — if nothing's wired to that pin, it'll read noisy, meaningless values. That's expected, not a bug.

Status

Working on the bench: voltage, temperature, and OLED display are all live and refreshing. Current sensing is a placeholder until a shunt/amp is wired in.
