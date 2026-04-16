# FlowOS
An operating system for ESP32-S3.

## Supported languages: `Русский`, `English`

# How to install
> I use "Arduino nano ESP32" in Arduino IDE, but my real board is Waveshare ESP32-S3 nano N16R8. Work on "ESP32-S3 DEV MODULE" may not be impossible but it's worth a try.
1. You need to download [Arduino IDE](https://www.arduino.cc/en/software/).
2. Once you have installed the Arduino IDE, you need to go to the `LIBRARY MANAGER` and install the library `ESPping 1.0.5` by dvarrel, Daniele Colanardi, Marian Crasciunescu.
3. After you have installed the library, you need to download the lastest version of the system in releases and open the `.ino` file in the Arduino IDE
4. Connect your ESP32-S3 and select the board (in my case `Arduino Nano ESP32`) and select the port.
5. Click the button `Upload` in Arduino IDE and wait for the installation to complete.
6. After successful installation, open `Serial Monitor` with baud rate 115200.
## If you want to install binary firmware you need :
1. Install [Flash Download Tool](https://docs.espressif.com/projects/esp-test-tools/en/latest/esp32s3/production_stage/tools/flash_download_tool.html)
2. Download the `*.bin` file from the release.
3. Launch "Flash Download Tool" and select `ChipType : ESP32-S3`, `WorkMode : Develop`, `LoadMode : UART` or else and click `OK`
4. Click on `...` above and select the *.bin file you downloaded.
5. After you have selected the file, there will be a place to the right where you need to enter the address `0x0`.
6. Select the COM port in the bottom right corner and select BAUD : 115200.
7. Click on the check mark on the left where you selected the file and click `START`.
8. Wait for the installation to complete successfully and reconnect the ESP32-S3 (as I usually do).
### If you see ERROR, try disconnecting the ESP32-S3 and connecting the wire to GND and GPIO 0 to enter bootloader mode (I do this on my Waveshare ESP32-S3 Nano) or try holding down the BOOT button and at the same time pressing RESET and then releasing BOOT.
## NOTE : If you want use PuTTY or other terminal, you need enable the "Local echo" and "Local line editing" in settings. Support for these terminals may be unstable at the moment.
