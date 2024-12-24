#include <Wire.h>
#include <I2Cdev.h>
#include <MPU6050.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>

// Создаем объекты для MPU6050
MPU6050 mpu1(0x68); // Адрес 0x68 (AD0 подключен к GND)
MPU6050 mpu2(0x69); // Адрес 0x69 (AD0 подключен к VCC)

// Настройки Wi-Fi
const char* ssid = "test1";       // Замените на ваш SSID
const char* password = "test123456"; // Замените на ваш пароль

// Настройки Telegram Bot
#define BOT_TOKEN "чувак ты надеялся что я так просто палю токен бота???"  // Замените на токен вашего бота
const unsigned long BOT_MTBS = 500; // Интервал проверки новых сообщений (1 секунда)

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_last_time; // Время последней проверки сообщений

// Массив поддерживающих фраз
const char* supportPhrases[] = {
  "Ты сильнее, чем думаешь!",
  "Не сдавайся, у тебя всё получится!",
  "Каждый день — это новый шанс!",
  "Ты делаешь большой шаг, даже если это кажется маленьким!",
  "Верь в себя, и мир откроется для тебя!"
};

// Количество фраз в массиве
const int numSupportPhrases = sizeof(supportPhrases) / sizeof(supportPhrases[0]);

// Массив с URL-адресами фотографий
const char* advicePhotos[] = {
  "https://i.mycdn.me/videoPreview?id=6081208519274&type=39&idx=7&tkn=15nH1bdOc1dDt5bPZ36djf1fOE8&fn=w_548",
  "https://via.placeholder.com/200", // Добавьте другие фотографии, если нужно
  "https://via.placeholder.com/250",
  "https://via.placeholder.com/300",
  "https://via.placeholder.com/350"
};

// Количество фотографий в массиве
const int numAdvicePhotos = sizeof(advicePhotos) / sizeof(advicePhotos[0]);

// Секретный пароль для подключения к боту
String secretPassword;
bool isConnectedToBot = false;

// Chat ID пользователя, которому нужно отправить секретный пароль
const String target_chat_id = "5700404625"; // Замените на реальный chat_id

// Переменные для хранения "нормы"
int16_t normAccX1, normAccY1, normAccZ1;
int16_t normAccX2, normAccY2, normAccZ2;

// Флаг для проверки, сохранена ли норма
bool normSaved = false;

// Вибромотор
const int vibroPin = 12; // Подключите вибромотор к GPIO12

void setup() {
  Serial.begin(115200);

  // Инициализация I2C шины (GPIO4, GPIO5)
  Wire.begin(4, 5); // SDA = GPIO4, SCL = GPIO5

  // Инициализация MPU6050
  initializeMPU(mpu1, "MPU1");
  initializeMPU(mpu2, "MPU2");

  // Подключение к Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println();
  Serial.print("Подключение к Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Подключено к Wi-Fi");

  // Отключение проверки сертификата (если необходимо)
  secured_client.setInsecure();

  // Генерация секретного пароля и отправка его в Telegram
  secretPassword = generateRandomPasswordAndSend();
  Serial.println("Секретный пароль: " + secretPassword);

  // Регистрация команд бота
  registerBotCommands();

  // Инициализация Telegram Bot
  bot_last_time = millis();

  // Инициализация вибромотора
  pinMode(vibroPin, OUTPUT);
  digitalWrite(vibroPin, LOW); // Выключаем вибромотор

  // Инициализация EEPROM
  EEPROM.begin(512);

  // Загрузка нормы из EEPROM
  loadNorm();

  Serial.println("Настройка завершена. Начинаем чтение данных...");
}

void loop() {
  // Читаем данные с MPU6050
  readMPU(mpu1, "MPU1");
  readMPU(mpu2, "MPU2");

  // Проверяем отклонение от нормы и включаем вибромотор при необходимости
  if (normSaved) {
    checkDeviationAndVibrate();
  }

  // Проверяем новые сообщения от Telegram
  if (millis() - bot_last_time > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    if (numNewMessages) {
      handleNewMessages(numNewMessages);
    }

    bot_last_time = millis();
  }

  delay(500); // Задержка 1 секунда
}

// Функция для инициализации MPU6050
void initializeMPU(MPU6050 &mpu, const char* name) {
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println(String(name) + " не подключен!");
    while (1); // Остановка программы, если датчик не подключен
  } else {
    Serial.println(String(name) + " подключен");
  }
}

// Функция для чтения данных с MPU6050
void readMPU(MPU6050 &mpu, const char* name) {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  // Получаем данные акселерометра и гироскопа
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Выводим данные в Serial-монитор
  Serial.println(String(name) + ":");
  Serial.print("Accel: "); Serial.print(ax); Serial.print(", "); Serial.print(ay); Serial.print(", "); Serial.println(az);
  Serial.print("Gyro: "); Serial.print(gx); Serial.print(", "); Serial.print(gy); Serial.print(", "); Serial.println(gz);
  Serial.println();
}

// Функция для генерации случайного секретного пароля и отправки его в Telegram
String generateRandomPasswordAndSend() {
  String password = "";
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  for (int i = 0; i < 6; i++) {
    password += charset[random(sizeof(charset) - 1)];
  }

  // Отправляем пароль в Telegram
  bot.sendMessage(target_chat_id, "Ваш секретный пароль: " + password);

  return password;
}

// Обработка новых сообщений
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    // Если имя пользователя не указано, используем "Guest"
    if (from_name == "") {
      from_name = "Guest";
    }

    // Отправляем уведомление в консоль
    Serial.print("[" + String(millis()) + "] ");
    Serial.print(from_name + " написал: " + text);
    Serial.println();

    // Обработка команд
    if (text == "/start") {
      bot.sendMessage(chat_id, "Привет! Я бот для креветки. Введите секретный пароль для подключения.");
    } else if (text == secretPassword) {
      isConnectedToBot = true;
      bot.sendMessage(chat_id, "Вы успешно подключены! Теперь вы можете использовать команды бота.");
    } else if (isConnectedToBot) {
      if (text == "/state") {
        bot.sendMessage(chat_id, "Состояние креветки: ты не справляешься, расправь плечи и смотри в сторону лучшего будущего!");
      } else if (text == "/advice") {
        // Выбираем случайное фото из массива
        int randomIndex = random(numAdvicePhotos);
        String randomPhotoUrl = advicePhotos[randomIndex];
        // Отправляем фото
        bot.sendPhoto(chat_id, randomPhotoUrl, "Совет от креветки: посмотри на это фото!");
      } else if (text == "/support") {
        // Выбираем случайную поддерживающую фразу
        int randomIndex = random(numSupportPhrases);
        String randomPhrase = supportPhrases[randomIndex];
        bot.sendMessage(chat_id, randomPhrase);
      } else if (text == "/data") {
        // Отправляем данные с гироскопов
        sendGyroData(chat_id);
      } else if (text == "/save") {
        // Сохраняем текущие значения как норму
        saveNorm();
        bot.sendMessage(chat_id, "Норма сохранена!");
      } else {
        bot.sendMessage(chat_id, "Неизвестная команда. Введите /start, чтобы начать.");
      }
    } else {
      bot.sendMessage(chat_id, "Вы не подключены. Введите секретный пароль для подключения.");
    }
  }
}

// Функция для отправки данных с гироскопов в чат
void sendGyroData(String chat_id) {
  int16_t ax1, ay1, az1, gx1, gy1, gz1;
  int16_t ax2, ay2, az2, gx2, gy2, gz2;

  // Читаем данные с MPU6050
  mpu1.getMotion6(&ax1, &ay1, &az1, &gx1, &gy1, &gz1);
  mpu2.getMotion6(&ax2, &ay2, &az2, &gx2, &gy2, &gz2);

  // Формируем сообщение с данными
  String message = "Данные с гироскопов:\n";
  message += "MPU1:\n";
  message += "Accel: " + String(ax1) + ", " + String(ay1) + ", " + String(az1) + "\n";
  message += "Gyro: " + String(gx1) + ", " + String(gy1) + ", " + String(gz1) + "\n";
  message += "MPU2:\n";
  message += "Accel: " + String(ax2) + ", " + String(ay2) + ", " + String(az2) + "\n";
  message += "Gyro: " + String(gx2) + ", " + String(gy2) + ", " + String(gz2) + "\n";

  // Отправляем сообщение в чат
  bot.sendMessage(chat_id, message);
}

// Функция для регистрации команд бота
void registerBotCommands() {
  String commands = "{\"commands\":["
                    "{\"command\":\"start\",\"description\":\"Начать работу с ботом\"},"
                    "{\"command\":\"data\",\"description\":\"Получить данные с гироскопов\"},"
                    "{\"command\":\"state\",\"description\":\"Получить состояние\"},"
                    "{\"command\":\"advice\",\"description\":\"Получить совет\"},"
                    "{\"command\":\"support\",\"description\":\"Получить поддержку\"},"
                    "{\"command\":\"save\",\"description\":\"Сохранить текущие значения как норму\"}"
                    "]}";

  // Формируем полную строку запроса
  String request = "setMyCommands?" + commands;

  // Отправляем запрос
  bot.sendGetToTelegram(request);
}

// Функция для сохранения текущих значений как нормы
void saveNorm() {
  mpu1.getMotion6(&normAccX1, &normAccY1, &normAccZ1, nullptr, nullptr, nullptr);
  mpu2.getMotion6(&normAccX2, &normAccY2, &normAccZ2, nullptr, nullptr, nullptr);

  // Сохраняем значения в EEPROM
  EEPROM.put(0, normAccX1);
  EEPROM.put(4, normAccY1);
  EEPROM.put(8, normAccZ1);
  EEPROM.put(12, normAccX2);
  EEPROM.put(16, normAccY2);
  EEPROM.put(20, normAccZ2);
  EEPROM.commit(); // Фиксируем изменения

  normSaved = true;
  Serial.println("Норма сохранена в EEPROM!");
}

// Функция для загрузки нормы из EEPROM
void loadNorm() {
  EEPROM.get(0, normAccX1);
  EEPROM.get(4, normAccY1);
  EEPROM.get(8, normAccZ1);
  EEPROM.get(12, normAccX2);
  EEPROM.get(16, normAccY2);
  EEPROM.get(20, normAccZ2);

  normSaved = true;
  Serial.println("Норма загружена из EEPROM!");
}

// Функция для проверки отклонения от нормы и включения вибромотора
void checkDeviationAndVibrate() {
  int16_t ax1, ay1, az1;
  int16_t ax2, ay2, az2;

  // Читаем текущие значения акселерометров
  mpu1.getMotion6(&ax1, &ay1, &az1, nullptr, nullptr, nullptr);
  mpu2.getMotion6(&ax2, &ay2, &az2, nullptr, nullptr, nullptr);

  // Проверяем отклонение от нормы
  bool deviationDetected = false;

  // Проверка для MPU1
  if (abs(ax1 - normAccX1) > 500 || abs(ay1 - normAccY1) > 500 || abs(az1 - normAccZ1) > 500) {
    deviationDetected = true;
  }

  // Проверка для MPU2
  if (abs(ax2 - normAccX2) > 500 || abs(ay2 - normAccY2) > 500 || abs(az2 - normAccZ2) > 500) {
    deviationDetected = true;
  }

  // Если обнаружено отклонение, включаем вибромотор
  if (deviationDetected) {
    digitalWrite(vibroPin, HIGH); // Включаем вибромотор
    Serial.println("Отклонение обнаружено! Вибромотор включен.");
  } else {
    digitalWrite(vibroPin, LOW); // Выключаем вибромотор
    Serial.println("Нет отклонения. Вибромотор выключен.");
  }
}
