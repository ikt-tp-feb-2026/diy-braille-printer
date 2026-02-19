#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <Preferences.h>

Preferences preferences;

// ==========================================
// НАСТРОЙКИ СЕТИ
// ==========================================
const char* ssid = "admin"; 
const char* password = "adminadmin"; 

// ==========================================
// НАСТРОЙКИ МЕХАНИКИ
// ==========================================
Servo myServo;
Servo myServo2;
const int servoPin = 5;
const int servo2Pin = 18;

// --- ШАГОВЫЙ МОТОР ---
const int STEP_PIN = 17; 
const int DIR_PIN = 16;  
const int STEP_DELAY = 900; // Задержка между шагами (мкс)

// --- СЕРВО ---
int ANGLE_UP = 0;      
int ANGLE_DOWN = 90;   

// Тайминги (мс) для печати (пока просто задержки)
int TIME_DOT = 150;   // Между точками
int TIME_CHAR = 200;  // Между буквами
int TIME_RET = 1500;  // Возврат каретки

// --- НАСТРОЙКИ ПРОТЯЖКИ БУМАГИ ---
int FEED_DIR = 1; // Направление, 1 = HIGH, 0 = LOW

// --- НАСТРОЙКИ СТРАНИЦЫ ---
int CHARS_PER_LINE = 13;   // Максимум символов в строке
int STEPS_ROW = 700;       // Шаг между точечными рядами
int STEPS_NEWLINE = 900;  // Шаг для перехода на новую строку

bool isPrinting = false;

WebServer server(80);

// ==========================================
// ВЕБ-ИНТЕРФЕЙС
// ==========================================

/*void handleRoot() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Error: index.html not found on SPIFFS/LittleFS");
    return;
  }
  server.streamFile(file, "text/html"); 
  file.close();
}*/

// ==========================================
// ПЕРЕМЕННЫЕ
// ==========================================

void loadSettings() {
  // Открываем пространство имен "printer" в режиме чтение/запись (false)
  preferences.begin("printer", false);

  // Читаем значения. Если ключа нет, берем второй аргумент (дефолт)
  ANGLE_UP = preferences.getInt("a_up", 0);
  ANGLE_DOWN = preferences.getInt("a_down", 90);
  TIME_DOT = preferences.getInt("t_dot", 150);
  TIME_CHAR = preferences.getInt("t_char", 200);
  TIME_RET = preferences.getInt("t_ret", 1500);
  FEED_DIR = preferences.getInt("f_dir", 1);
  CHARS_PER_LINE = preferences.getInt("c_line", 13);
  STEPS_ROW = preferences.getInt("s_row", 700);
  STEPS_NEWLINE = preferences.getInt("s_new", 900);

  Serial.println("Settings loaded from NVS");
}

// Обработчик сохранения (вызывается кнопкой "Сохранить" с сайта)
void handleSaveSettings() {
  bool changed = false;

  if (server.hasArg("a_up")) {
    ANGLE_UP = server.arg("a_up").toInt();
    preferences.putInt("a_up", ANGLE_UP);
    changed = true;
  }
  if (server.hasArg("a_down")) {
    ANGLE_DOWN = server.arg("a_down").toInt();
    preferences.putInt("a_down", ANGLE_DOWN);
    changed = true;
  }
  if (server.hasArg("t_dot")) {
    TIME_DOT = server.arg("t_dot").toInt();
    preferences.putInt("t_dot", TIME_DOT);
    changed = true;
  }
  if (server.hasArg("t_char")) { // Было пропущено во фронте
    TIME_CHAR = server.arg("t_char").toInt();
    preferences.putInt("t_char", TIME_CHAR);
    changed = true;
  }
  if (server.hasArg("t_ret")) { // Было пропущено в C++
    TIME_RET = server.arg("t_ret").toInt();
    preferences.putInt("t_ret", TIME_RET);
    changed = true;
  }
  if (server.hasArg("s_row")) {
    STEPS_ROW = server.arg("s_row").toInt();
    preferences.putInt("s_row", STEPS_ROW);
    changed = true;
  }
  if (server.hasArg("s_new")) { // Было пропущено в C++
    STEPS_NEWLINE = server.arg("s_new").toInt();
    preferences.putInt("s_new", STEPS_NEWLINE);
    changed = true;
  }
  if (server.hasArg("f_dir")) {
    FEED_DIR = server.arg("f_dir").toInt();
    preferences.putInt("f_dir", FEED_DIR);
    changed = true;
  }
  if (server.hasArg("c_line")) {
    CHARS_PER_LINE = server.arg("c_line").toInt();
    preferences.putInt("c_line", CHARS_PER_LINE);
    changed = true;
  }

  if (changed) {
    server.send(200, "text/plain", "Settings Saved & Applied!");
    Serial.println("New settings saved to NVS");
  } else {
    server.send(304, "text/plain", "No changes detected");
  }
}

void handleGetSettings() {
  String json = "{";
  json += "\"a_up\":" + String(ANGLE_UP) + ",";
  json += "\"a_down\":" + String(ANGLE_DOWN) + ",";
  json += "\"t_dot\":" + String(TIME_DOT) + ",";
  json += "\"t_char\":" + String(TIME_CHAR) + ",";
  json += "\"t_ret\":" + String(TIME_RET) + ",";
  json += "\"s_row\":" + String(STEPS_ROW) + ",";
  json += "\"s_new\":" + String(STEPS_NEWLINE) + ",";
  json += "\"c_line\":" + String(CHARS_PER_LINE) + ",";
  json += "\"f_dir\":" + String(FEED_DIR);
  json += "}";
  server.send(200, "application/json", json);
}

// ==========================================
// ЛОГИКА ESP32
// ==========================================

// База данных символов
byte getCharBits(String l) {
  // Кириллица
  if(l=="а" || l=="А") return 0b000001; 
  if(l=="б" || l=="Б") return 0b000011; 
  if(l=="в" || l=="В") return 0b111010;
  if(l=="г" || l=="Г") return 0b011011; 
  if(l=="д" || l=="Д") return 0b011001; 
  if(l=="е" || l=="Е") return 0b010001;
  if(l=="ё" || l=="Ё") return 0b100001; 
  if(l=="ж" || l=="Ж") return 0b011010; 
  if(l=="з" || l=="З") return 0b110101;
  if(l=="и" || l=="И") return 0b001010; 
  if(l=="й" || l=="Й") return 0b101111; 
  if(l=="к" || l=="К") return 0b000101;
  if(l=="л" || l=="Л") return 0b000111; 
  if(l=="м" || l=="М") return 0b001101; 
  if(l=="н" || l=="Н") return 0b011101;
  if(l=="о" || l=="О") return 0b010101; 
  if(l=="п" || l=="П") return 0b001111; 
  if(l=="р" || l=="Р") return 0b010111;
  if(l=="с" || l=="С") return 0b001110; 
  if(l=="т" || l=="Т") return 0b011110; 
  if(l=="у" || l=="У") return 0b100101;
  if(l=="ф" || l=="Ф") return 0b001011; 
  if(l=="х" || l=="Х") return 0b010011; 
  if(l=="ц" || l=="Ц") return 0b001001;
  if(l=="ч" || l=="Ч") return 0b011111; 
  if(l=="ш" || l=="Ш") return 0b110001; 
  if(l=="щ" || l=="Щ") return 0b101101;
  if(l=="ъ" || l=="Ъ") return 0b110111; 
  if(l=="ы" || l=="Ы") return 0b101110; 
  if(l=="ь" || l=="Ь") return 0b111110;
  if(l=="э" || l=="Э") return 0b101010; 
  if(l=="ю" || l=="Ю") return 0b110011; 
  if(l=="я" || l=="Я") return 0b101101;
  
  // Цифры
  if(l=="1") return 0b000001;
  if(l=="2") return 0b000011;
  if(l=="3") return 0b001001;
  if(l=="4") return 0b011001;
  if(l=="5") return 0b010001;
  if(l=="6") return 0b001011;
  if(l=="7") return 0b011011;
  if(l=="8") return 0b010011;
  if(l=="9") return 0b001010;
  if(l=="0") return 0b011010;
  
  // Знаки
  if(l=="." || l=="●") return 0b100100;
  if(l==",") return 0b000010;
  if(l=="!") return 0b010110;
  if(l=="?") return 0b100110;
  if(l=="-") return 0b100100;
  
  return 0; 
}

// Функция удара
void punch() {
  myServo.write(ANGLE_DOWN); 
  delay(120); 
  myServo.write(ANGLE_UP);   
  delay(120);
}

void runStepper(int steps, bool direction) {
  digitalWrite(DIR_PIN, direction);
  
  for (int i = 0; i < steps; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(STEP_DELAY);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(STEP_DELAY);
    
    if (i % 100 == 0) yield(); 
  }
}

// Главная логика печати
void printSequence(String text) {
  String currentLineBuffer = "";
  int charCount = 0;

  for (int i = 0; i < text.length(); i++) {
    server.handleClient(); 

    // Вытаскиваем полный символ (учитывая UTF-8)
    String ch = "";
    if ((text[i] & 0x80) == 0) {
      ch += (char)text[i];
    } else {
      ch += (char)text[i];
      if (i + 1 < text.length()) ch += (char)text[i+1];
      i++; 
    }

    // Если встретили принудительный перенос строки (\n)
    if (ch == "\n") {
      if (currentLineBuffer.length() > 0) {
        printPhysicalLine(currentLineBuffer);
      }
      currentLineBuffer = "";
      charCount = 0;
      continue;
    }

    // Накапливаем строку
    currentLineBuffer += ch;
    charCount++;

    // Если достигли лимита символов
    if (charCount >= CHARS_PER_LINE) {
      printPhysicalLine(currentLineBuffer); // Печатаем накопленное
      currentLineBuffer = "";              // Очищаем буфер
      charCount = 0;                       // Сбрасываем счетчик
    }
  }

  // Если после цикла что-то осталось в буфере (последняя неполная строка)
  if (currentLineBuffer.length() > 0) {
    printPhysicalLine(currentLineBuffer);
  }
}

void printPhysicalLine(String lineText) {
  Serial.println("Printing line: " + lineText);
  
  for (int row = 0; row < 3; row++) {
    // Проход печати точек
    Serial.print("--- Start Row Pass: "); Serial.println(row);
    
    for (int i = 0; i < lineText.length(); i++) {
      server.handleClient(); // Чтобы WiFi не отвалился
      
      String ch = "";
      // UTF-8 сборка символа
      if ((lineText[i] & 0x80) == 0) {
        ch += (char)lineText[i];
      } else {
        ch += (char)lineText[i];
        if (i+1 < lineText.length()) ch += (char)lineText[i+1];
        i++; 
      }
      
      if (ch == "\n" || ch == "\r") continue; // Игнорируем спецсимволы при печати точек

      byte bits = getCharBits(ch);
      bool d1=0, d2=0;

      if(row==0) { d1 = bits & 1;  d2 = bits & 8;  } 
      if(row==1) { d1 = bits & 2;  d2 = bits & 16; } 
      if(row==2) { d1 = bits & 4;  d2 = bits & 32; } 

      if(d1) punch(); else delay(240); 
      delay(TIME_DOT); 
      if(d2) punch(); else delay(240); 
      delay(TIME_CHAR); 
    }
    
    // Конец прохода (строка точек набита)
    Serial.println("Row pass complete. Returning carriage...");
    delay(TIME_RET); // Ждем, пока каретка вернется в начало (или возвращаем её руками)

    // Протяжка бумаги
    if (row < 2) {
      // Если это 0-й или 1-й проход — делаем МАЛЕНЬКИЙ шаг к следующему ряду точек 
      runStepper(STEPS_ROW, (FEED_DIR == 1 ? HIGH : LOW));
    } else {
      // Если это 2-й проход (последний) — делаем БОЛЬШОЙ шаг к следующей строке текста
      Serial.println("Line complete. Newline feed.");
      runStepper(STEPS_NEWLINE, (FEED_DIR == 1 ? HIGH : LOW));
    }
  }
}

void handlePrint() {
  if (server.hasArg("text")) {
    server.send(200, "text/plain", "OK");
    String msg = server.arg("text");
    Serial.println("Received: " + msg);

    isPrinting = true;
    printSequence(msg);
    isPrinting = false;
  } else {
    server.send(400, "text/plain", "Error: No text");
  }
}

void handleStatus(){
  if (isPrinting) {
    server.send(200, "text/plain", "printing");
  } else {
    server.send(200, "text/plain", "idle");
  }
}

void handleServo(){
  if (server.hasArg("angle")) {
    String angleVal = server.arg("angle");
    int angle = angleVal.toInt();
    
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    myServo.write(angle); 
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing angle");
  }
}

void handleStepper() {
  if (!server.hasArg("steps") || !server.hasArg("dir")) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }

  int steps = server.arg("steps").toInt();
  String dir = server.arg("dir");

  if (steps > 5000) steps = 5000;

  if (dir == "cw") {
    digitalWrite(DIR_PIN, HIGH); 
  } else {
    digitalWrite(DIR_PIN, LOW);
  }
  
  for (int i = 0; i < steps; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(STEP_DELAY);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(STEP_DELAY);
    if (i % 100 == 0) {
        yield();
    }
  }

  server.send(200, "text/plain", "Moved " + String(steps));
}

void setup() {
  Serial.begin(115200);

  loadSettings();

  // Файловая система
  if(!LittleFS.begin(true)){ 
    Serial.println("LittleFS Mount Failed");
    return;
  }
  
  // Серво
  myServo.attach(servoPin, 500, 2400); 
  myServo.write(ANGLE_UP);
  
  // Пины шаговика
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  // WiFi AP
  Serial.println("Starting AP...");
  WiFi.softAP(ssid, password);
  Serial.print("IP Address: "); 
  Serial.println(WiFi.softAPIP());

  server.serveStatic("/", LittleFS, "/index.html");
  server.serveStatic("/styles.css", LittleFS, "/styles.css");
  server.serveStatic("/script.js", LittleFS, "/script.js");

  // Маршруты
  //server.on("/", handleRoot);
  server.on("/print", handlePrint);
  server.on("/status", handleStatus);
  server.on("/servo", handleServo);
  server.on("/stepper", handleStepper);
  server.on("/save_settings", handleSaveSettings);
  server.on("/get_settings", handleGetSettings);
  
  server.begin();
  Serial.println("Server Started!");
}

void loop() {
  server.handleClient();
}
