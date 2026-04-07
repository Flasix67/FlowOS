#include <WiFi.h>
#include <ESPping.h>
#include <esp_timer.h>

#define MAX_VARS 64

int CPU_FREQ_SLEEP = 10;
int CPU_FREQ_MIN = 80;
int CPU_FREQ_MID = 160;
int CPU_FREQ_MAX = 240;
bool CPU_OVERHEAT = false;

bool pingRunning = false;
bool pingStopRequested = false;
bool pingStopFlag = false;

void setup() {
  setCpuFrequencyMhz(CPU_FREQ_MAX);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);

  registerVar("CPU_FREQ_SLEEP", CPU_FREQ_SLEEP);
  registerVar("CPU_FREQ_MIN", CPU_FREQ_MIN);
  registerVar("CPU_FREQ_MID", CPU_FREQ_MID);
  registerVar("CPU_FREQ_MAX", CPU_FREQ_MAX);
  registerVar("CPU_OVERHEAT", CPU_OVERHEAT);

  Serial.begin(115200);
  Serial.setTimeout(50);

  delay(3000);

  Serial.println("-------------------------------------");
  Serial.println("|         FlowOS 1.0.2(BETA)        |");
  Serial.println("-------------------------------------");
  Serial.println("(Based on Waveshare ESP32S3 NANO N16R8)");
  Serial.println("======================================");
  Serial.println("Put the command : ");

  digitalWrite(LED_BUILTIN, LOW);
  setCpuFrequencyMhz(CPU_FREQ_MID);
}

struct DynVar {
  char name[32];
  int value;
  bool active;
} dynVars[MAX_VARS];
int varCount = 0;

void registerVar(const char* name, int initialVal) {
  if (varCount < MAX_VARS) {
    strcpy(dynVars[varCount].name, name);
    dynVars[varCount].value = initialVal;
    dynVars[varCount].active = true;
    varCount++;
  }
}

int findVar(const char* name) {
  for (int i = 0; i < varCount; i++) {
    if (dynVars[i].active && strcmp(dynVars[i].name, name) == 0) return i;
  }
  return -1;
}

void printAllVars() {
  Serial.println("=== Dynamic Variables ===");
  if (varCount == 0) {
    Serial.println("(empty)");
  } else {
    for (int i = 0; i < varCount; i++) {
      Serial.printf("  %s = %d\n", dynVars[i].name, dynVars[i].value);
      Serial.println("-------------------------");
    }
  }
}

void runPing(String target, int count, bool continuous) {
  if (!WiFi.isConnected()) {
    Serial.println("Error: WiFi not connected.");
    pingRunning = false;
    return;
  }

  IPAddress ip;
  if (!WiFi.hostByName(target.c_str(), ip)) {
    Serial.printf("Error: Cannot resolve '%s'\n", target.c_str());
    pingRunning = false;
    return;
  }

  Serial.printf("\nPinging %s [%s] with 32 bytes:\n", target.c_str(), ip.toString().c_str());
  
  int success = 0;
  int total = 0;

  while (continuous || total < count) {
    if (pingStopRequested) {
      pingStopRequested = false;
      pingRunning = false;
      Serial.println("\nPing stopped.");
      return;
    }
    if (Serial.available()) {
      String check = Serial.readStringUntil('\n');
      check.trim();
      if (check == "stop") {
        pingStopRequested = true;
        Serial.printf("\nPing statistics for %s:\n", ip.toString().c_str());
        Serial.printf("    Packets: Sent = %d, Received = %d, Lost = %d (%.0f%% loss)\n", 
                        total, success, total-success, (float)(total-success)/total*100);
        continue;
      }
    }
    total++;
    bool result = Ping.ping(ip, 1);
    if (result) {
      float avg_time = Ping.averageTime();
      Serial.printf("Reply from %s: bytes=32 time=%.0fms\n", ip.toString().c_str(), avg_time);
      success++;
    } else {
      Serial.println("Request timed out.");
    }
    delay(1000);
    if (!continuous && total >= count) break;
  }
  pingRunning = false;
  pingStopRequested = false;
  Serial.printf("\nPing statistics for %s:\n", ip.toString().c_str());
  Serial.printf("    Packets: Sent = %d, Received = %d, Lost = %d (%.0f%% loss)\n", 
                total, success, total-success, (float)(total-success)/total*100);
}

void loop() {
  if (temperatureRead() > 80) {
    if (CPU_OVERHEAT == false) {
      Serial.println("CPU is overheated! The frequency will be reduced to 80MHz");
      setCpuFrequencyMhz(CPU_FREQ_MIN);
      CPU_OVERHEAT = true;
      int idx = findVar("CPU_OVERHEAT");
      if (idx != -1) dynVars[idx].value = 1;
    }
  } else if (CPU_OVERHEAT == true) {
    if (temperatureRead() < 71) {
      setCpuFrequencyMhz(CPU_FREQ_MID);
      CPU_OVERHEAT = false;
      int idx = findVar("CPU_OVERHEAT");
      if (idx != -1) dynVars[idx].value = 0;
    }
  }

  if (Serial.available() > 0 ) {
    setCpuFrequencyMhz(CPU_FREQ_MID);
    char termChar = '\n';
    if (Serial.peek() == '\r') termChar = '\r';
    String input = Serial.readStringUntil(termChar);
    input.trim();
    //String input = Serial.readString();
    Serial.print("> ");
    Serial.println(input);
    if (input.length() == 0) return;

    String cmd = input;
    int spacePos = cmd.indexOf(' ');
    if (spacePos != -1) cmd = cmd.substring(0, spacePos);
    cmd.toLowerCase();

    if (input == "reboot") {
      ESP.restart();
    } else if (input == "help") {
      Serial.println("-------------------------");
      Serial.println("|   Available commands  |");
      Serial.println("-------------------------");
      Serial.println("REBOOT - Restart ESP");
      //Serial.println("SET [NAME] [VALUE] - Change the value. You can also enable GPIO by this command");
      Serial.println("INFO - Show the info of CPU/RAM/FLASH/PSRAM/SYSTEM");
      //Serial.println("BLE [ON/OFF/CONNECT/DISCONNECT/AP] - Start BLE modem");
      Serial.println("SLEEP - Puts the CPU into a power-efficient state");
      Serial.println("WIFI [ON/OFF/SCAN/CONNECT/DISCONNECT/AP] - Start WIFI modem");
      Serial.println("PING [OPTIONS] [Host] - Show the ping of host");
      Serial.println("STOP - Stops the process like 'ping -t'");
      //Serial.println("------In development-----");
      Serial.println("SET [NAME/GPIO] [VALUE] - Change the value. You can also enable GPIO by this command");
      Serial.println("CLEAR - Cleaning the screen");
      Serial.println("-------------------------");
    } else if (input == "set" || input.startsWith("set ")) {
      String args = "";
      if (input.length() > 4) args = input.substring(4);
        args.trim();
        if (args.length() == 0) {
          printAllVars();
          return;
        }
        int spaceIdx = args.indexOf(' ');
        String varName = args;
        String varValue = "";
        if (spaceIdx != -1) {
          varName = args.substring(0, spaceIdx);
          varValue = args.substring(spaceIdx + 1);
          varName.trim(); varValue.trim();
        }
        bool isGpio = true;
        for (char c : varName) { if (!isDigit(c)) { isGpio = false; break; } }
        if (isGpio && spaceIdx != -1) {
          int pin = varName.toInt();
          int state = varValue.toInt();
          if (pin >= 0 && pin <= 48) {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, state);
            Serial.printf("GPIO %d -> %s\n", pin, state ? "HIGH" : "LOW");
            return;
          }
        }
        int idx = findVar(varName.c_str());

        if (varValue.length() == 0) {
          if (idx != -1) {
            Serial.printf("%s = %d\n", dynVars[idx].name, dynVars[idx].value);
          } else {
            Serial.printf("Variable '%s' not found. Use 'set %s [VALUE]' to create.\n", varName.c_str(), varName.c_str());
          }
        } else {
          int newVal = 0;
          if (varValue.equalsIgnoreCase("true")) newVal = 1;
          else if (varValue.equalsIgnoreCase("false")) newVal = 0;
          else newVal = varValue.toInt();
          if (idx != -1) {
            dynVars[idx].value = newVal;
            Serial.printf("Updated: %s = %d\n", dynVars[idx].name, newVal);

            if (strcmp(dynVars[idx].name, "CPU_FREQ_SLEEP") == 0) CPU_FREQ_SLEEP = newVal;
            else if (strcmp(dynVars[idx].name, "CPU_FREQ_MIN") == 0) CPU_FREQ_MIN = newVal;
            else if (strcmp(dynVars[idx].name, "CPU_FREQ_MID") == 0) CPU_FREQ_MID = newVal;
            else if (strcmp(dynVars[idx].name, "CPU_FREQ_MAX") == 0) CPU_FREQ_MAX = newVal;
            else if (strcmp(dynVars[idx].name, "CPU_OVERHEAT") == 0) {
            CPU_OVERHEAT = (newVal != 0);
            Serial.printf("%s\n", CPU_OVERHEAT ? "true" : "false");
          }
          } else {
            if (varCount >= MAX_VARS) {
              Serial.println("Error: Variable table full.");
            } else {
              varName.toCharArray(dynVars[varCount].name, 32);
              dynVars[varCount].value = newVal;
              dynVars[varCount].active = true;
              Serial.printf("Created: %s = %d\n", dynVars[varCount].name, newVal);
              varCount++;
            }
          }
        }
      } else if (input == "info") {
      //digitalWrite(LED_BUILTIN, HIGH);
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
      Serial.println("FlowOS 1.0.2 BETA by Flasix67");
      Serial.printf("ESP-IDF Version: %s\n", ESP.getSdkVersion());
      Serial.println("----------------------------------------------");
      //digitalWrite(LED_BUILTIN, LOW);
    } else if (input == "sleep") {
      Serial.println("CPU has been send to sleep");
      Serial.println("If you send a command via the Serial Monitor, the CPU will exit sleep mode.");
      delay(1000);
      setCpuFrequencyMhz(CPU_FREQ_SLEEP);
    } else if (input.startsWith("wifi ")) {
      String sub = input.substring(5);
      sub.trim();
      if (sub == "scan") {
        digitalWrite(LED_GREEN, LOW);
        Serial.println(F("WiFi: Scanning..."));
        int n = WiFi.scanNetworks();
        if (n == 0) {
          Serial.println(F("No networks found."));
        } else {
          Serial.printf("Found %d networks:\n", n);
          for (int i = 0; i < n; ++i) {
            Serial.printf("%d: %s (%d dBm)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
          }
        }
        WiFi.scanDelete();
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
        digitalWrite(LED_GREEN, HIGH);
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
    } else if (input.startsWith("ping")) {
      String args = input.substring(5);
      args.trim();
      
      String target = "";
      int count = 4;
      bool continuous = false;
      
      int idx = 0;
      while (idx < args.length()) {
        while (idx < args.length() && args[idx] == ' ') idx++;
        if (idx >= args.length()) break;
        
        int start = idx;
        while (idx < args.length() && args[idx] != ' ') idx++;
        String token = args.substring(start, idx);
        
        if (token.startsWith("-")) {
          if (token == "-n" && idx < args.length()) {
            while (idx < args.length() && args[idx] == ' ') idx++;
            start = idx;
            while (idx < args.length() && args[idx] != ' ') idx++;
            count = args.substring(start, idx).toInt();
          }
          else if (token == "-t") {
            continuous = true;
            count = 999999;
            //Serial.println("(Continuous ping. Type 'stop' to cancel)");
          }
          else if (token == "-w" || token == "-i" || token == "-l") {
            Serial.printf("(Note: %s not supported by ESPping library)\n", token.c_str());
          }
        }
        else if (target.isEmpty()) {
          target = token;
        }
      }
      
      if (target.isEmpty()) {
        Serial.println("Usage: ping [options] <host>");
        Serial.println("Examples: ping 8.8.8.8 | ping -t google.com | ping -n 10 192.168.1.1");
      }
      else if (pingRunning) {
        Serial.println("Error: Ping already running. Use 'stop' first.");
      }
      else {
        pingRunning = true;
        pingStopFlag = false;
        runPing(target, count, continuous);
      }
      
    } else if (input == "stop") {
      if (pingRunning) {
        pingStopRequested = true;
        Serial.println("Stopping ping...");
        delay(50);
      } else {
        Serial.println("No active task to stop");
      }
    } else if (input == "clear") {
      Serial.print("\033[2J\033[H");
      for (int i = 0; i < 40; i++) Serial.println();
    } else {
      Serial.println("Invalid command!");
    }
  }
}
