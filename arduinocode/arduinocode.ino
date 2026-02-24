#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <math.h>

Preferences preferences;

// ==========================================
// НАСТРОЙКИ СЕТИ
// ==========================================
String ssid = "admin"; 
String password = "adminadmin"; 

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
int ANGLE_UP = 30;
int ANGLE_DOWN = 0;

// --- СЕРВО2 ---
int ANGLE_SIDE = 30;
int ANGLE_MAX_SIDE = 180;

int STEP_DOT = 3;   // Градусы поворота между 1 и 2 точкой внутри символа
int STEP_CHAR = 7;  // Градусы поворота между символами
int MAX_C = 320;
int RET_STEP_ANGLE = 2;   // На сколько градусов возвращать серво за один "тик"
int RET_STEP_DELAY = 15;  // Задержка (мс) между "тиками" возврата для плавности
int DEFAULT_POS_X = 95;
int currentPosX = DEFAULT_POS_X; // Текущая позиция каретки

// Тайминги (мс) для печати (пока просто задержки)
int TIME_DOT = 150;   // Между точками
int TIME_CHAR = 200;  // Между буквами
int TIME_RET = 1500;  // Возврат каретки

// --- НАСТРОЙКИ ПРОТЯЖКИ БУМАГИ ---
int FEED_DIR = 1; // Направление, 1 = HIGH, 0 = LOW

// --- НАСТРОЙКИ СТРАНИЦЫ ---
int CHARS_PER_LINE = 15;   // Максимум символов в строке
int STEPS_ROW = 700;       // Шаг между точечными рядами
int STEPS_NEWLINE = 900;  // Шаг для перехода на новую строку

int SIDE_A = 170;
int SIDE_B = 80;

int PUNCH_DELAY = 200;

bool isPrinting = false;

WebServer server(80);

// ==========================================
// ВЕБ-ИНТЕРФЕЙС
// ==========================================

void handleRoot() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Error: index.html not found on SPIFFS/LittleFS");
    return;
  }
  server.streamFile(file, "text/html"); 
  file.close();
}

// ==========================================
// ПЕРЕМЕННЫЕ
// ==========================================

void loadSettings() {
  // Открываем пространство имен "printer" в режиме чтение/запись (false)
  preferences.begin("printer", false);

  // Читаем значения. Если ключа нет, берем второй аргумент (дефолт)
  ssid = preferences.getString("ssid", "admin");
  password = preferences.getString("password", "adminadmin");
  
  ANGLE_UP = preferences.getInt("a_up", 30);
  ANGLE_DOWN = preferences.getInt("a_down", 0);
  ANGLE_SIDE = preferences.getInt("a_side", 30);
  TIME_DOT = preferences.getInt("t_dot", 150);
  TIME_CHAR = preferences.getInt("t_char", 200);
  TIME_RET = preferences.getInt("t_ret", 1500);
  FEED_DIR = preferences.getInt("f_dir", 1);
  CHARS_PER_LINE = preferences.getInt("c_line", 15);
  STEPS_ROW = preferences.getInt("s_row", 700);
  STEPS_NEWLINE = preferences.getInt("s_new", 900);
  STEP_DOT = preferences.getInt("as_dot", 3);
  STEP_CHAR = preferences.getInt("as_char", 6);
  RET_STEP_ANGLE = preferences.getInt("r_ang", 2);
  RET_STEP_DELAY = preferences.getInt("r_del", 15);

  SIDE_A = preferences.getInt("s_a", 170);
  SIDE_B = preferences.getInt("s_b", 80);
  MAX_C = preferences.getInt("m_c", 160);
  DEFAULT_POS_X = preferences.getInt("d_pos_x", 95);
  PUNCH_DELAY = preferences.getInt("p_del", 200);

  Serial.println("Settings loaded from NVS");
}

// Обработчик сохранения (вызывается кнопкой "Сохранить" с сайта)
void handleSaveSettings() {
  bool changed = false;
  bool wifiChanged = false;

  if (server.hasArg("ssid")) {
    String newSsid = server.arg("ssid");
    if (newSsid != ssid) {
      ssid = newSsid;
      preferences.putString("ssid", ssid);
      wifiChanged = true;
    }
  }
  if (server.hasArg("password")) {
    String newPassword = server.arg("password");
    if (newPassword != password) {
      password = newPassword;
      preferences.putString("password", password);
      wifiChanged = true;
    }
  }

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
  if (server.hasArg("a_side")) {
    ANGLE_SIDE = server.arg("a_side").toInt();
    preferences.putInt("a_side", ANGLE_SIDE);
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
  if (server.hasArg("as_dot")) {
    STEP_DOT = server.arg("as_dot").toInt();
    preferences.putInt("as_dot", STEP_DOT);
    changed = true;
  }
  if (server.hasArg("as_char")) {
    STEP_CHAR = server.arg("as_char").toInt();
    preferences.putInt("as_char", STEP_CHAR);
    changed = true;
  }
  if (server.hasArg("r_ang")) {
    RET_STEP_ANGLE = server.arg("r_ang").toInt();
    preferences.putInt("r_ang", RET_STEP_ANGLE);
    changed = true;
  }
  if (server.hasArg("r_del")) {
    RET_STEP_DELAY = server.arg("r_del").toInt();
    preferences.putInt("r_del", RET_STEP_DELAY);
    changed = true;
  }

  if (server.hasArg("s_a")) {
    SIDE_A = server.arg("s_a").toInt();
    preferences.putInt("s_a", SIDE_A);
    changed = true;
  }
  if (server.hasArg("s_b")) {
    SIDE_B = server.arg("s_b").toInt();
    preferences.putInt("s_b", SIDE_B);
    changed = true;
  }
  if (server.hasArg("m_c")) {
    MAX_C = server.arg("m_c").toInt();
    preferences.putInt("m_c", MAX_C);
    changed = true;
  }
  if (server.hasArg("d_pos_x")) {
    DEFAULT_POS_X = server.arg("d_pos_x").toInt();
    preferences.putInt("d_pos_x", DEFAULT_POS_X);
    changed = true;
  }
  if (server.hasArg("p_del")) {
    PUNCH_DELAY = server.arg("p_del").toInt();
    preferences.putInt("p_del", PUNCH_DELAY);
    changed = true;
  }

  if (wifiChanged) {
    server.send(200, "text/plain", "WiFi settings saved! Please reboot the device.");
    Serial.println("New WiFi settings saved to NVS. Reboot needed.");
  } else if (changed) {
    server.send(200, "text/plain", "Settings Saved & Applied!");
    Serial.println("New settings saved to NVS");
  } else {
    server.send(304, "text/plain", "No changes detected");
  }
}

void handleGetSettings() {
  String json = "{";
  json += "\"ssid\":\"" + ssid + "\",";
  json += "\"password\":\"" + password + "\",";
  json += "\"a_up\":" + String(ANGLE_UP) + ",";
  json += "\"a_down\":" + String(ANGLE_DOWN) + ",";
  json += "\"a_side\":" + String(ANGLE_SIDE) + ",";
  json += "\"t_dot\":" + String(TIME_DOT) + ",";
  json += "\"t_char\":" + String(TIME_CHAR) + ",";
  json += "\"t_ret\":" + String(TIME_RET) + ",";
  json += "\"s_row\":" + String(STEPS_ROW) + ",";
  json += "\"s_new\":" + String(STEPS_NEWLINE) + ",";
  json += "\"c_line\":" + String(CHARS_PER_LINE) + ",";
  json += "\"f_dir\":" + String(FEED_DIR) + ",";
  json += "\"as_dot\":" + String(STEP_DOT) + ",";
  json += "\"as_char\":" + String(STEP_CHAR) + ",";
  json += "\"r_ang\":" + String(RET_STEP_ANGLE) + ",";
  json += "\"r_del\":" + String(RET_STEP_DELAY) + ",";
  json += "\"s_a\":" + String(SIDE_A) + ",";
  json += "\"s_b\":" + String(SIDE_B) + ",";
  json += "\"m_c\":" + String(MAX_C) + ",";
  json += "\"d_pos_x\":" + String(DEFAULT_POS_X) + ",";
  json += "\"p_del\":" + String(PUNCH_DELAY);
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
  if(l=="я" || l=="Я") return 0b101011;
  
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
  if(l=="." || l=="●") return 0b110010;
  if(l==",") return 0b000010;
  if(l=="!") return 0b010110;
  if(l=="?") return 0b100010;
  if(l=="-") return 0b100100;
  
  //Цифровой идентификатор
  if(l=="#") return 0b111100;
  
  return 0; 
}

// Функция удара
void punch() {
  myServo.write(ANGLE_DOWN); 
  delay(PUNCH_DELAY); 
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

    currentPosX = DEFAULT_POS_X;
    if(currentPosX > MAX_C) currentPosX = MAX_C; // Защита от перекрута
    double cos_a = (- pow(SIDE_A, 2) + pow(SIDE_B, 2) + pow(currentPosX, 2)) / (2.0 * currentPosX * SIDE_B);
    if (cos_a < -1.0) cos_a = -1.0;
    if (cos_a > 1.0) cos_a = 1.0;
    double a = 180 - degrees(acos(cos_a));
    myServo2.write(a);
    delay(TIME_CHAR);
    
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
      
      // Сдвигаемся ко второй точке символа
      myServo2.write(ANGLE_SIDE);
      delay(TIME_RET);
      currentPosX += STEP_DOT;
      if(currentPosX > MAX_C) currentPosX = MAX_C; // Защита от перекрута
      cos_a = (- pow(SIDE_A, 2) + pow(SIDE_B, 2) + pow(currentPosX, 2)) / (2.0 * currentPosX * SIDE_B);
      if (cos_a < -1.0) cos_a = -1.0;
      if (cos_a > 1.0) cos_a = 1.0;
      a = 180 - degrees(acos(cos_a));
      Serial.print(currentPosX);
      Serial.print("|");
      Serial.print(a);
      Serial.print("|");
      Serial.print(cos_a);
      Serial.println("|");
      //myServo2.write(0);
      //delay(TIME_DOT); 
      myServo2.write(a);
      delay(TIME_DOT); 
      
      if(d2) punch(); else delay(240); 
      
      // Сдвигаемся к следующему символу
      myServo2.write(ANGLE_SIDE);
      delay(TIME_RET);
      currentPosX += STEP_CHAR;
      if(currentPosX > MAX_C) currentPosX = MAX_C; // Защита от перекрута
      cos_a = (- pow(SIDE_A, 2) + pow(SIDE_B, 2) + pow(currentPosX, 2)) / (2.0 * currentPosX * SIDE_B);
      if (cos_a < -1.0) cos_a = -1.0;
      if (cos_a > 1.0) cos_a = 1.0;
      a = 180 - degrees(acos(cos_a));
      Serial.print(currentPosX);
      Serial.print("|");
      Serial.print(a);
      Serial.print("|");
      Serial.println(cos_a);
      //myServo2.write(0);
      //delay(TIME_CHAR); 
      myServo2.write(a);
      delay(TIME_CHAR);
    }
    
    myServo2.write(ANGLE_SIDE);
    // Конец прохода (строка точек набита)
    Serial.println("Row pass complete. Returning carriage...");
    delay(TIME_RET); // Оставляем общую паузу перед следующим шагом мотора (на всякий случай)

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


void handleServo2(){
  if (server.hasArg("angle")) {
    String angleVal = server.arg("angle");
    int angle = angleVal.toInt();
    
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    myServo2.write(angle); 
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
  
  myServo2.attach(servo2Pin, 500, 2400); 
  myServo2.write(ANGLE_SIDE);
  
  // Пины шаговика
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  // WiFi AP
  Serial.println("Starting AP...");
  WiFi.softAP(ssid.c_str(), password.c_str());
  Serial.print("IP Address: "); 
  Serial.println(WiFi.softAPIP());

  // Маршруты
  server.on("/", handleRoot);
  server.on("/print", handlePrint);
  server.on("/status", handleStatus);
  server.on("/servo", handleServo);
  server.on("/servo2", handleServo2);
  server.on("/stepper", handleStepper);
  server.on("/save_settings", handleSaveSettings);
  server.on("/get_settings", handleGetSettings);

  //server.serveStatic("/", LittleFS, "/");
  
  server.begin();
  Serial.println("Server Started!");
}

void loop() {
  server.handleClient();
}
