#include <WiFi.h>
#include <ESPping.h>
#include <esp_timer.h>

#define MAX_VARS 64
String SYS_VER = "1.1-RC3";
String DEVELOPER = "Flasix67";
String OS_NAME = "FlowOS";

uint32_t systemStartTime = 0;

int CPU_FREQ_SLEEP = 10;
int CPU_FREQ_MIN = 80;
int CPU_FREQ_MID = 160;
int CPU_FREQ_MAX = 240;
bool CPU_OVERHEAT = false;
float CPU_TEMP = 0;

bool pingRunning = false;
bool pingStopRequested = false;

void setup() {
  systemStartTime = millis();
  setCpuFrequencyMhz(CPU_FREQ_MAX);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);

  CPU_TEMP = temperatureRead();
  registerVar("CPU_FREQ_SLEEP", "10");
  registerVar("CPU_FREQ_MIN", "80");
  registerVar("CPU_FREQ_MID", "160");
  registerVar("CPU_FREQ_MAX", "240");
  registerVar("CPU_OVERHEAT", "0");
  registerVar("SYS_VER", SYS_VER.c_str());
  registerVar("DEVELOPER", DEVELOPER.c_str());
  registerVar("OS_NAME", OS_NAME.c_str());
  registerVar("CPU_TEMP", String(CPU_TEMP, 1).c_str());

  Serial.begin(115200);
  Serial.setTimeout(50);

  delay(3000);

  Serial.println("-------------------------------------");
  Serial.printf("|          %s %s           |\n", OS_NAME, SYS_VER);
  Serial.println("-------------------------------------");
  Serial.println("(Based on Waveshare ESP32S3 NANO N16R8)");
  Serial.println("======================================");
  Serial.println("Put the command : ");

  digitalWrite(LED_BUILTIN, LOW);
  setCpuFrequencyMhz(CPU_FREQ_MID);
}

String getUptimePrecise() {
  uint32_t totalMs = millis() - systemStartTime;
  uint32_t ms = totalMs % 1000;
  uint32_t totalSec = totalMs / 1000;
  uint32_t h = totalSec / 3600;
  uint32_t m = (totalSec % 3600) / 60;
  uint32_t s = totalSec % 60;
  
  char buf[15];
  sprintf(buf, "%02u:%02u:%02u.%03u", h, m, s, ms);
  return String(buf);
}

struct DynVar {
  char name[32];
  char value[64];
  bool active;
} dynVars[MAX_VARS];
int varCount = 0;

void registerVar(const char* name, const char* initialVal) {
  if (strlen(name) >= 32) {
    Serial.printf("Error: Variable name '%s' is too long!\n", name);
    return;
  }
  if (varCount >= MAX_VARS) {
    Serial.println("Error: Variable table full.");
    return;
  }
  strcpy(dynVars[varCount].name, name);
  strncpy(dynVars[varCount].value, initialVal, 63);
  dynVars[varCount].value[63] = '\0';
  dynVars[varCount].active = true;
  varCount++;
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
      Serial.printf("  %s = %s\n", dynVars[i].name, dynVars[i].value);
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
    if (Serial.available()) {
      String check = Serial.readStringUntil('\n');
      check.trim();
      check.toLowerCase();
      if (check == "stop") {
        break;
      }
    }

    if (pingStopRequested) {
      break;
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
  }
  pingRunning = false;
  pingStopRequested = false;
  Serial.printf("\nPing statistics for %s:\n", ip.toString().c_str());
  Serial.printf("    Packets: Sent = %d, Received = %d, Lost = %d (%.0f%% loss)\n", 
                total, success, total-success, (float)(total-success)/total*100);
}

String formatUptime(uint32_t ms) {
  uint32_t sec = ms / 1000;
  uint32_t min = sec / 60;
  uint32_t hr  = min / 60;
  uint32_t day = hr / 24;
  
  char buf[32];
  if (day > 0) {
    sprintf(buf, "%dd %dh %dm %ds", day, hr % 24, min % 60, sec % 60);
  } else if (hr > 0) {
    sprintf(buf, "%dh %dm %ds", hr, min % 60, sec % 60);
  } else if (min > 0) {
    sprintf(buf, "%dm %ds", min, sec % 60);
  } else {
    sprintf(buf, "%ds", sec);
  }
  return String(buf);
}

void loop() {
  CPU_TEMP = temperatureRead();
  int idx = findVar("CPU_TEMP");
  if (idx != -1) {
    snprintf(dynVars[idx].value, 64, "%.1f", CPU_TEMP);
  }

  if (CPU_TEMP > 80) {
    if (CPU_OVERHEAT == false) {
      Serial.println("CPU is overheated! The frequency will be reduced to 80MHz");
      setCpuFrequencyMhz(CPU_FREQ_MIN);
      CPU_OVERHEAT = true;
      int idx = findVar("CPU_OVERHEAT");
      if (idx != -1) strcpy(dynVars[idx].value, "1");
    }
  } else if (CPU_OVERHEAT == true) {
    if (CPU_TEMP < 71) {
      setCpuFrequencyMhz(CPU_FREQ_MID);
      CPU_OVERHEAT = false;
      int idx = findVar("CPU_OVERHEAT");
      if (idx != -1) strcpy(dynVars[idx].value, "0");
    }
  }

  if (Serial.available() > 0 ) {
    setCpuFrequencyMhz(CPU_FREQ_MID);
    char termChar = '\n';
    if (Serial.peek() == '\r') termChar = '\r';
    String input = Serial.readStringUntil(termChar);
    input.trim();
    Serial.printf("[%s] > ", getUptimePrecise().c_str());
    Serial.println(input);
    if (input.length() == 0) return;

    String cmd = input;
    int spacePos = cmd.indexOf(' ');
    if (spacePos != -1) cmd = cmd.substring(0, spacePos);
    cmd.toLowerCase();

    if (cmd == "reboot") {
      ESP.restart();
    } else if (cmd == "help") {
      Serial.println("-------------------------");
      Serial.println("|   Available commands  |");
      Serial.println("-------------------------");
      Serial.println("REBOOT - Restart ESP");
      Serial.println("INFO - Show the info of CPU/RAM/FLASH/PSRAM/SYSTEM");
      Serial.println("SLEEP - Puts the CPU into a power-efficient state");
      Serial.println("WIFI [ON/OFF/SCAN/CONNECT/DISCONNECT/AP] - Start WIFI modem");
      Serial.println("PING [OPTIONS] [Host] - Shows the ping of host");
      Serial.println("STOP - Stops the process like 'ping -t'");
      Serial.println("SET [NAME/GPIO] [VALUE] - Create or change the variable. You can also enable GPIO by this command");
      Serial.println("CLEAR - Cleaning the screen");
      Serial.println("ECHO [TEXT] - Prints text to serial monitor. Use %VAR% for variables in text.");
      Serial.println("UNSET [NAME] - Removes a variable");
      Serial.println("-------------------------");
    } else if (cmd == "set") {
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
            Serial.printf("%s = %s\n", dynVars[idx].name, dynVars[idx].value);
          } else {
            Serial.printf("Variable '%s' not found. Use 'set %s [VALUE]' to create.\n", varName.c_str(), varName.c_str());
          }
        } else {
          int newVal = 0;
          if (varValue.equalsIgnoreCase("true")) newVal = 1;
          else if (varValue.equalsIgnoreCase("false")) newVal = 0;
          else newVal = varValue.toInt();
          if (idx != -1) {
            strncpy(dynVars[idx].value, varValue.c_str(), 63);
            dynVars[idx].value[63] = '\0';
            Serial.printf("Updated: %s = %s\n", dynVars[idx].name, dynVars[idx].value);

            if (strcmp(dynVars[idx].name, "CPU_FREQ_SLEEP") == 0) CPU_FREQ_SLEEP = varValue.toInt();
            else if (strcmp(dynVars[idx].name, "CPU_FREQ_MIN") == 0) CPU_FREQ_MIN = varValue.toInt();
            else if (strcmp(dynVars[idx].name, "CPU_FREQ_MID") == 0) CPU_FREQ_MID = varValue.toInt();
            else if (strcmp(dynVars[idx].name, "CPU_FREQ_MAX") == 0) CPU_FREQ_MAX = varValue.toInt();
            else if (strcmp(dynVars[idx].name, "CPU_OVERHEAT") == 0) {
            CPU_OVERHEAT = (varValue == "1" || varValue.equalsIgnoreCase("true"));
            Serial.printf("%s\n", CPU_OVERHEAT ? "true" : "false");
          }
            if (strcmp(dynVars[idx].name, "DEVELOPER") == 0) DEVELOPER = varValue;
            else if (strcmp(dynVars[idx].name, "SYS_VER") == 0) SYS_VER = varValue;
            else if (strcmp(dynVars[idx].name, "OS_NAME") == 0) OS_NAME = varValue;
            else if (strcmp(dynVars[idx].name, "CPU_TEMP") == 0) CPU_TEMP = varValue.toFloat();
            } else {
              if (varCount >= MAX_VARS) {
                Serial.println("Error: Variable table full.");
                return;
              }
                strncpy(dynVars[varCount].name, varName.c_str(), 31);
                dynVars[varCount].name[31] = '\0';
                strncpy(dynVars[varCount].value, varValue.c_str(), 63);
                dynVars[varCount].value[63] = '\0';
                dynVars[varCount].active = true;
                Serial.printf("Created: %s = %s\n", dynVars[varCount].name, dynVars[varCount].value);
                varCount++;
            }
          }
    } else if (cmd == "ping") {
      String args = "";
      if (input.length() > 4) {
        args = input.substring(4);
        args.trim();
      }
      
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
          if (token == "-n") {
            while (idx < args.length() && args[idx] == ' ') idx++;
            if (idx >= args.length()) {
              Serial.println("Error: Option -n requires a value.");
              return;
            }
            start = idx;
            while (idx < args.length() && args[idx] != ' ') idx++;
            String numStr = args.substring(start, idx);
            
            bool isNum = true;
            for (char c : numStr) {
              if (!isDigit(c)) { isNum = false; break; }
            }
            if (!isNum) {
              Serial.println("Error: -n parameter must contain a number.");
              return;
            }
            
            long parsed = numStr.toInt();
            if (parsed < 1) {
              Serial.println("Invalid value for parameter -n, minimum allowed range is 1.");
              return;
            }
            count = parsed;
          }
          else if (token == "-t") {
            continuous = true;
            count = 999999;
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
        Serial.println("Examples: ping 8.8.8.8 | ping -t example.com | ping -n 10 example.com");
      }
      else if (pingRunning) {
        Serial.println("Error: Ping already running. Use 'stop' first.");
      }
      else {
        pingRunning = true;
        runPing(target, count, continuous);
      }
    } else if (cmd == "info") {
      Serial.println("|      INFO       |");
      Serial.println("--------------------CPU-----------------------");
      Serial.printf("Chip Model: %s (Cores: %d)\n", ESP.getChipModel(), ESP.getChipCores());
      Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
      Serial.printf("Temperature: %.1f C\n", CPU_TEMP);
      Serial.printf("Chip revision: %d\n", ESP.getChipRevision());
      Serial.println("--------------------RAM-----------------------");
      Serial.printf("Free Heap: %d KB\n", ESP.getFreeHeap() / 1024);
      Serial.printf("Heap Used:  %d KB\n", (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024);
      Serial.printf("Total Heap: %d KB\n", ESP.getHeapSize() / 1024);
      Serial.println("-------------------FLASH----------------------");
      Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
      Serial.printf("Flash Speed: %d MHz\n", ESP.getFlashChipSpeed() / 1000000);
      Serial.println("-------------------PSRAM----------------------");
      Serial.printf("PSRAM Size: %d KB\n", ESP.getPsramSize() / 1024);
      Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);
      Serial.println("-------------------SYSTEM---------------------");
      Serial.printf("Uptime: %s\n", formatUptime(millis() - systemStartTime).c_str());
      Serial.printf("%s %s by %s\n", OS_NAME.c_str(), SYS_VER.c_str(), DEVELOPER.c_str());
      Serial.printf("ESP-IDF Version: %s\n", ESP.getSdkVersion());
      Serial.println("----------------------------------------------");
    } else if (cmd == "sleep") {
      Serial.println("CPU has been send to sleep");
      Serial.println("If you send a command via the Serial Monitor, the CPU will exit sleep mode.");
      delay(1000);
      setCpuFrequencyMhz(CPU_FREQ_SLEEP);
    } else if (cmd == "wifi") {
      String sub = "";
      if (input.length() > 5) {
        sub = input.substring(5);
        sub.trim();
      }

      if (sub.length() == 0) {
        Serial.println(F("WiFi commands:"));
        Serial.println(F("  wifi on          - Turn on WiFi (STA mode)"));
        Serial.println(F("  wifi off         - Turn off WiFi"));
        Serial.println(F("  wifi scan        - Scan for networks"));
        Serial.println(F("  wifi connect [SSID] [PASS] - Connect to network"));
        Serial.println(F("  wifi ap [SSID] [PASS]      - Start Access Point"));
        return;
      }

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
        } else {
          Serial.println(F("Usage: wifi connect [SSID] [PASSWORD]"));
        }
      } 
      else if (sub == "off") {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        Serial.println(F("WiFi: OFF"));
        digitalWrite(LED_GREEN, HIGH);
      } 
      else if (sub == "on") {
        WiFi.mode(WIFI_STA);
        Serial.println(F("WiFi: ON"));
        Serial.print(F("MAC Address: "));
        Serial.println(WiFi.macAddress());
        digitalWrite(LED_GREEN, LOW);
      } 
      else if (sub == "ap" || sub.startsWith("ap ")) {
        String args = "";
        if (sub.length() > 3) {
          args = sub.substring(3);
          args.trim();
        }
        
        if (args.length() == 0) {
          Serial.println(F("Usage: wifi ap [SSID] [PASSWORD]"));
          Serial.println(F("Example: wifi ap MyNetwork MyPassword123"));
          return;
        }
        
        int spaceIndex = args.indexOf(' ');
        if (spaceIndex == -1) {
          Serial.println(F("Error: Password is required."));
          Serial.println(F("Usage: wifi ap [SSID] [PASSWORD]"));
          return;
        }
        
        String userSSID = args.substring(0, spaceIndex);
        String userPASS = args.substring(spaceIndex + 1);
        userSSID.trim();
        userPASS.trim();
        
        if (userSSID.length() == 0) {
          Serial.println(F("Error: SSID cannot be empty."));
          return;
        }
        if (userPASS.length() < 8) {
          Serial.println(F("Error: Password must be at least 8 characters!"));
          return;
        }
        
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
      } else {
        Serial.printf("Unknown WiFi parameter: %s\n", sub.c_str());
        Serial.println(F("Type 'wifi' for help."));
      }
    } else if (cmd == "stop") {
      if (pingRunning) {
        pingStopRequested = true;
        Serial.println("Stopping ping...");
        delay(50);
      } else {
        Serial.println("No active task to stop");
      }
    } else if (cmd == "clear") {
      Serial.print("\033[2J\033[H");
      for (int i = 0; i < 40; i++) Serial.println();
    } else if (input.startsWith("echo ")) {
      String msg = input.substring(5);
      int pos = 0;
      while (pos < msg.length()) {
        int startPct = msg.indexOf('%', pos);
        if (startPct == -1) break;
        int endPct = msg.indexOf('%', startPct + 1);
        if (endPct == -1) break;
        String varName = msg.substring(startPct + 1, endPct);
        String valStr = "[UNKNOWN]";
        int idx = findVar(varName.c_str());
        if (idx != -1) {
          if (varName == "CPU_OVERHEAT") {
            valStr = dynVars[idx].value ? "true" : "false";
          } else {
            valStr = String(dynVars[idx].value);
          }
        }
        String startPart = msg.substring(0, startPct);
        String endPart = msg.substring(endPct + 1);
        msg = startPart + valStr + endPart;
        pos = startPart.length() + valStr.length();
      }
      Serial.println(msg);
    } else if (cmd == "echo") {
      Serial.println();
    } else if (cmd == "unset") {
      String varName = "";
      if (input.length() > 6) {
        varName = input.substring(6);
        varName.trim();
      }
      int spacePos = input.indexOf(' ');
      if (spacePos != -1) varName = input.substring(spacePos + 1);
      varName.trim();
      if (varName.length() == 0) {
        Serial.println("Usage: unset <VARIABLE_NAME>");
        return;
      }
      int idx = findVar(varName.c_str());
      if (idx != -1) {
        for (int i = idx; i < varCount - 1; i++) {
          dynVars[i] = dynVars[i + 1];
        }
        varCount--;
        Serial.printf("Variable '%s' removed.\n", varName.c_str());
      } else {
        Serial.printf("Variable '%s' not found.\n", varName.c_str());
      }
    } else {
      Serial.println("Invalid command!");
    }
  }
}
