# Portable LCD Driver Board

STM32-based 2.8-inch SPI display driver board with TF card media playback, 320x240 resolution, 10Hz refresh rate, USB-C &amp; battery powered.

## Project Overview
This project is a compact portable display driver system.
It uses an STM32 MCU to drive a 2.8-inch SPI LCD (320x240), reads images and videos from TF card, and supports dual power supply: USB-C and lithium battery.
The refresh rate can reach up to 10Hz for video playback.

## Demo & Real Product
### Real Device
![Device Real Shot](https://github.com/excezon/portable-lcd-driver-board/raw/main/display_image.jpg)

### Function Demonstration
[![Demo Video](images/video_cover.jpg)](https://github.com/excezon/portable-lcd-driver-board/raw/main/display_video.mp4)

Online video: https://www.bilibili.com/video/BV1Br93BaEWx/

## Key Features
- 2.8" SPI LCD, 320×240 resolution，10Hz
- Powered by STM32F401RCT6
- Play image & video from TF (MicroSD) card
- Touchscreen compatible
- USB-C & battery dual power design
- Self-developed PC tool for media-to-RGB565 conversion

## Hardware Preview
### PCB Design
![PCB Top View](https://github.com/excezon/portable-lcd-driver-board/raw/main/PCB_bottom_layer.png)
![PCB Bottom View](https://github.com/excezon/portable-lcd-driver-board/raw/main/PCB_top_layer.png)
### Schematic Diagram
![Schematic](https://github.com/excezon/portable-lcd-driver-board/raw/main/Schematic.png)

## Related Tool
media2rgb565 - Image & video to RGB565 converter
[https://github.com/excezon/media2rgb565](https://github.com/excezon/media2rgb565)
