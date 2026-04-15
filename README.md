# FlowOS
An operating system for ESP32-S3.

## Supported languages: `Russian`, `English`

# How to install
> I use "Arduino nano ESP32" in Arduino IDE, but my real board is Waveshare ESP32-S3 nano N16R8. Work on "ESP32-S3 DEV MODULE" may not be impossible but it's worth a try.
1. You need to download [Arduino IDE](https://www.arduino.cc/en/software/).
2. Once you have installed the Arduino IDE, you need to go to the `LIBRARY MANAGER` and install the library `ESPping 1.0.5` by dvarrel, Daniele Colanardi, Marian Crasciunescu.
3. After you have installed the library, you need to download the lastest version of the system in releases and open the `.ino` file in the Arduino IDE
4. Connect your ESP32-S3 and select the board (in my case `Arduino Nano ESP32`) and select the port.
5. Click the button `Upload` in Arduino IDE and wait for the installation to complete.
6. After successful installation, open `Serial Monitor` with baud rate 115200.

### If you want use PuTTY or other terminal, you need enable the "Local echo" and "Local line editing" in settings. Support for these terminals may be unstable at the moment.
