# Portable LCD Driver Board

A compact STM32-based portable LCD driver board designed for embedded display and media playback applications.  
It drives a 2.8-inch SPI LCD with 320×240 resolution, reads image and video data from a TF card, and supports smooth playback at up to 10Hz refresh rate.  
The device is powered by either USB-C or a lithium battery, making it suitable for portable display scenarios and embedded system demonstrations.

## Real Device
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/assets/driver_board.jpg" alt="driver_board" width="50%">
</div>

<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/assets/display_image.jpg" alt="display_image" width="50%">
</div>

## Function Demonstration
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/assets/display_video.mp4" alt="display_video" width="50%">
</div>

Online video: https://www.bilibili.com/video/BV1Br93BaEWx/

## Key Features
- Real-time image display on a **2.8-inch SPI LCD**
- **320×240** resolution output
- Up to **10Hz refresh rate** for video playback
- **TF (MicroSD) card** image and video loading
- Based on **STM32F401RCT6**
- **Touchscreen-compatible** hardware interface
- **USB-C + battery** dual power supply
- Self-developed **media-to-RGB565 conversion tool**

## System Behavior
### Media Preparation
Before playback, the media files:
1. are converted into RGB565 format using a custom PC-side conversion tool
2. are stored on a TF card for embedded access
3. are organized for image display or frame-by-frame video playback

### Real-Time Display
After power-up, the device:
1. initializes the STM32 MCU and LCD driver
2. mounts the TF card storage
3. reads image or video frame data from the card
4. transfers display data through the SPI interface
5. updates the LCD in real time at up to 10Hz

### Display Capability
- **Static image display** from TF card
- **Video playback** through sequential frame loading
- Smooth embedded visual output on a compact LCD module

## Workflow
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/assets/Work_flow.png" alt="Work_flow" width="100%">
</div>

## Hardware Preview
### PCB Design
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/assets/PCB_top_layer.png" alt="PCB_top_layer" width="50%">
</div>

<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/assets/PCB_bottom_layer.png" alt="PCB_bottom_layer" width="50%">
</div>

### Schematic Diagram
<div align="center">
<img src="https://github.com/excezon/portable-lcd-driver-board/raw/main/assets/Schematic.png" alt="Schematic" width="50%">
</div>

## Project Value
This project highlights several notable aspects of embedded system design, including:
- STM32 embedded firmware development
- SPI LCD driver implementation
- TF card file access and media streaming
- RGB565 image/video processing pipeline
- portable power system design
- PCB and schematic design
- end-to-end embedded display system development

## Related Tool
### media2rgb565
Image & video to RGB565 converter  
[https://github.com/excezon/media2rgb565](https://github.com/excezon/media2rgb565)

## Suitable Application Scenarios
- Portable embedded display terminal
- STM32 multimedia demonstration platform
- Image/video playback interface for custom devices
- Embedded systems portfolio project
- Small-screen HMI development reference
