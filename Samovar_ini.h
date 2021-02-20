//**************************************************************************************************************
// Параметры подключения к WIFI
//**************************************************************************************************************
#define SAMOVAR_HOST "samovar"
#define SAMOVAR_SSID "ssid"
#define SAMOVAR_PASSWORD "pwd"
#define SAMOVAR_AUTH "auth-auth"
//**************************************************************************************************************

//**************************************************************************************************************
// Настройки предельных значений для контроля автоматики
//**************************************************************************************************************
//Температура воды, при достижении которой будет оповещен оператор
#define ALARM_WATER_TEMP 70
//Максимальное значение температуры воды, при котором выключится питание
#define MAX_WATER_TEMP 75
//Максимальное значение температуры пара, при котором выключится питание
#define MAX_STEAM_TEMP 99.8
//Значение температуры датчика пара, при котором колонна перейдет из режима разгона в рабочий режим
#define CHANGE_POWER_MODE_STEAM_TEMP 76.1
//Значение температуры датчика в кубе, при котором откроется клапан подачи воды
#define OPEN_VALVE_TANK_TEMP 59.5
//Корректировка для угла поворота сервопривода.
int8_t servoDelta[11] = {0, -2, -3, -4, -3, -2, 0, 0, 0, 0, -2};
//**************************************************************************************************************

//**************************************************************************************************************
// Настройки использования подсистем Самовара
//**************************************************************************************************************
#define SAMOVAR_USE_BLYNK                   //использовать Blynk в проекте
#define SAMOVAR_USE_POWER                   //использовать регулятор напряжения в проекте
#define USE_WATERSENSOR                     //использовать датчик потока воды охлаждения. Если используется - в случае отсутствия воды в течении заданного времени Самовар отключит нагрев колонны
#define USE_HEAD_LEVEL_SENSOR               //использовать датчик уровня флегмы в голове. Если используется - в случае срабатывания датчика, снизит напряжение на 3 вольта

//Необходимо выбрать, какой используется датчик давления, и оставить только одну строку. Другие строки должны быть закоментиированы.
#define USE_BMP180                          //использовать датчик давления BMP180/BMP085
//#define USE_BMP280                          //использовать датчик давления BMP280
//#define USE_BME680                          //использовать датчик давления BME680

//#define USE_UPDATE_OTA                      //использовать ArduinoOTA для обновления через WI-FI. Удобно при отладке, при работе создает лишнюю нагрузку на ESP32
//#define USE_WEB_SERIAL                      //использовать библиотеку WebSerial для отладки

#define WRITE_PROGNUM_IN_LOG                // писать в лог номер текущей строки программы
