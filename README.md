# STM32 Bare-Metal ESC Firmware

This repository contains a bare-metal ESC firmware project based on the STM32F103C8T6.

The project is focused on understanding and implementing the firmware side of a sensorless BLDC ESC. Rather than building separate examples for GPIO, timers, PWM, or ADC, these peripherals will be developed as part of the actual ESC firmware.

## Target

- MCU: STM32F103C8T6
- Core: ARM Cortex-M3
- Language: C
- Development approach: Bare-metal and register-level programming

## Project Scope

The firmware will cover the main parts of an ESC, including:

- ESC state management
- PWM generation and duty-cycle control
- Controlled duty-cycle ramping
- Sensorless commutation
- Back-EMF sensing
- Rotor position detection
- RPM calculation
- Current measurement using onboard current-sense circuitry
- Voltage measurement using onboard voltage-sensing circuitry
- Temperature measurement using an onboard NTC or thermistor
- ADC data acquisition
- Telemetry handling
- Fault detection and protection

The goal is to build the firmware as a complete system instead of treating each peripheral as an isolated learning exercise.

## Sensing and Measurement

The project distinguishes between values that are directly measured and values that are calculated from those measurements.

### Back-EMF and RPM

A separate RPM sensor is not required in a sensorless BLDC system.

The motor phases are monitored for back-EMF information. This can be used to determine rotor position and commutation timing. The time between electrical events can then be used to calculate motor speed.

### Current

Current is measured using an onboard current-sensing circuit. Depending on the hardware design, this may include a shunt resistor and analog signal conditioning before the signal reaches the ADC.

The firmware is responsible for reading, converting, filtering, and using this data for control and protection.

### Voltage

Supply voltage is measured through onboard voltage-sensing circuitry, typically using a voltage divider connected to an ADC channel.

The firmware converts the ADC value into the actual supply voltage.

### Temperature

Temperature is measured using an onboard NTC or thermistor connected to an ADC channel.

The firmware converts the measured ADC value into a temperature value and uses it for monitoring and protection.

## PWM Control

PWM is one of the main parts of the ESC firmware.

The control flow is expected to follow this general path:

Control command

↓

Target duty cycle

↓

Duty-cycle ramping or rate limiting

↓

PWM timer update

↓

Power stage control

The duty cycle will not simply jump between arbitrary values. The firmware will implement controlled changes based on the current state and operating conditions.

## Firmware Direction

The firmware will gradually be structured around the following areas:

- Core system initialization
- Clock configuration
- GPIO control
- Timers and PWM
- ADC and signal acquisition
- Interrupt handling
- ESC state machine
- Motor startup
- Commutation control
- Back-EMF processing
- Current, voltage, and temperature processing
- RPM calculation
- Telemetry
- Fault handling and protection

These modules will be added only when they are needed by the firmware.

## Project Goal

The main goal of this project is to build a clean and understandable bare-metal ESC firmware codebase from the ground up.

The project is intended to improve my understanding of embedded firmware, real-time control, STM32 peripherals, and sensorless BLDC motor control while producing a publicly shareable project that demonstrates practical firmware development.

## Status

Work in progress.
