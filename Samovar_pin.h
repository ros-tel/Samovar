#ifndef __SAMOVAR_PIN_H_
#define __SAMOVAR_PIN_H_

//**************************************************************************************************************
// Распиновка
//**************************************************************************************************************

//**************************************************************************************************************
// Пины для BME680, BMP180, BMP280
//#define BME_SCK 22   //SCL
//#define BME_MISO --  //SDO
//#define BME_MOSI 21  //SDA
//#define BME_CS --    //CS
//#define SEALEVELPRESSURE_HPA (1013.25)
//**************************************************************************************************************

#if defined(ARDUINO_ESP32S3_DEV)
#define ENC_CLK 21 //S2
#define ENC_DT 47  //S1
#define ENC_SW 48  //KEY

#define STEPPER_STEP 10
#define STEPPER_DIR 11
#define STEPPER_EN 12

#define RELE_CHANNEL1 4             //используется для пускателя, который включает нагреватель
#define RELE_CHANNEL2 5             //в режиме "Пиво" используется для включения мешалки
#define RELE_CHANNEL3 6             //используется для клапана, открывающего/закрывающего воду охлаждения
#define RELE_CHANNEL4 7             //если не используется регулятор с управлением, выход используется для управления разгоном

#define ONE_WIRE_BUS 35

#define SERVO_PIN 15

#define BTN_PIN 16

#define LUA_PIN 1

#define ALARM_BTN_PIN 2

#define WATERSENSOR_PIN 38

#define WHEAD_LEVEL_SENSOR_PIN 37

#define LCD_SDA 13
#define LCD_SCL 14

#define RXD2 18
#define TXD2 17

#define WATER_PUMP_PIN 36

#define BZZ_PIN 35
#endif

//**************************************************************************************************************
// Пины для Encoder
#ifndef ENC_CLK
#define ENC_CLK 19 //S2
#endif
#ifndef ENC_DT
#define ENC_DT 18  //S1
#endif
#ifndef ENC_SW
#define ENC_SW 23  //KEY
#endif
//**************************************************************************************************************

//**************************************************************************************************************
// Пины для шагового двигателя
#ifndef STEPPER_STEP
#define STEPPER_STEP 26
#endif
#ifndef STEPPER_DIR
#define STEPPER_DIR 32
#endif
#ifndef STEPPER_EN
#define STEPPER_EN 33
#endif
#define STEPPER_STEPS 400 //количество шагов, 200 x MS
#define STEPPER_STEP_ML 1020 //количество шагов на 1 мл жидкости для драйвера с шагами 1/2
#define STEPPER_MAX_SPEED 8000
//**************************************************************************************************************

//**************************************************************************************************************
// Пины для релейного модуля
#ifndef RELE_CHANNEL1
#define RELE_CHANNEL1 2             //используется для пускателя, который включает нагреватель
#endif
#ifndef RELE_CHANNEL2
#define RELE_CHANNEL2 15 //было 34  //в режиме "Пиво" используется для включения мешалки
#endif
#ifndef RELE_CHANNEL3
#define RELE_CHANNEL3 14            //используется для клапана, открывающего/закрывающего воду охлаждения
#endif
#ifndef RELE_CHANNEL4
#define RELE_CHANNEL4 13            //если не используется регулятор с управлением, выход используется для управления разгоном
#endif
//**************************************************************************************************************

//**************************************************************************************************************
// Пин для DS1820
#ifndef ONE_WIRE_BUS
#define ONE_WIRE_BUS 5
#endif
//**************************************************************************************************************

//**************************************************************************************************************
// Пины для сервопривода
#ifndef SERVO_PIN
#define SERVO_PIN 25
#endif
//Максимальный угол сервопривода
#define SERVO_ANGLE 180
// Количество емкостей. (0 используется всегда). Для расчета позиции серво считаем угол поворота между емкостями
// равным 180 / CAPACITY_NUM
#define CAPACITY_NUM 10
//**************************************************************************************************************

//**************************************************************************************************************
// Пин кнопки - не обязательно, но удобно
#ifdef USE_BTN
#ifndef BTN_PIN
#define BTN_PIN 39
#endif
#endif
//#undef BTN_PIN
//**************************************************************************************************************

//**************************************************************************************************************
// Пин для использования в Lua-скриптах
#ifndef LUA_PIN
#define LUA_PIN 34
#endif
//**************************************************************************************************************

//**************************************************************************************************************
// Пин аварийной кнопки
#ifdef USE_ALARM_BTN
#ifndef ALARM_BTN_PIN
#define ALARM_BTN_PIN 35
#endif
#endif
//#undef ALARM_BTN_PIN
//**************************************************************************************************************

//**************************************************************************************************************
// Пины сенсора охлаждения воды - используется для отключения нагрева в случае отсутствия охлаждения. Не обязательно, но безопасно
#ifndef WATERSENSOR_PIN
#define WATERSENSOR_PIN 36
#endif
#ifndef WF_CALIBRATION
#define WF_CALIBRATION 98 //Значение на датчике F=98*Q(L/min)
#endif
#define WF_ALARM_COUNT 20 //Секунд до отключения нагрева, если в течении этого времени не будет подана вода охлаждения
//**************************************************************************************************************

//**************************************************************************************************************
//Температура воды, которую будет поддерживать ШИМ-регулятор. Так же, если при выключении питания температура воды будет меньше TARGET_WATER_TEMP - 15 отключится подача воды
#define TARGET_WATER_TEMP 48
//**************************************************************************************************************

//**************************************************************************************************************
// Пины сенсора уровня флегмы в узле отбора - используется для уменьшения нагрева при начале захлебывания колонны. Не обязательно, но безопасно
#ifndef WHEAD_LEVEL_SENSOR_PIN
#define WHEAD_LEVEL_SENSOR_PIN 27
#endif
#define WHLS_ALARM_TIME 3 //Секунд, через сколько сработает тревога по уровню флегмы
//**************************************************************************************************************

//**************************************************************************************************************
// Установки для экрана: пины, адрес, количество колонок и строк
#ifndef LCD_SDA
#define LCD_SDA 21
#endif

#ifndef LCD_SCL
#define LCD_SCL 22
#endif

#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4
//**************************************************************************************************************

//**************************************************************************************************************
// Пины UART2 для взаимодействия с регулятором напряжения
#ifndef RXD2
#define RXD2 16
#endif

#ifndef TXD2
#define TXD2 17
#endif
//**************************************************************************************************************

//**************************************************************************************************************
// Пин для управления насосом по ШИМ
#ifndef WATER_PUMP_PIN
#define WATER_PUMP_PIN 4
#endif
//**************************************************************************************************************

//**************************************************************************************************************
// Пин для Buzzer
#ifndef BZZ_PIN
#define BZZ_PIN 12
#endif
//**************************************************************************************************************

#endif