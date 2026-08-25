# STM32 Bare-Metal ESC Firmware

Bare-metal sensorless BLDC ESC firmware for the STM32F103C8T6 (Blue Pill), written directly against the register map — no HAL, no CMSIS drivers.

## Target
- MCU: STM32F103C8T6
- Core: ARM Cortex-M3
- Language: C
- Board: Blue Pill

## What it does
- Reads RC throttle signal (1000–2000us) via input capture
- Generates 3-phase PWM (~20kHz) for the high side, GPIO for the low side
- Spins the motor up open-loop, then hands off to sensorless BEMF commutation
- Calculates RPM from commutation timing
- Reads bus voltage, current, and onboard temperature via ADC
- Shows live telemetry on an SSD1306 OLED (bit-banged I2C)
- Blinks the onboard LED (PC13) every 2s as a heartbeat / alive check

## Pin Map
| Pin | Function |
|---|---|
| PA8/PA9/PA10 | TIM1 PWM — phase A/B/C high side |
| PB0/PB1/PB10 | Phase A/B/C low side (GPIO) |
| PA4/PA5/PA6 | BEMF sense — phase A/B/C |
| PA1 | Bus voltage sense |
| PA2 | Current sense |
| PA0 | RC throttle input capture |
| PB6/PB7 | I2C SCL/SDA — OLED |
| PC13 | Onboard LED (heartbeat) |

## Timing
Two separate timebases on purpose: a microsecond timer (TIM4) for anything commutation-related where precision matters, and a millisecond timer (SysTick/`millis()`) for everything else — arming, telemetry refresh, LED blink.

## Not done yet
- Over-current / over-temperature protection
- Duty-cycle rate limiting during normal run
- Testing on an actual motor with a real gate driver stage

## Disclaimer
Logic-level outputs only — needs a proper gate driver / half-bridge stage before touching a real motor. BEMF threshold is a fixed mid-scale value and will need tuning for your hardware.

## Status
Work in progress. Core control loop, sensing, and telemetry are working on the bench. Protection and real motor testing are next.
