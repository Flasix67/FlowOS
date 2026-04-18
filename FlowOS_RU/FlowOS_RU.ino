#include <WiFi.h>
#include <ESPping.h>
#include <esp_timer.h>

#define MAX_VARS 64
String SYS_VER = "1.1-RC2";
String DEVELOPER = "Flasix67";
String OS_NAME = "FlowOS";

uint32_t systemStartTime = 0;

int CPU_FREQ_SLEEP = 10;
int CPU_FREQ_MIN = 80;
int CPU_FREQ_MID = 160;
int CPU_FREQ_MAX = 240;
bool CPU_OVERHEAT = false;

bool pingRunning = false;
bool pingStopRequested = false;
bool pingStopFlag = false;

void setup() {
  systemStartTime = millis();
  setCpuFrequencyMhz(CPU_FREQ_MAX);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);

  registerVar("CPU_FREQ_SLEEP", "10");
  registerVar("CPU_FREQ_MIN", "80");
  registerVar("CPU_FREQ_MID", "160");
  registerVar("CPU_FREQ_MAX", "240");
  registerVar("CPU_OVERHEAT", "0");
  registerVar("SYS_VER", SYS_VER.c_str());
  registerVar("DEVELOPER", DEVELOPER.c_str());
  registerVar("OS_NAME", OS_NAME.c_str());

  Serial.begin(115200);
  Serial.setTimeout(50);

  delay(3000);

  Serial.println("-------------------------------------");
  Serial.printf("|          %s %s           |\n", OS_NAME, SYS_VER);
  Serial.println("-------------------------------------");
  Serial.println("(Основано на Waveshare ESP32S3 NANO N16R8)");
  Serial.println("======================================");
  Serial.println("Введите команду : ");

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
    Serial.printf("Ошибка: Имя переменной '%s' слишком длинное!\n", name);
    return;
  }
  if (varCount >= MAX_VARS) {
    Serial.println("Ошибка: Таблица переменных заполнена.");
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
  Serial.println("=== Динамические переменные ===");
  if (varCount == 0) {
    Serial.println("(пусто)");
  } else {
    for (int i = 0; i < varCount; i++) {
      Serial.printf("  %s = %s\n", dynVars[i].name, dynVars[i].value);
      Serial.println("-------------------------");
    }
  }
}

void runPing(String target, int count, bool continuous) {
  if (!WiFi.isConnected()) {
    Serial.println("Ошибка: WiFi не подключен.");
    pingRunning = false;
    return;
  }

  IPAddress ip;
  if (!WiFi.hostByName(target.c_str(), ip)) {
    Serial.printf("Ошибка: Не удается найти '%s'\n", target.c_str());
    pingRunning = false;
    return;
  }

  Serial.printf("\nОбмен пакетами с %s [%s] с 32 байтами данных:\n", target.c_str(), ip.toString().c_str());
  
  int success = 0;
  int total = 0;

  while (continuous || total < count) {
    if (pingStopRequested) {
      pingStopRequested = false;
      pingRunning = false;
      Serial.println("\nping остановлен.");
      return;
    }
    if (Serial.available()) {
      String check = Serial.readStringUntil('\n');
      check.trim();
      if (check == "stop") {
        pingStopRequested = true;
        Serial.printf("\nСтатистика Ping для %s:\n", ip.toString().c_str());
        Serial.printf("    Пакеты: Отправлено = %d, Получено = %d, Потеряно = %d (%.0f%% потеряно)\n", 
                        total, success, total-success, (float)(total-success)/total*100);
        pingRunning = false;
        return;
      }
    }
    total++;
    bool result = Ping.ping(ip, 1);
    if (result) {
      float avg_time = Ping.averageTime();
      Serial.printf("Ответ от %s: байтов=32 time=%.0fms\n", ip.toString().c_str(), avg_time);
      success++;
    } else {
      Serial.println("Время ожидания истекло.");
    }
    delay(1000);
    if (!continuous && total >= count) break;
  }
  pingRunning = false;
  pingStopRequested = false;
  Serial.printf("\nСтатистика Ping для %s:\n", ip.toString().c_str());
  Serial.printf("    Пакеты: Отправлено = %d, Получено = %d, Потеряно = %d (%.0f%% потеряно)\n", 
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
  if (temperatureRead() > 80) {
    if (CPU_OVERHEAT == false) {
      Serial.println("CPU перегрелся! Частота снизится до 80MHz");
      setCpuFrequencyMhz(CPU_FREQ_MIN);
      CPU_OVERHEAT = true;
      int idx = findVar("CPU_OVERHEAT");
      if (idx != -1) strcpy(dynVars[idx].value, "1");
    }
  } else if (CPU_OVERHEAT == true) {
    if (temperatureRead() < 71) {
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

    if (input == "reboot") {
      ESP.restart();
    } else if (input == "help") {
      Serial.println("-------------------------");
      Serial.println("|   Доступные комманды  |");
      Serial.println("-------------------------");
      Serial.println("REBOOT - Перезапускает ESP");
      Serial.println("INFO - Показывает информацию о CPU/RAM/FLASH/PSRAM/SYSTEM");
      Serial.println("SLEEP - Переводит процессор в энергоэффективный режим.");
      Serial.println("WIFI [ON/OFF/SCAN/CONNECT/DISCONNECT/AP] - Запускает WiFi модем");
      Serial.println("PING [OPTIONS] [Host] - Показывает пинг хоста");
      Serial.println("STOP - Останавливает процесс по типу 'ping -t'");
      Serial.println("SET [NAME/GPIO] [VALUE] - Создает или изменяет переменную. Вы также можете включить GPIO с этой коммандой");
      Serial.println("CLEAR - Очищает экран");
      Serial.println("ECHO [TEXT] - Печатает текст в Serial Monitor. Используйте %VAR% для переменных в тексте.");
      Serial.println("UNSET [NAME] - Удаляет переменную.");
      Serial.println("-------------------------");
      } else if (input == "ping" || input.startsWith("ping ")) {
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
              Serial.println("Ошибка: Для параметра -n требуется значение.");
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
              Serial.println("Ошибка: параметр -n должен содержать число.");
              return;
            }
            
            long parsed = numStr.toInt();
            if (parsed < 1) {
              Serial.println("Недопустимое значение для параметра -n, минимально допустимый диапазон равен 1.");
              return;
            }
            count = parsed;
          }
          else if (token == "-t") {
            continuous = true;
            count = 999999;
          }
          else if (token == "-w" || token == "-i" || token == "-l") {
            Serial.printf("(Примечание: %s не поддерживается в ESPping библиотеке)\n", token.c_str());
          }
        }
        else if (target.isEmpty()) {
          target = token;
        }
      }
      
      if (target.isEmpty()) {
        Serial.println("Использование: ping [опции] <хост>");
        Serial.println("Примеры: ping 8.8.8.8 | ping -t example.com | ping -n 10 example.com");
      }
      else if (pingRunning) {
        Serial.println("Ошибка: ping уже запущен. Сначала введите 'stop'.");
      }
      else {
        pingRunning = true;
        pingStopFlag = false;
        runPing(target, count, continuous);
      }
    } else if (input == "info") {
      Serial.println("|      Информация       |");
      Serial.println("--------------------CPU-----------------------");
      Serial.printf("Модель чипа: %s (Cores: %d)\n", ESP.getChipModel(), ESP.getChipCores());
      Serial.printf("Частота CPU: %d MHz\n", ESP.getCpuFreqMHz());
      Serial.printf("Температура: %.1f C\n", temperatureRead());
      Serial.printf("Ревизия чипа: %d\n", ESP.getChipRevision());
      Serial.println("--------------------RAM-----------------------");
      Serial.printf("Heap Свободно: %d KB\n", ESP.getFreeHeap() / 1024);
      Serial.printf("Heap Использовано:  %d KB\n", (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024);
      Serial.printf("Всего Heap: %d KB\n", ESP.getHeapSize() / 1024);
      Serial.println("-------------------FLASH----------------------");
      Serial.printf("Flash размер: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
      Serial.printf("Flash скорость: %d MHz\n", ESP.getFlashChipSpeed() / 1000000);
      Serial.println("-------------------PSRAM----------------------");
      Serial.printf("PSRAM Размер: %d KB\n", ESP.getPsramSize() / 1024);
      Serial.printf("PSRAM Свободно: %d KB\n", ESP.getFreePsram() / 1024);
      Serial.println("-------------------SYSTEM---------------------");
      Serial.printf("Время работы: %s\n", formatUptime(millis() - systemStartTime).c_str());
      Serial.printf("%s %s от %s\n", OS_NAME.c_str(), SYS_VER.c_str(), DEVELOPER.c_str());
      Serial.printf("ESP-IDF Версия: %s\n", ESP.getSdkVersion());
      Serial.println("----------------------------------------------");
    } else if (input == "sleep") {
      Serial.println("CPU переведен в спящий режим.");
      Serial.println("Если вы отправете команду через Serial Monitor, то процессор выйдет из этого режима.");
      delay(1000);
      setCpuFrequencyMhz(CPU_FREQ_SLEEP);
    } else if (input == "wifi" || input.startsWith("wifi ")) {
      String sub = "";
      if (input.length() > 5) {
        sub = input.substring(5);
        sub.trim();
      }

      if (sub.length() == 0) {
        Serial.println(F("WiFi команды:"));
        Serial.println(F("  wifi on          - Включить WiFi (STA mode)"));
        Serial.println(F("  wifi off         - Выключить WiFi"));
        Serial.println(F("  wifi scan        - Сканировать WiFi"));
        Serial.println(F("  wifi connect [SSID] [PASS] - Подключится к WiFi"));
        Serial.println(F("  wifi ap [SSID] [PASS]      - Запустить точку доступа"));
        return;
      }

      if (sub == "scan") {
        digitalWrite(LED_GREEN, LOW);
        Serial.println(F("WiFi: Сканирование..."));
        int n = WiFi.scanNetworks();
        if (n == 0) {
          Serial.println(F("Ничего не найдено."));
        } else {
          Serial.printf("Найдено %d сетей:\n", n);
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
          
          Serial.printf("Подключение к %s...\n", ssid.c_str());
          WiFi.begin(ssid.c_str(), pass.c_str());
          int timer = 0;
          while (WiFi.status() != WL_CONNECTED && timer < 20) {
            delay(500);
            Serial.print(".");
            timer++;
          }

          if (WiFi.status() == WL_CONNECTED) {
            Serial.println(F("\nПодключено!"));
            Serial.print(F("IP: ")); Serial.println(WiFi.localIP());
            digitalWrite(LED_GREEN, LOW);
          } else {
            Serial.println(F("\nНе удалось. Проверьте SSID/Pass."));
          }
        } else {
          Serial.println(F(": wifi connect [SSID] [PASSWORD]"));
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
        Serial.print(F("MAC Адрес: "));
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
          Serial.println(F("Использование: wifi ap [SSID] [PASSWORD]"));
          Serial.println(F("Пример: wifi ap MyNetwork MyPassword123"));
          return;
        }
        
        int spaceIndex = args.indexOf(' ');
        if (spaceIndex == -1) {
          Serial.println(F("Ошибка: Требуется пароль."));
          Serial.println(F("Использование: wifi ap [SSID] [PASSWORD]"));
          return;
        }
        
        String userSSID = args.substring(0, spaceIndex);
        String userPASS = args.substring(spaceIndex + 1);
        userSSID.trim();
        userPASS.trim();
        
        if (userSSID.length() == 0) {
          Serial.println(F("Ошибка: SSID не может быть пустым."));
          return;
        }
        if (userPASS.length() < 8) {
          Serial.println(F("Ошибка: Пароль должен содержать не менее 8 символов!"));
          return;
        }
        
        WiFi.mode(WIFI_AP);
        digitalWrite(LED_GREEN, LOW);
        if (WiFi.softAP(userSSID.c_str(), userPASS.c_str())) {
          Serial.println(F("-------------------------------------"));
          Serial.println(F("          WiFi: AP ON                "));
          Serial.print(F("SSID: ")); Serial.println(userSSID);
          Serial.print(F("PASS: ")); Serial.println(userPASS);
          Serial.print(F("IP Адрес: ")); Serial.println(WiFi.softAPIP());
          Serial.println(F("-------------------------------------"));
        } else {
          Serial.println(F("WiFi: Критическая ошибка при запуске точки доступа."));
        }
      } else {
        Serial.printf("Неизвестный параметр WiFi: %s\n", sub.c_str());
        Serial.println(F("Введите 'wifi' для справки."));
      }
    } else if (input == "stop") {
      if (pingRunning) {
        pingStopRequested = true;
        Serial.println("Остановка ping...");
        delay(50);
      } else {
        Serial.println("Нету активной задачи.");
      }
    } else if (input == "clear") {
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
    } else if (input == "echo") {
      Serial.println();
    } else if (input == "unset" || input.startsWith("unset ")) {
      String varName = "";
      if (input.length() > 6) {
        varName = input.substring(6);
        varName.trim();
      }
      int spacePos = input.indexOf(' ');
      if (spacePos != -1) varName = input.substring(spacePos + 1);
      varName.trim();
      if (varName.length() == 0) {
        Serial.println("Использование: unset <ИМЯ_ПЕРЕМЕННОЙ>");
        return;
      }
      int idx = findVar(varName.c_str());
      if (idx != -1) {
        for (int i = idx; i < varCount - 1; i++) {
          dynVars[i] = dynVars[i + 1];
        }
        varCount--;
        Serial.printf("Переменная '%s' удалена.\n", varName.c_str());
      } else {
        Serial.printf("Переменная '%s' не найдена.\n", varName.c_str());
      }
    } else {
      Serial.println("Неверная команда!");
    }
  }
}
