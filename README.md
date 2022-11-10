# _Audio Signal Analyzer for ESP32-S3_

## About

ASD

## How to use this project in VS Code with Espressif IDF extension
1. Clone this repository 
2. Initianlize and update the ESP-DSP submodule:
    1. Run "git submodule init" git command
    2. Run "git submodule update" git command
3. Run the following command from the vscode command pallette (Ctrl-Shift-P):
    1. ESP-IDF: Add vscode configuration folder
4. Use the Espressif IDF extension bar at the buttom of VS Code to build and flash. Monitoring via the "USB-to-UART" port on the ESP32-S3-DevKitC-1 is not possible for this project since UART0 is repurposed for ECEP (a custom command event protocol). For monitoring / debug-console please use GPIO ASD(RX) and GPIO ASD(TX).
 
(An alternative to step 1 and 2 is to pass --recurse-submodules to the git clone command. This will automatically initialize and update each submodule in the repository)

## Required Hardware
This project is created for the ESP32-S3 \
(ESP32-S3-DEVKITC-1-N8R8 was used in the development) \
(The used ESP-S3 module has to have PSRAM. This project is configured to use Octal SPI PSRAM)

## List of menuconfig changes:
- CPU frequency:      240MHz (default: 160MHz) \

- Maximum FFT length: 4096   (default: 4096) \

- Bootloader log verbosity: No output (default: info) \
- Permanently change Boot ROM output: Permanently disable logging (default: Always log) \

- Channel for console output: Custom UART (default: UART0) \
- UART peripheral to use for console output (0-1): UART1 (default: UART0) \
- UART TX on GPIO: 10 (default: 43) \
- UART RX on GPIO: 11 (default: 44) \ 
- UART console baud rate: 230400 (default: 115200) (this value for some reason needs to be double of the wished baud. idk why. aka the actual baud --> 230400/2 = 115200)\

- Support for external, SPI-connected RAM: Enable (default: Disabled) (Ops: requires ESP32 module with PSRAM) \
- Mode (QUAD/OCT) of SPI RAM chip in use: Octal Mode PSRAM (Default: Quad mode PSRAM) (Ops: requires ESP32 module with support for Octal mode SPI PSRAM) \
- Set RAM clock speed: 80MHz clock speed (default: 40MHz clock speed) \
- SPI RAM access method: "Make RAM allocatable using heap_caps_malloc(..., MALLOC_CAP_SPIRAM)" (default: "Make RAM allocatable using malloc() as well") \

## Used GPIO's
- IO10: UART1 TX
- IO11: UART1 RX
- IO30: PSRAM CLK IO number (The default. Can be changed in menuconfig if needed) \
- IO26: PSRAM CS IO number (The default. Can be changed in menuconfig if needed) \

!LIST NOT COMPLETE!

## ESP-IDF Version
This project is created on ESP-IDF 5.0

## EspressIf documentation
ESP32-S3 Programming guide:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/index.html

ESP32-S3 Technical Reference Manual:
- https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf

ESP32-S3 Datasheet:
- https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf 

ESP32-S3-DevKitC-1 page:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1-v1.0.html 

ESP-IDF Github:
- https://github.com/espressif/esp-idf

ESP-DSP (DSP library) Github:
- https://github.com/espressif/esp-dsp 

### Related documents

ESP32-S3 Hardware Design Guidelines:
- https://www.espressif.com/sites/default/files/documentation/esp32-s3_hardware_design_guidelines_en.pdf 

ESP32-S3-WROOM-1 & ESP32-S3-WROOM-1U Datasheet:
- https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf 

ESP32-S3-WROOM-2 Datasheet:
- https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-2_datasheet_en.pdf 
