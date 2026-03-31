# Portable LCD Driver Board

STM32-based 2.8-inch SPI display driver board with TF card media playback, 320x240 resolution, 10Hz refresh rate, USB-C &amp; battery powered.

## Project Overview
This project is a compact portable display driver system.
It uses an STM32 MCU to drive a 2.8-inch SPI LCD (320x240), reads images and videos from TF card, and supports dual power supply: USB-C and lithium battery.
The refresh rate can reach up to 10Hz for video playback.

## Demo & Real Product
### Real Device
![Device Real Shot](images/real_device.jpg)

### Function Demonstration
[![Demo Video](images/video_cover.jpg)](https://www.bilibili.com/你的视频链接)

## Key Features
- 2.8" SPI LCD, 320×240 resolution
- STM32 embedded driver development
- TF (MicroSD) card media playback
- USB-C & battery dual power design
- Custom PCB hardware design
- Self-developed PC tool for media-to-RGB565 conversion

## Hardware Preview
### PCB Design
![PCB Top View](images/pcb_top.jpg)
![PCB Bottom View](images/pcb_bottom.jpg)

### Schematic Diagram
![Schematic](images/schematic.jpg)

## Technical Stack
- Hardware: STM32, SPI LCD, TF Card, USB-C, Li-ion power
- Embedded: C language, Keil MDK, LCD driver, FATFS file system
- PC Tool: Python, image/video processing, RGB565 format conversion
- PCB Design: LCSC EDA, schematic & layout design

## Project Structure (Brief)
- Firmware: STM32 driver for LCD & TF card playback
- Hardware: Custom 2-layer PCB design
- Tool: Python and keil

## Related Tool
media2rgb565 - Image & video to RGB565 converter
[https://github.com/excezon/media2rgb565](https://github.com/excezon/media2rgb565)
