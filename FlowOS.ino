#include <BLEDevice.h>
#include <WiFi.h>
//#include <BLEServer.h>
//#include <BLEUtils.h>
//#include <BLE2902.h>
#define CPU_FREQ_SLEEP 10
#define CPU_FREQ_MIN 80
#define CPU_FREQ_MID 160
#define CPU_FREQ_MAX 240

bool CPU_OVERHEAT = false;
bool bleEnabled = false;
BLEServer* pServer = NULL;

void setup() {
  setCpuFrequencyMhz(CPU_FREQ_MAX);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  Serial.setTimeout(50);

  delay(3000);

  Serial.println("-------------------------------------");
  Serial.println("|          FlowOS 1.0(ALPHA)        |");
  Serial.println("-------------------------------------");
  Serial.println("(Based on Waveshare ESP32S3 NANO N16R8)");
  Serial.println("======================================");
  Serial.println("Put the command : ");

  digitalWrite(LED_BUILTIN, LOW);
  setCpuFrequencyMhz(CPU_FREQ_MID);
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("\nBLE: Connected");
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("\nBLE: Disconnected");
      pServer->getAdvertising()->start();
    }
};


void loop() {
  if (temperatureRead() > 80) {
    Serial.println("CPU is overheated! The frequency will be reduced to 80MHz");
    setCpuFrequencyMhz(CPU_FREQ_MIN);
    CPU_OVERHEAT = true;
  } else {
    setCpuFrequencyMhz(CPU_FREQ_MID);
    CPU_OVERHEAT = false;
  }


  if (Serial.available() > 0 ) {
    String input = Serial.readString();
    Serial.print("> ");
    Serial.println(input);
    input.trim();
    input.toLowerCase();

    if (input == "reboot") {
      ESP.restart();
    } else if (input == "help") {
      Serial.println("-------------------------");
      Serial.println("|   Available commands  |");
      Serial.println("-------------------------");
      Serial.println("REBOOT - Restart ESP");
      Serial.println("SET [NAME] [VALUE] - Change the value. You can also enable GPIO by this command");
      Serial.println("INFO - Show the info of CPU/RAM/FLASH/PSRAM/SYSTEM");
      Serial.println("BLE [ON/OFF/CONNECT/DISCONNECT/AP] - Start BLE modem");
      Serial.println("SLEEP - Puts the CPU into a power-efficient state");
      Serial.println("WIFI [ON/OFF/SCAN/CONNECT/DISCONNECT/AP] - Start WIFI modem");
    } else if (input.startsWith("set ")) {
      String args = input.substring(4);
      args.trim();
      int spaceIndex = args.indexOf(' ');
      
      if (spaceIndex != -1) {
        String name = args.substring(0, spaceIndex);
        String value = args.substring(spaceIndex + 1);
        name.trim();
        value.trim();
        
        int pin = name.toInt();
        int state = value.toInt();
        
        pinMode(pin, OUTPUT);
        digitalWrite(pin, state);
        
        Serial.print("GPIO ");
        Serial.print(pin);
        Serial.println(state == 1 ? " ENABLED (HIGH)" : " DISABLED (LOW)");
      } else {
        Serial.println("Error: Usage 'set [pin] [0/1]'");
      }
    } else if (input == "info") {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("|      INFO       |");
      Serial.println("--------------------CPU-----------------------");
      Serial.printf("Chip Model: %s (Cores: %d)\n", ESP.getChipModel(), ESP.getChipCores());
      Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
      Serial.printf("Temperature: %.1f C\n", temperatureRead());
      Serial.println("--------------------RAM-----------------------");
      Serial.printf("Free Heap: %d KB\n", ESP.getFreeHeap() / 1024);
      Serial.printf("Total Heap: %d KB\n", ESP.getHeapSize() / 1024);
      Serial.println("-------------------FLASH----------------------");
      Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
      Serial.printf("Flash Speed: %d MHz\n", ESP.getFlashChipSpeed() / 1000000);
      Serial.println("-------------------PSRAM----------------------");
      Serial.printf("PSRAM Size: %d KB\n", ESP.getPsramSize() / 1024);
      Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);
      Serial.println("-------------------SYSTEM---------------------");
      Serial.println("FlowOS 1.0 ALPHA by Flasix");
      Serial.printf("ESP-IDF Version: %s\n", ESP.getSdkVersion());
      Serial.println("----------------------------------------------");
      digitalWrite(LED_BUILTIN, LOW);
    } else if (input == "sleep") {
      Serial.println("CPU has been send to sleep");
      Serial.println("If you send a command via the Serial Monitor, the CPU will exit sleep mode.");
      if (bleEnabled == true) {
        setCpuFrequencyMhz(CPU_FREQ_MIN);
      } else {
        setCpuFrequencyMhz(CPU_FREQ_SLEEP);
      }
    } else if (input.startsWith("ble ")) {
      //digitalWrite(LED_BUILTIN, HIGH);
      String sub = input.substring(4);
      sub.trim();
      if (sub == "on") {
        if (!bleEnabled) {
          BLEDevice::init("FlowOS_S3");
          pServer = BLEDevice::createServer();
          BLEService *pService = pServer->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
          pService->start();
          
          bleEnabled = true;
          Serial.println("BLE: ON");
          digitalWrite(LED_BLUE, LOW);
        }
      } else if (sub == "ap") {
        if (bleEnabled && pServer) {
          BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
          pAdvertising->addServiceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
          pAdvertising->setScanResponse(true);
          pAdvertising->setMinPreferred(0x06);  
          BLEDevice::startAdvertising(); 
          Serial.println("BLE: AP ON");
        }
      } else if (sub == "connect") {
        if (bleEnabled && pServer && pServer->getConnectedCount() > 0) {
          Serial.printf("BLE: Currently connected devices: %d\n", pServer->getConnectedCount());
        } else {
          Serial.println("BLE: Invalid connection. Use 'ble ap' and try again");
        }
      } else if (sub == "disconnect") {
        if (bleEnabled && pServer) {
          pServer->getAdvertising()->stop();
          Serial.println("BLE: Disconnected");
        }
      } else if (sub == "scan") {
        if (bleEnabled) {
          Serial.println("BLE: Scanning(5 sec)...");
          BLEScan* pBLEScan = BLEDevice::getScan();
          pBLEScan->setActiveScan(true);
          //pBLEScan->setInterval(100);
          //pBLEScan->setWindow(99);
          BLEScanResults foundDevices = pBLEScan->start(5, false);
          
          Serial.printf("Found: %d\n", foundDevices.getCount());

          for (int i = 0; i < foundDevices.getCount(); i++) {
            BLEAdvertisedDevice device = foundDevices.getDevice(i);
            
            Serial.print("["); Serial.print(i); Serial.print("] ");
            Serial.print("RSSI: "); Serial.print(device.getRSSI()); // Уровень сигнала
            Serial.print(" | Addr: "); Serial.print(device.getAddress().toString().c_str());
            
            if (device.haveName()) {
              Serial.print(" | Name: ");
              Serial.print(device.getName().c_str());
            }
            Serial.println();
          }

          pBLEScan->clearResults();
          Serial.println("BLE: Scan DONE");
        } else {
          Serial.println("Error: First, turn BLE via 'ble on'");
        }
        //digitalWrite(LED_BUILTIN, LOW);
      } else if (sub == "off") {
        if (bleEnabled) {
          BLEDevice::deinit(false);
          bleEnabled = false;
          
          Serial.println("BLE: OFF");
          digitalWrite(LED_BLUE, HIGH);
        } else {
          Serial.println("BLE: Already OFF");
        }
      }
    } else if (input.startsWith("wifi ")) {
      String sub = input.substring(5);
      sub.trim();

      // --- СКАНЕР СЕТЕЙ ---
      if (sub == "scan") {
        Serial.println(F("WiFi: Scanning..."));
        int n = WiFi.scanNetworks();
        if (n == 0) {
          Serial.println(F("No networks found."));
        } else {
          Serial.printf("Found %d networks:\n", n);
          for (int i = 0; i < n; ++i) {
            Serial.printf("%d: %s (%d dBm)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
            digitalWrite(LED_GREEN, LOW);
          }
        }
        WiFi.scanDelete(); // Очистка памяти после скана
      }
      else if (sub.startsWith("connect ")) {
        String auth = sub.substring(8);
        int space = auth.indexOf(' ');
        if (space != -1) {
          String ssid = auth.substring(0, space);
          String pass = auth.substring(space + 1);
          
          Serial.printf("Connecting to %s...\n", ssid.c_str());
          WiFi.begin(ssid.c_str(), pass.c_str());
          int timer = 0;
          while (WiFi.status() != WL_CONNECTED && timer < 20) {
            delay(500);
            Serial.print(".");
            timer++;
          }

          if (WiFi.status() == WL_CONNECTED) {
            Serial.println(F("\nConnected!"));
            Serial.print(F("IP: ")); Serial.println(WiFi.localIP());
            digitalWrite(LED_GREEN, LOW);
          } else {
            Serial.println(F("\nFailed. Check SSID/Pass."));
          }
        }
      } else if (sub == "off") {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        Serial.println(F("WiFi: OFF"));
        digitalWrite(LED_GREEN, LOW);
      } else if (sub == "on") {
        WiFi.mode(WIFI_STA);
        Serial.println(F("WiFi: ON"));
        Serial.print(F("MAC Address: "));
        Serial.println(WiFi.macAddress());
        digitalWrite(LED_GREEN, LOW);
      } else if (sub.startsWith("ap ")) {
        String args = sub.substring(3);
        args.trim();

        int spaceIndex = args.indexOf(' ');

        if (spaceIndex != -1) {
          String userSSID = args.substring(0, spaceIndex);
          String userPASS = args.substring(spaceIndex + 1);
          userSSID.trim();
          userPASS.trim();
          if (userPASS.length() < 8) {
            Serial.println(F("Error: Password must be at least 8 characters!"));
          } else {
            WiFi.mode(WIFI_AP);
            digitalWrite(LED_GREEN, LOW);
            if (WiFi.softAP(userSSID.c_str(), userPASS.c_str())) {
              Serial.println(F("-------------------------------------"));
              Serial.println(F("          WiFi: AP ON                "));
              Serial.print(F("SSID: ")); Serial.println(userSSID);
              Serial.print(F("PASS: ")); Serial.println(userPASS);
              Serial.print(F("IP Address: ")); Serial.println(WiFi.softAPIP());
              Serial.println(F("-------------------------------------"));
            } else {
              Serial.println(F("WiFi: Fatal Error during AP start."));
            }
          }
        } else {
          Serial.println(F("Usage: wifi ap [SSID] [PASSWORD]"));
        }
      }
    } 

    
    
     else {
      Serial.println("Invalid command!");
    }
  }
}
