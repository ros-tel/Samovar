#ifndef __SAMOVAR_I_H_
#define __SAMOVAR_I_H_
#include <Arduino.h>
//**************************************************************************************************************
// Параметры
//**************************************************************************************************************
#define SAMOVAR_HOST "samovar"
//**************************************************************************************************************

//**************************************************************************************************************
// Настройки предельных значений для контроля автоматики
//**************************************************************************************************************
//Температура воды, при достижении которой будет оповещен оператор
#define ALARM_WATER_TEMP 70
//Максимальное значение температуры воды, при котором выключится питание
#define MAX_WATER_TEMP 75
//Максимальное значение температуры пара, при котором выключится питание
#define MAX_STEAM_TEMP 98.8
//Максимальное значение температуры в ТСА, при котором выключится питание
#define MAX_ACP_TEMP 75
//Значение температуры датчика пара, при котором колонна перейдет из режима разгона в рабочий режим
#define CHANGE_POWER_MODE_STEAM_TEMP 39
//Значение температуры датчика в кубе, при котором откроется клапан подачи воды и включится насос подачи воды
#define OPEN_VALVE_TANK_TEMP 77
//Разность между заданной Т охлаждения воды и Т воды, до которой будет работать насос после завершения процесса перегонки в режимах дистилляции и ректификации
#define DELTA_T_CLOSE_VALVE 20


//Нижнее значение (в процентах), для работы ШИМ-регулятора для насоса. Необходимо подобрать такое значение, которое обеспечивает поток воды
#define PWM_LOW_VALUE 10
//Значение (в процентах), с которого начнет работать ШИМ-регулятор для насоса. Необходимо подобрать такое значение, которое гарантированно обеспечивает запуск мотора
#define PWM_START_VALUE 40

//Корректировка для угла поворота сервопривода.
int8_t servoDelta[11] = {0, -2, -3, -4, -3, -2, 0, 0, 0, 0, -2};

//Разница между целевой температурой и текущей температурой (для режимов пиво и су-вид), до достяжения которой нагрев будет вестись на полную мощность (в режиме разгона).
#define HEAT_DELTA 1
//Если разница между целевой температурой и текущей температурой (для режимов пиво и су-вид) больше заданной, то при использовании регулятора, управляемого по UART дополнительно Реле4 будет включать разгонный тэн, при разнице меньше разгонный тэн отключится
#define ACCELERATION_HEATER_DELTA 4


//Температура кипения (для режима пиво)
#define BOILING_TEMP 98.9
//**************************************************************************************************************

//**************************************************************************************************************
// Настройки использования подсистем Самовара
//**************************************************************************************************************
#define SAMOVAR_USE_BLYNK                    //использовать Blynk в проекте
#define BLYNK_SAMOVAR_TOOL "samovar-tool.ru" //использовать бесплатный сервер Blynk samovar-tool.ru  вместо облачного Blynk
//#define USE_MQTT                             //использовать сохранение логов в облако. Для этого необходимо зарегистрироваться на сайте www.samovar-tool.ru и в редактировании своего профиля указать токен Blynk.
//Все логи с сообщения Самовара будут сохраняться и будут доступны на сайте через меню - Пользователь - Логи.

#define SAMOVAR_USE_POWER                    //использовать регулятор напряжения в проекте https://alcodistillers.ru/forum/viewtopic.php?id=1524
//#define SAMOVAR_USE_RMVK                     //использовать в проекте регулятор напряжения РМВК с управлением по UART
//#define SAMOVAR_USE_SEM_AVR                  //использовать в проекте регулятор мощности https://alcodistillers.ru/forum/viewtopic.php?id=1549 с управлением по UART

#define SAMOVAR_USE_POWER_START_TIME 2000    //задержка перед отправкой команды на разгон регулятору. Нужна для конфигурации, когда регулятор включается контактором, чтобы регулятор успел запуститься

#define USE_WATERSENSOR                      //использовать датчик потока воды охлаждения. Если используется - в случае отсутствия воды в течении заданного времени Самовар отключит нагрев колонны
#define WF_CALIBRATION 98                    //Значение на датчике потока F=98*Q(L/min), для другого датчика необходимо установить другое значение.

#define USE_HEAD_LEVEL_SENSOR                //использовать датчик уровня флегмы в голове (датчик P-N-P). Если используется - в случае срабатывания датчика, снизит напряжение на 3 вольта
//#define WHLS_HIGH_PULL                       //использовать датчик уровня жидкости N-P-N

#define USE_WATER_PUMP                       //использовать управление ШИМ для насоса для воды и/или включения мешалки для режима пива
//#define USE_WATER_VALVE LOW                  //использовать управление клапаном для регулировки напора воды

//#define USE_ALARM_BTN                      //использовать аварийнуу кнопку. Нажатие на нее останавливает подачу воды и отключает питание. Можно использовать, установив датчики протечки (можно несколько параллельно), или обыкновенную кнопку (ее так-же можно включить параллельно датчикам протечки).

//#define USE_BODY_TEMP_AUTOSET              //использовать автоматическую коррекцию Т тела для первой программы отбора тела или предзахлеба после хвостов, а также для программы предзахлеба, если она стоит раньше предпоследней программы отбора тела или предзахлеба

//Необходимо выбрать, какой используется датчик давления, и оставить только одну строку. Другие строки должны быть закоментированы.
//#define USE_BMP180                           //использовать датчик давления BMP180/BMP085
//#define USE_BMP280                           //использовать датчик давления BMP280
//#define USE_BMP280_ALT                       //использовать датчик давления BMP280 с альтернативным адресом
//#define USE_BME280                           //использовать датчик давления BME280
#define USE_BME680                           //использовать датчик давления BME680

#define USE_BTN                              //использовать кнопку

#define USE_UPDATE_OTA                       //использовать ArduinoOTA для обновления через WI-FI. Удобно при отладке, при работе создает лишнюю нагрузку на ESP32

//#define USE_EXPANDER 0x20                    //использовать расширитель портов PCF8575
//#define USE_ANALOG_EXPANDER 0x48             //использовать расширитель аналоговых портов PCF8591
//#define USE_LUA                              //использовать lua для написания скриптов для управления Самоваром и расширителем портов

#endif
