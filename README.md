# Portable LCD Driver Board

STM32-based 2.8-inch SPI display driver board with TF card media playback, 320x240 resolution, 10Hz refresh rate, USB-C &amp; battery powered.

## Project Overview
This project is a compact portable display driver system.
It uses an STM32 MCU to drive a 2.8-inch SPI LCD (320x240), reads images and videos from TF card, and supports dual power supply: USB-C and lithium battery.
The refresh rate can reach up to 10Hz for video playback.
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/driver_board.jpg" alt="driver_board" width="50%">
</div>

## Demo & Real Product
### Real Device
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/display_image.jpg" alt="driver_board" width="50%">
</div>

### Function Demonstration
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/display_video.mp4" alt="driver_board" width="50%">
</div>

Online video: https://www.bilibili.com/video/BV1Br93BaEWx/

## Key Features
- 2.8" SPI LCD, 320×240 resolution，10Hz
- Powered by STM32F401RCT6
- Play image & video from TF (MicroSD) card
- Touchscreen compatible
- USB-C & battery dual power design
- Self-developed PC tool for media-to-RGB565 conversion

## Work Flow
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/work_flow.mp4" alt="driver_board" width="50%">
</div>

## Hardware Preview
### PCB Design
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/PCB_top_layer.png" alt="driver_board" width="50%">
</div>
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/PCB_bottom_layer.png" alt="driver_board" width="50%">
</div>

### Schematic Diagram
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/Schematic.png" alt="driver_board" width="50%">
</div>

## Related Tool
media2rgb565 - Image & video to RGB565 converter
[https://github.com/excezon/media2rgb565](https://github.com/excezon/media2rgb565)
