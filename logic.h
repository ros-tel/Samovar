#include <Arduino.h>
#include "Samovar.h"

#ifdef SAMOVAR_USE_BLYNK
//#define BLYNK_PRINT Serial

#include <BlynkSimpleEsp32.h>
#endif
//**************************************************************************************************************
// Логика работы ректификационной колонны
//**************************************************************************************************************
void save_profile();
void menu_samovar_start();
void set_menu_screen(byte param);
void samovar_reset();
void create_data();
void pause_withdrawal(bool Pause);
void read_config();
String inline format_float(float v, int d);
float get_temp_by_pressure(float start_pressure, float start_temp, float current_pressure);
unsigned int hexToDec(String hexString);
void set_current_power(float Volt);
void set_power_mode(String Mode);
void open_valve(bool Val);
void stopService(void);
void startService(void);
void reset_sensor_counter(void);
void set_pump_speed(float pumpspeed, bool continue_process);
void set_pump_pwm(float duty);
void set_pump_speed_pid(float temp);
void set_power(bool On);
void set_body_temp();
void set_buzzer(bool fl);
void start_self_test(void);
void stop_self_test(void);
bool check_boiling();
float get_alcohol(float t);
void set_boiling();

#ifdef SAMOVAR_USE_POWER
void check_power_error();
#endif

void SendMsg(String m, MESSAGE_TYPE msg_type);

//Получить количество разделителей
byte IRAM_ATTR getDelimCount(String data, char separator) {
  int cnt = 0;
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex; i++) {
    if (data.charAt(i) == separator) {
      cnt++;
    }
  }
  return cnt;
}

//Получить подстроку через разделитель
String IRAM_ATTR getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

//Получаем объем отбора
float IRAM_ATTR get_liguid_volume_by_step(int StepCount) {
  static float retval;
  if (SamSetup.StepperStepMl > 0) retval = (float)StepCount / SamSetup.StepperStepMl;
  else
    retval = 0;
  return retval;
}

//Получаем скорость отбора
float IRAM_ATTR get_liguid_rate_by_step(int StepperSpeed) {
  return round(get_liguid_volume_by_step(StepperSpeed) * 3.6 * 1000) / 1000.00;
}

float IRAM_ATTR get_speed_from_rate(float volume_per_hour) {
  static float v;
  ActualVolumePerHour = volume_per_hour;
  v = round(SamSetup.StepperStepMl * volume_per_hour * 1000 / 3.6) / 1000.00;
  if (v < 1) v = 1;
  return v;
}

int IRAM_ATTR get_liquid_volume() {
  return get_liguid_volume_by_step(stepper.getCurrent());
}

void set_alarm() {
  // выключаем питание, выключаем воду, взводим флаг аварии
  if (PowerOn) {
    sam_command_sync = SAMOVAR_POWER;
  }
  set_power(false);
  alarm_event = true;
  open_valve(false);
#ifdef USE_WATER_PUMP
  set_pump_pwm(0);
#endif
  SendMsg(F("Аварийное отключение!"), ALARM_MSG);
}

void IRAM_ATTR withdrawal(void) {
  //Определяем, что необходимо сменить режим работы
  //По завершению паузы
  if (program_Pause) {
    if (millis() >= t_min) {
      t_min = 0;
      menu_samovar_start();
    }
    return;
  }

  //По достижению шаговика цели
  CurrrentStepps = stepper.getCurrent();

  if (TargetStepps == CurrrentStepps && TargetStepps != 0 && (startval == 1 || startval == 2)) {
    menu_samovar_start();
  }

  //По превышению температуры в программе
  if (program[ProgramNum].Temp != 0) {
    if (program[ProgramNum].Temp > 0 && program[ProgramNum].Temp < 20) {
      if (SteamSensor.avgTemp > (program[ProgramNum].Temp + SteamSensor.StartProgTemp)) {
        menu_samovar_start();
      }
    } else {
      if (SteamSensor.avgTemp > program[ProgramNum].Temp) {
        menu_samovar_start();
      }
    }
  }

  float c_temp;  //стартовая температура отбора тела с учетом корректировки от давления или без
  c_temp = get_temp_by_pressure(SteamSensor.Start_Pressure, SteamSensor.BodyTemp, bme_pressure);

  //Возвращаем колонну в стабильное состояние, если работает программа отбора тела и температура пара вышла за пределы или корректируем пределы
  if ((program[ProgramNum].WType == "B" || program[ProgramNum].WType == "C") && (SteamSensor.avgTemp >= c_temp + SteamSensor.SetTemp) && SteamSensor.BodyTemp > 0) {
#ifdef USE_BODY_TEMP_AUTOSET
    //Если строка программы - предзахлеб и после есть еще две строки с отбором тела, то корректируем Т тела
    if (program[ProgramNum].WType == "C" && (ProgramNum < ProgramLen - 3)  && (program[ProgramNum + 1].WType == "B" || program[ProgramNum + 1].WType == "C" || program[ProgramNum + 1].WType == "P") && (program[ProgramNum + 2].WType == "B" || program[ProgramNum + 2].WType == "C")) {
      set_body_temp();
    }
    //Если это первая строка с телом - корректируем Т тела
    else if ((ProgramNum > 0) && (program[ProgramNum].WType == "B" || program[ProgramNum].WType == "C") && program[ProgramNum - 1].WType == "H") {
      set_body_temp();
    }
    else
#endif
      //ставим отбор на паузу, если еще не стоит, и задаем время ожидания
      if (!PauseOn && !program_Wait) {
        program_Wait_Type = "(пар)";
        //Если в настройках задан параметр - снижать скорость отбора - снижаем, и напряжение тоже
        if (SamSetup.useautospeed && setautospeed) {
          setautospeed = false;
          CurrrentStepperSpeed = stepper.getSpeed() - round(stepper.getSpeed() / 100 * SamSetup.autospeed);
          set_pump_speed(CurrrentStepperSpeed, false);
#ifdef SAMOVAR_USE_POWER
          if (program[ProgramNum].WType == "B" && SamSetup.useautopowerdown) {
#ifdef SAMOVAR_USE_SEM_AVR
            set_current_power(target_power_volt - target_power_volt / 100 * 3);
#else
            set_current_power(target_power_volt - 3 * PWR_FACTOR);
#endif
          }
#endif
          vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        program_Wait = true;
        pause_withdrawal(true);
        t_min = millis() + SteamSensor.Delay * 1000;
        set_buzzer(true);
        SendMsg(F("Пауза по Т пара"), WARNING_MSG);
      }
    // если время вышло, еще раз пытаемся дождаться
    if (millis() >= t_min) t_min = millis() + SteamSensor.Delay * 1000;
  } else if ((program[ProgramNum].WType == "B" || program[ProgramNum].WType == "C") && SteamSensor.avgTemp < SteamSensor.BodyTemp + SteamSensor.SetTemp && millis() >= t_min && t_min > 0 && program_Wait) {
    //продолжаем отбор
    setautospeed = true;
    t_min = 0;
    program_Wait = false;
    pause_withdrawal(false);
  }

  c_temp = get_temp_by_pressure(SteamSensor.Start_Pressure, PipeSensor.BodyTemp, bme_pressure);
  //Возвращаем колонну в стабильное состояние, если работает программа отбора тела и температура в колонне вышла за пределы или корректируем пределы
  if ((program[ProgramNum].WType == "B" || program[ProgramNum].WType == "C") && (PipeSensor.avgTemp >= c_temp + PipeSensor.SetTemp) && PipeSensor.BodyTemp > 0) {
#ifdef USE_BODY_TEMP_AUTOSET
    if (program[ProgramNum].WType == "C" && (ProgramNum < ProgramLen - 3)  && (program[ProgramNum + 1].WType == "B" || program[ProgramNum + 1].WType == "C" || program[ProgramNum + 1].WType == "P") && (program[ProgramNum + 2].WType == "B" || program[ProgramNum + 2].WType == "C")) {
      set_body_temp();
    }
    //Если это первая строка с телом - корректируем Т тела
    else if ((ProgramNum > 0) && (program[ProgramNum].WType == "B" || program[ProgramNum].WType == "C") && program[ProgramNum - 1].WType == "H") {
      set_body_temp();
    }
    else
#endif
      //ставим отбор на паузу, если еще не стоит, и задаем время ожидания
      if (!PauseOn && !program_Wait) {
        program_Wait_Type = "(царга)";
        //Если в настройках задан параметр - снижать скорость отбора - снижаем, и напряжение тоже
        if (SamSetup.useautospeed && setautospeed) {
          setautospeed = false;
          CurrrentStepperSpeed = stepper.getSpeed() - round(stepper.getSpeed() / 100 * SamSetup.autospeed);
          set_pump_speed(CurrrentStepperSpeed, false);
#ifdef SAMOVAR_USE_POWER
          if (program[ProgramNum].WType == "B" && SamSetup.useautopowerdown) {
#ifdef SAMOVAR_USE_SEM_AVR
            set_current_power(target_power_volt - target_power_volt / 100 * 3);
#else
            set_current_power(target_power_volt - 3 * PWR_FACTOR);
#endif
          }
#endif
          vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        program_Wait = true;
        pause_withdrawal(true);
        t_min = millis() + PipeSensor.Delay * 1000;
        set_buzzer(true);
        SendMsg(F("Пауза по Т царги"), WARNING_MSG);
      }
    // если время вышло, еще раз пытаемся дождаться
    if (millis() >= t_min) t_min = millis() + PipeSensor.Delay * 1000;
  } else if ((program[ProgramNum].WType == "B" || program[ProgramNum].WType == "C") && PipeSensor.avgTemp < PipeSensor.BodyTemp + PipeSensor.SetTemp && millis() >= t_min && t_min > 0 && program_Wait) {
    //продолжаем отбор
    setautospeed = true;
    t_min = 0;
    program_Wait = false;
    pause_withdrawal(false);
  }
}

void pump_calibrate(int stpspeed) {
  if (startval != 0 && startval != 100) {
    return;
  }

  if (stpspeed == 0) {
    startval = 0;
    //Сохраняем полученное значение калибровки
    SamSetup.StepperStepMl = round((float)stepper.getCurrent() / 100);
    stopService();
    stepper.brake();
    stepper.disable();
    save_profile();
    read_config();
  } else {
    startval = 100;
    //крутим двигатель, пока не остановят
    if (!stepper.getState()) stepper.setCurrent(0);
    stepper.setMaxSpeed(stpspeed);
    stepper.setSpeed(stpspeed);
    stepper.setTarget(999999999);
    startService();
  }
}

void IRAM_ATTR pause_withdrawal(bool Pause) {
  if (Samovar_Mode != SAMOVAR_RECTIFICATION_MODE) return;
  if (!stepper.getState() && !PauseOn) return;
  PauseOn = Pause;
  if (Pause) {
    TargetStepps = stepper.getTarget();
    CurrrentStepps = stepper.getCurrent();
    CurrrentStepperSpeed = stepper.getSpeed();
    stopService();
    stepper.brake();
    stepper.disable();
  } else {
    stepper.setMaxSpeed(CurrrentStepperSpeed);
    stepper.setSpeed(CurrrentStepperSpeed);
    stepper.setCurrent(CurrrentStepps);
    stepper.setTarget(TargetStepps);
    startService();
  }
}

void IRAM_ATTR set_pump_speed(float pumpspeed, bool continue_process) {
  if (pumpspeed < 1) return;
  if (!(SamovarStatusInt == 10 || SamovarStatusInt == 15 || SamovarStatusInt == 40)) return;

  bool cp = continue_process;
  if (!stepper.getState()) cp = false;

  CurrrentStepperSpeed = pumpspeed;
  ActualVolumePerHour = get_liguid_rate_by_step(CurrrentStepperSpeed);

  stopService();
  stepper.setMaxSpeed(CurrrentStepperSpeed);
  stepper.setSpeed(CurrrentStepperSpeed);
  //Пересчитываем время отбора этой строки программы на текущую скорость
  if (ActualVolumePerHour == 0) program[ProgramNum].Time = 65535;
  else
    program[ProgramNum].Time = program[ProgramNum].Volume / ActualVolumePerHour / 1000;
  if (cp)
    startService();
}

String IRAM_ATTR get_Samovar_Status() {
  if (!PowerOn) {
    SamovarStatus = F("Выключено");
    SamovarStatusInt = 0;
  } else if (PowerOn && startval == 1 && !PauseOn && !program_Wait) {
    SamovarStatus = "Прг №" + String(ProgramNum + 1);
    SamovarStatusInt = 10;
  } else if (PowerOn && startval == 1 && program_Wait) {
    int s = 0;
    if (t_min > (millis() + 10)) {
      s = (t_min - millis()) / 1000;
    }
    SamovarStatus = "Прг №" + String(ProgramNum + 1) + " пауза " + program_Wait_Type + ". Продолжение через " + (String)s + " сек.";
    SamovarStatusInt = 15;
  } else if (PowerOn && startval == 2) {
    SamovarStatus = F("Выполнение программы завершено");
    SamovarStatusInt = 20;
  } else if (PowerOn && startval == 100) {
    SamovarStatus = F("Калибровка");
    SamovarStatusInt = 30;
  } else if (PauseOn) {
    SamovarStatus = F("Пауза");
    SamovarStatusInt = 40;
  } else if (PowerOn && startval == 0 && !stepper.getState()) {
    if (SamovarStatusInt != 51 && SamovarStatusInt != 52) {
      SamovarStatus = F("Разгон колонны");
      SamovarStatusInt = 50;
    } else if (SamovarStatusInt == 51) {
      SamovarStatus = F("Разгон завершен. Стабилизация/Работа на себя");
    } else if (SamovarStatusInt == 52) {
      SamovarStatus = F("Стабилизация завершена/Работа на себя");
    }
  } else if (SamovarStatusInt == 1000) {
    SamovarStatus = F("Режим дистилляции");
  } else if (SamovarStatusInt == 3000) {
    SamovarStatus = F("Режим бражной колонны");
  } else if (SamovarStatusInt == 2000) {
#ifdef SAM_BEER_PRG
    SamovarStatus = "Прг №" + String(ProgramNum + 1) + "; ";
#else
    SamovarStatus = "";
#endif
    if (startval == 2001 && program[ProgramNum].WType == "M") {
      SamovarStatus = SamovarStatus + "Разогрев до температуры засыпи солода";
    } else if (startval == 2002 && program[ProgramNum].WType == "M") {
      SamovarStatus = SamovarStatus + "Ожидание засыпи солода";
    } else if (program[ProgramNum].WType == "P") {
      if (begintime == 0) {
        SamovarStatus = SamovarStatus + "Нагрев до " + String(program[ProgramNum].Temp);
      } else {
        SamovarStatus = SamovarStatus + "Пауза " + String(program[ProgramNum].Time - (millis() - begintime) / 1000 / 60) + " мин";
      }
    } else if (program[ProgramNum].WType == "C") {
      SamovarStatus = SamovarStatus + "Охлаждение до " + String(program[ProgramNum].Temp);
    } else if (program[ProgramNum].WType == "W") {
      SamovarStatus = SamovarStatus + "Ожидание. Нажмите 'Следующая программа'";
    } else if (program[ProgramNum].WType == "A") {
      SamovarStatus = SamovarStatus + "Автокалибровка. После завершения питание будет выключено";
    } else if (program[ProgramNum].WType == "B") {
      if (begintime == 0) {
        SamovarStatus = SamovarStatus + "; Кипячение - нагрев";
      } else {
        SamovarStatus = SamovarStatus + "; Кипячение " + String(program[ProgramNum].Time - (millis() - begintime) / 1000 / 60) + " мин";
      }
    }
  }

  if (SamovarStatusInt == 10 || SamovarStatusInt == 15 || (SamovarStatusInt == 2000 && PowerOn)) {
    SamovarStatus += "; Осталось:" + WthdrwTimeS + "|" + WthdrwTimeAllS;
  }
  if (SteamSensor.BodyTemp > 0) SamovarStatus += ";Т тела пар:" + format_float(get_temp_by_pressure(SteamSensor.Start_Pressure, SteamSensor.BodyTemp, bme_pressure), 3) + ";Т тела царга:" + format_float(get_temp_by_pressure(SteamSensor.Start_Pressure, PipeSensor.BodyTemp, bme_pressure), 3);

  return SamovarStatus;
}

void IRAM_ATTR set_capacity(byte cap) {
  capacity_num = cap;

#ifdef SERVO_PIN
  int p = ((int)cap * SERVO_ANGLE) / (int)CAPACITY_NUM + servoDelta[cap];
  servo.write(p);
#elif USER_SERVO
  user_set_capacity(cap);
#endif
}

void IRAM_ATTR next_capacity(void) {
  set_capacity(capacity_num + 1);
}

void set_program(String WProgram) {
  //  WProgram.trim();
  //  if (WProgram = "") return;
  char c[500];
  WProgram.toCharArray(c, 500);
  char *pair = strtok(c, ";");
  int i = 0;
  while (pair != NULL and i < CAPACITY_NUM * 2) {
    program[i].WType = pair;
    pair = strtok(NULL, ";");
    program[i].Volume = atoi(pair);
    pair = strtok(NULL, ";");
    program[i].Speed = atof(pair);
    pair = strtok(NULL, ";");
    program[i].capacity_num = atoi(pair);
    pair = strtok(NULL, ";");
    program[i].Temp = atof(pair);
    pair = strtok(NULL, "\n");
    program[i].Power = atof(pair);
    if (program[i].WType == "P") {
      program[i].Time = program[i].Volume / 60 / (float)60;
    } else {
      program[i].Time = program[i].Volume / program[i].Speed / 1000;
    }
    i++;
    ProgramLen = i;
    pair = strtok(NULL, ";");
    if ((!pair || pair == NULL || pair[0] == 13) and i < CAPACITY_NUM * 2) {
      program[i].WType = "";
      break;
    }
  }
}

String get_program(byte s) {
  String Str = "";
  byte k = CAPACITY_NUM * 2;
  if (s == CAPACITY_NUM * 2) {
    s = 0;
  } else {
    k = s + 1;
  }
  for (byte i = s; i < k; i++) {
    if (program[i].WType == "") {
      i = CAPACITY_NUM * 2 + 1;
    } else {
      Str += program[i].WType + ";";
      Str += (String)program[i].Volume + ";";
      Str += (String)program[i].Speed + ";";
      Str += (String)program[i].capacity_num + ";";
      Str += (String)program[i].Temp + ";";
      Str += (String)program[i].Power + "\n";
    }
  }
  return Str;
}

void IRAM_ATTR run_program(byte num) {
  t_min = 0;
  program_Pause = false;
  program_Wait = false;
  PauseOn = false;
  pause_withdrawal(false);

  //запоминаем текущие значения температур
  SteamSensor.StartProgTemp = SteamSensor.avgTemp;
  PipeSensor.StartProgTemp = PipeSensor.avgTemp;
  WaterSensor.StartProgTemp = WaterSensor.avgTemp;
  TankSensor.StartProgTemp = TankSensor.avgTemp;

  if (SamSetup.ChangeProgramBuzzer) {
    set_buzzer(true);
  }
  if (num == CAPACITY_NUM * 2) {
    //если num = CAPACITY_NUM * 2 значит мы достигли финала (или отбор сброшен принудительно), завершаем отбор
    ProgramNum = 0;
    stopService();
    stepper.brake();
    stepper.disable();
    stepper.setCurrent(0);
    stepper.setTarget(0);
    set_capacity(0);
    if (fileToAppend) {
      fileToAppend.close();
    }
    set_power(false);
    SendMsg(F("Выполнение программы завершено."), NOTIFY_MSG);
  } else {
#ifdef SAMOVAR_USE_POWER
#ifdef SAMOVAR_USE_SEM_AVR
    if (abs(program[num].Power) > 400 && program[num].Power > 0) {
#else
    if (abs(program[num].Power) > 40 && program[num].Power > 0) {
#endif
      set_current_power(program[num].Power);
    } else if (program[num].Power != 0) {
      set_current_power(target_power_volt + program[num].Power);
    }
    if (program[num].WType == "C" && alarm_c_low_min == 0) alarm_c_low_min = millis();
#endif
    String p_s;
    p_s = "Программа: старт строки  №" + (String)(num + 1);
    if (program[num].WType == "H" || program[num].WType == "B" || program[num].WType == "T" || program[num].WType == "C") {
      if (program[num].WType == "H" || program[num].WType == "T") {
        SteamSensor.BodyTemp = 0;
        PipeSensor.BodyTemp = 0;
        WaterSensor.BodyTemp = 0;
        TankSensor.BodyTemp = 0;
      }
      p_s += ", отбор в ёмкость " + (String)program[num].capacity_num;
      //устанавливаем параметры для текущей программы отбора
      set_capacity(program[num].capacity_num);
      stepper.setMaxSpeed(get_speed_from_rate(program[num].Speed));
      stepper.setSpeed(get_speed_from_rate(program[num].Speed));
      TargetStepps = program[num].Volume * SamSetup.StepperStepMl;
      stepper.setCurrent(0);
      stepper.setTarget(TargetStepps);
      startService();
      ActualVolumePerHour = program[num].Speed;
      if ((program[num].WType == "B" || program[num].WType == "C") && program[num].Temp > 0) {
        SteamSensor.BodyTemp = program[num].Temp;
      }

      //Если у первой программы отбора тела не задана температура, при которой начинать отбор, считаем, что она равна текущей
      //Считаем, что колонна стабильна
      //Итак, текущая температура - это температура, которой Самовар будет придерживаться во время всех программ отобора тела.
      //Если она будет выходить за пределы, заданные в настройках, отбор будет ставиться на паузу, и продолжится после возвращения температуры в колонне к заданному значению.
      if ((program[num].WType == "B" || program[num].WType == "C") && SteamSensor.BodyTemp == 0) {
        set_body_temp();
      }
    } else if (program[num].WType == "P") {
      //Сбрасываем Т тела, так как при изменении напряжения на регуляторе изменяется Т в царге.
      SteamSensor.BodyTemp = 0;
      PipeSensor.BodyTemp = 0;
      WaterSensor.BodyTemp = 0;
      TankSensor.BodyTemp = 0;

      //устанавливаем параметры ожидания для программы паузы. Время в секундах задано в program[num].Volume
      p_s += ", пауза " + (String)program[num].Volume + " сек.";
      t_min = millis() + program[num].Volume * 1000;
      program_Pause = true;
      stopService();
      stepper.setMaxSpeed(-1);
      stepper.setSpeed(-1);
      stepper.brake();
      stepper.disable();
      stepper.setCurrent(0);
      stepper.setTarget(0);
    }
    SendMsg(p_s, NOTIFY_MSG);
  }
  TargetStepps = stepper.getTarget();
}

//функция корректировки температуры кипения спирта в зависимости от давления
float IRAM_ATTR get_temp_by_pressure(float start_pressure, float start_temp, float current_pressure) {
  if (start_temp == 0) return 0;
  if (current_pressure < 10) return start_temp;

  //скорректированная температура кипения спирта при текущем давлении
  static float c_temp;

  if (SamSetup.UsePreccureCorrect) {
    //идеальная температура кипения спирта при текущем давлении
    float i_temp;
    //температурная дельта
    float d_temp;

    i_temp = current_pressure * 0.038 + 49.27;

    if (start_pressure == 0) {
      d_temp = start_temp - 78.15;
    } else {
      d_temp = start_temp - start_pressure * 0.038 - 49.27;  //учитываем поправку на погрешность измерения датчиков
    }
    c_temp = i_temp + d_temp;                              // получаем текущую температуру кипения при переданном давлении с учетом поправки
  } else {
    //Используем сохраненную температуру отбора тела без корректировки
    c_temp = start_temp;
  }

  return c_temp;
}

void IRAM_ATTR set_body_temp() {
  if (program[ProgramNum].WType == "B" || program[ProgramNum].WType == "C" || program[ProgramNum].WType == "P") {
    SteamSensor.Start_Pressure = bme_pressure;
    SteamSensor.BodyTemp = SteamSensor.avgTemp;
    PipeSensor.BodyTemp = PipeSensor.avgTemp;
    WaterSensor.BodyTemp = WaterSensor.avgTemp;
    TankSensor.BodyTemp = TankSensor.avgTemp;
    SendMsg("Новые Т тела: пар = " + String(SteamSensor.BodyTemp) + ", царга = " + String(PipeSensor.BodyTemp), WARNING_MSG);
  } else {
    SendMsg(F("Не возможно установить Т тела."), WARNING_MSG);
  }
}

void IRAM_ATTR check_alarm() {
  //сбросим паузу события безопасности
  if (alarm_t_min > 0 && alarm_t_min <= millis()) alarm_t_min = 0;

#ifdef SAMOVAR_USE_POWER
  //управляем разгонным тэном
  if (SamovarStatusInt == 50 && TankSensor.avgTemp <= OPEN_VALVE_TANK_TEMP && PowerOn) {
    if (!acceleration_heater) {
      //включаем разгонный тэн
      digitalWrite(RELE_CHANNEL4, SamSetup.rele4);
      acceleration_heater = true;
    }
  } else {
    if (acceleration_heater) {
      //выключаем разгонный тэн
      digitalWrite(RELE_CHANNEL4, !SamSetup.rele4);
      acceleration_heater = false;
    }
  }
#endif

  //Если используется датчик уровня флегмы в голове
#ifdef USE_HEAD_LEVEL_SENSOR
  whls.tick();
  if (whls.isHolded() && alarm_h_min == 0) {
    whls.resetStates();
    if (program[ProgramNum].WType != "C") {
      set_buzzer(true);
      SendMsg(F("Сработал датчик захлёба!"), ALARM_MSG);
#ifdef SAMOVAR_USE_POWER
      alarm_c_min = 0;
      alarm_c_low_min = 0;
      prev_target_power_volt = 0;
#endif
    } else {
#ifdef SAMOVAR_USE_POWER
      //запускаем счетчик - TIME_C минут, нужен для возврата заданного напряжения
      alarm_c_min = millis() + 1000 * 60 * TIME_C / 5;
      //счетчик для повышения напряжения сбрасываем
      alarm_c_low_min = 0;
      if (prev_target_power_volt == 0) prev_target_power_volt = target_power_volt;
#endif
    }
#ifdef SAMOVAR_USE_POWER
    SendMsg((String)PWR_MSG + " снижаем с " + (String)target_power_volt, NOTIFY_MSG);
#ifdef SAMOVAR_USE_SEM_AVR
    set_current_power(target_power_volt - target_power_volt / 100 * 2);
#else
    set_current_power(target_power_volt - 1 * PWR_FACTOR);
#endif
#endif
    //Если уже реагировали - надо подождать 40 секунд, так как процесс инерционный
    alarm_h_min = millis() + 1000 * 40;
  }

  if (alarm_h_min > 0 && alarm_h_min <= millis()) {
    whls.resetStates();
    alarm_h_min = 0;
  }
#ifdef SAMOVAR_USE_POWER
  //Если программа - предзахлеб, и сброс напряжения был больше TIME_C минут назад, то возвращаем напряжение к последнему сохраненному - 0.5
  if (alarm_c_min > 0 && alarm_c_min <= millis()) {
    if (program[ProgramNum].WType == "C") {
      if (prev_target_power_volt == 0) {
#ifdef SAMOVAR_USE_SEM_AVR
        prev_target_power_volt = target_power_volt + target_power_volt / 100 * 4;
#else
        prev_target_power_volt = target_power_volt + 2 * PWR_FACTOR;
#endif
      }
#ifdef SAMOVAR_USE_SEM_AVR
      set_current_power(prev_target_power_volt - target_power_volt / 100 * 3);
#else
      set_current_power(prev_target_power_volt - 1 * PWR_FACTOR);
#endif
      prev_target_power_volt = 0;
      //запускаем счетчик - TIME_C минут, нужен для повышения текущего напряжения чтобы поймать предзахлеб
      alarm_c_low_min = millis() + 1000 * 60 * TIME_C;
    }
    alarm_c_min = 0;
  }
  //Если программа предзахлеб и давно не было срабатывания датчика - повышаем напряжение
  if (program[ProgramNum].WType == "C") {
    if (alarm_c_low_min > 0 && alarm_c_low_min <= millis()) {
#ifdef SAMOVAR_USE_SEM_AVR
      set_current_power(target_power_volt + target_power_volt / 100 * 1);
#else
      set_current_power(target_power_volt + 0.5 * PWR_FACTOR);
#endif
      alarm_c_low_min = millis() + 1000 * 60 * TIME_C;
    } else if (alarm_c_low_min == 0 && alarm_c_min == 0) {
      alarm_c_low_min = millis() + 1000 * 60 * TIME_C;
    }
  } else alarm_c_low_min = 0;

#endif
  //Если используется датчик уровня флегмы в голове
#endif


#ifdef SAMOVAR_USE_POWER
  check_power_error();
#endif

  if (!valve_status) {
    if (ACPSensor.avgTemp >= MAX_ACP_TEMP - 5) open_valve(true);
    else if (TankSensor.avgTemp >= OPEN_VALVE_TANK_TEMP && PowerOn) {
      set_buzzer(true);
      open_valve(true);
    }
  }

  if (!PowerOn && !is_self_test && valve_status && WaterSensor.avgTemp <= SamSetup.SetWaterTemp - DELTA_T_CLOSE_VALVE && ACPSensor.avgTemp <= MAX_ACP_TEMP - 10) {
    open_valve(false);
    set_buzzer(true);
#ifdef USE_WATER_PUMP
    if (pump_started) set_pump_pwm(0);
#endif
  }

  //Определяем, что началось кипение - вода охлаждения начала нагреваться
  //check_boiling();

#ifdef USE_WATER_PUMP
  //Устанавливаем ШИМ для насоса в зависимости от температуры воды
  if (valve_status) {
    if (ACPSensor.avgTemp > 39 && ACPSensor.avgTemp > WaterSensor.avgTemp) set_pump_speed_pid(SamSetup.SetWaterTemp + 3);
    else
      set_pump_speed_pid(WaterSensor.avgTemp);
  }
#endif

  //Проверяем, что температурные параметры не вышли за предельные значения
  if ((SteamSensor.avgTemp >= MAX_STEAM_TEMP || WaterSensor.avgTemp >= MAX_WATER_TEMP || TankSensor.avgTemp >= SamSetup.DistTemp || ACPSensor.avgTemp >= MAX_ACP_TEMP) && PowerOn) {
    //Если с температурой проблемы - выключаем нагрев, пусть оператор разбирается
    delay(1000);  //Пауза на всякий случай, чтобы прошли все другие команды
    set_buzzer(true);
    set_power(false);
    String s = "";
    if (SteamSensor.avgTemp >= MAX_STEAM_TEMP) s = s + " Пара";
    else if (WaterSensor.avgTemp >= MAX_WATER_TEMP)
      s = s + " Воды";
    else if (ACPSensor.avgTemp >= MAX_ACP_TEMP)
      s = s + " ТСА";

    if (TankSensor.avgTemp >= SamSetup.DistTemp) {
      //Если температура в кубе превысила заданную, штатно завершаем ректификацию.
      SendMsg(F("Лимит максимальной температуры куба. Программа завершена."), NOTIFY_MSG);
    } else
      SendMsg("Аварийное отключение! Превышена максимальная температура" + s, ALARM_MSG);
  }

#ifdef USE_WATERSENSOR
  //Проверим, что вода подается
  if (WFAlarmCount > WF_ALARM_COUNT && PowerOn) {
    set_buzzer(true);
    //Если с водой проблемы - выключаем нагрев, пусть оператор разбирается
    sam_command_sync = SAMOVAR_POWER;
    SendMsg(F("Аварийное отключение! Прекращена подача воды."), ALARM_MSG);
  }
#endif

  if ((WaterSensor.avgTemp >= ALARM_WATER_TEMP - 5) && PowerOn && alarm_t_min == 0) {
    set_buzzer(true);
    //Если уже реагировали - надо подождать 30 секунд, так как процесс инерционный
    SendMsg(F("Критическая температура воды!"), WARNING_MSG);

#ifdef SAMOVAR_USE_POWER
    if (WaterSensor.avgTemp >= ALARM_WATER_TEMP) {
      set_buzzer(true);
      SendMsg("Критическая температура воды! Ошибка подачи воды. " + (String)PWR_MSG + " снижаем с " + (String)target_power_volt, ALARM_MSG);
      //Попробуем снизить напряжение регулятора на 5 вольт, чтобы исключить перегрев колонны.
#ifdef SAMOVAR_USE_SEM_AVR
      set_current_power(target_power_volt - target_power_volt / 100 * 8);
#else
      set_current_power(target_power_volt - 5 * PWR_FACTOR);
#endif
    }
#endif
    alarm_t_min = millis() + 1000 * 30;
  }

  if (SamovarStatusInt == 50 && SteamSensor.avgTemp >= CHANGE_POWER_MODE_STEAM_TEMP) {
#ifdef USE_WATER_PUMP
    //Сбросим счетчик насоса охлаждения, что приведет к увеличению потока воды. Дальше уже будет штатно работать PID
    wp_count = 0;
#endif
    //достигли заданной температуры на разгоне, переходим на рабочий режим, устанавливаем заданную температуру, зовем оператора
    SamovarStatusInt = 51;

    SendMsg("Разгон завершён. Стабилизация/работа на себя.", NOTIFY_MSG);
    set_buzzer(true);
#ifdef SAMOVAR_USE_POWER
    set_current_power(program[0].Power);
#else
    current_power_mode = POWER_WORK_MODE;
    digitalWrite(RELE_CHANNEL4, !SamSetup.rele4);
#endif
  }

  if (SamovarStatusInt == 51 && !boil_started) {
    set_boiling();
    if (boil_started) {
      SendMsg("Спиртуозность " + format_float(alcohol_s, 1), WARNING_MSG);
    }
  }

  //Разгон и стабилизация завершены - шесть минут температура пара не меняется больше, чем на 0.1 градус:
  //https://alcodistillers.ru/forum/viewtopic.php?id=137 - указано 3 замера раз в три минуты.
  if ((SamovarStatusInt == 51 || SamovarStatusInt == 52) && SteamSensor.avgTemp > CHANGE_POWER_MODE_STEAM_TEMP) {
    float d = SteamSensor.avgTemp - SteamSensor.PrevTemp;
    d = abs(d);
    if (d < 0.1) {
      acceleration_temp += 1;
      if (acceleration_temp == 60 * 6) {
        SamovarStatusInt = 52;
        set_buzzer(true);
        SendMsg(F("Стабилизация завершена, колонна работает стабильно."), NOTIFY_MSG);
      }
    } else {
      acceleration_temp = 0;
      SteamSensor.PrevTemp = SteamSensor.avgTemp;
    }
  }
#ifdef USE_WATER_VALVE
  if (WaterSensor.avgTemp >= SamSetup.SetWaterTemp + 1) {
    digitalWrite(WATER_PUMP_PIN, USE_WATER_VALVE);
  } else if (WaterSensor.avgTemp <= SamSetup.SetWaterTemp - 1) {
    digitalWrite(WATER_PUMP_PIN, !USE_WATER_VALVE);
  }
#endif
}

void IRAM_ATTR open_valve(bool Val) {
  if (Val && !alarm_event) {
    valve_status = true;
    SendMsg(F("Откройте подачу воды!"), WARNING_MSG);
    digitalWrite(RELE_CHANNEL3, SamSetup.rele3);
  } else {
    valve_status = false;
    SendMsg(F("Закройте подачу воды!"), WARNING_MSG);
    digitalWrite(RELE_CHANNEL3, !SamSetup.rele3);
  }
}

void IRAM_ATTR triggerBuzzerTask(void *parameter) {
  TickType_t beep = 400 / portTICK_RATE_MS;
  TickType_t silent = 600 / portTICK_RATE_MS;
  int tick_buzz = 0;

  while (true) {
    if (BuzzerTaskFl) {
      digitalWrite(BZZ_PIN, HIGH);
      vTaskDelay(beep / portTICK_PERIOD_MS);
      digitalWrite(BZZ_PIN, LOW);
      tick_buzz++;
      if (tick_buzz > 5) BuzzerTaskFl = false;
    }
    vTaskDelay(silent / portTICK_PERIOD_MS);
  }
}

void IRAM_ATTR set_buzzer(bool fl) {
  if (fl && SamSetup.UseBuzzer) {
    if (BuzzerTask == NULL) {
      BuzzerTaskFl = true;
      //Запускаем таск для пищалки
      xTaskCreatePinnedToCore(
        triggerBuzzerTask, /* Function to implement the task */
        "BuzzerTask",      /* Name of the task */
        800,               /* Stack size in words */
        NULL,              /* Task input parameter */
        0,                 /* Priority of the task */
        &BuzzerTask,       /* Task handle. */
        1);                /* Core where the task should run */
    }
  } else {
    if (BuzzerTask != NULL && !BuzzerTaskFl) {
      vTaskDelete(BuzzerTask);
      BuzzerTask = NULL;
      digitalWrite(BZZ_PIN, LOW);
    }
  }
}

void IRAM_ATTR set_power(bool On) {
  if (alarm_event && On) {
    return;
  }
  PowerOn = On;
  if (On) {
    digitalWrite(RELE_CHANNEL1, SamSetup.rele1);
    set_menu_screen(2);
    power_text_ptr = (char *)"OFF";

#ifdef SAMOVAR_USE_SEM_AVR
    vTaskDelay(3000 / portTICK_PERIOD_MS);
#endif

#ifdef SAMOVAR_USE_POWER
    vTaskDelay(SAMOVAR_USE_POWER_START_TIME / portTICK_PERIOD_MS);
    set_power_mode(POWER_SPEED_MODE);
#else
    current_power_mode = POWER_SPEED_MODE;
    digitalWrite(RELE_CHANNEL4, SamSetup.rele4);
#endif
  } else {
    digitalWrite(RELE_CHANNEL4, !SamSetup.rele4);
    acceleration_heater = false;
#ifdef SAMOVAR_USE_POWER
    vTaskDelay(500 / portTICK_PERIOD_MS);
    set_power_mode(POWER_SLEEP_MODE);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
#else
    current_power_mode = POWER_SLEEP_MODE;
#endif
    power_text_ptr = (char *)"ON";
    sam_command_sync = SAMOVAR_RESET;
    digitalWrite(RELE_CHANNEL1, !SamSetup.rele1);
  }
}

float get_steam_alcohol(float t) {
  if (!boil_started) return 100;

  static float r;
  float t1;
  float s;
  float k;
  byte t0;

  t1 = t;
  t = get_temp_by_pressure(0, t, bme_pressure);

  if (t >= 99 && t < 99.84) {
    s = 11.21;
    k = -13;
    t0 = 99;
  } else if (t >= 98 && t < 99) {
    s = 20.744;
    k = -9.84;
    t0 = 98;
  } else if (t >= 97 && t < 98) {
    s = 29.936;
    k = -9;
    t0 = 97;
  } else if (t >= 96 && t < 97) {
    s = 38.36;
    k = -8.2;
    t0 = 96;
  } else if (t >= 95 && t < 96) {
    s = 43.877647;
    k = -4.847;
    t0 = 95;
  } else if (t >= 94 && t < 95) {
    s = 49.2775;
    k = -4.65;
    t0 = 94;
  } else if (t >= 93 && t < 94) {
    s = 53.76;
    k = -4.483;
    t0 = 93;
  } else if (t >= 92 && t < 93) {
    s = 57.539;
    k = -3.778;
    t0 = 92;
  } else if (t >= 91 && t < 92) {
    s = 61.22;
    k = -3.682;
    t0 = 91;
  } else if (t >= 90 && t < 91) {
    s = 66.4633;
    k = -5.244;
    t0 = 90;
  } else if (t >= 89 && t < 90) {
    s = 67.74;
    k = -5.75;
    t0 = 89;
  } else if (t >= 88 && t < 89) {
    s = 69.2257;
    k = -1.4857;
    t0 = 88;
  } else if (t >= 87 && t < 88) {
    s = 72.42;
    k = -1.6;
    t0 = 87;
  } else if (t >= 86 && t < 87) {
    s = 75.03;
    k = -2.66;
    t0 = 86;
  } else if (t >= 85 && t < 86) {
    s = 77.21;
    k = -2.2;
    t0 = 85;
  } else if (t >= 84 && t < 85) {
    s = 79.35;
    k = -1;
    t0 = 84;
  } else if (t >= 83 && t < 84) {
    s = 81.08;
    k = -1.2;
    t0 = 83;
  } else {
    s = 82;
    k = -1;
    t0 = 82;
  }

  if (t > 100) {
    r = 0;
  }
  else if (t > 99.84) {
    r = get_alcohol(t1);
  }
  else {
    r = s + k * (t - t0);
  }
  if (r < 0) r = 0;
  return r;
}

float get_alcohol(float t) {
  if (!boil_started) return 100;
  t = get_temp_by_pressure(0, t, bme_pressure);
  static float r;
  float k;
  k = (t - 89) / 6.49;

  r = 17.26 - k * (18.32 - k * (7.81 - k * (1.77 - k * (4.81 - k * (2.95 + k * (1.43 - k * (0.8 + 0.05 * k) )))))); // формула Макеода для вычисления крепости
  if (r < 0) r = 0;
  r = float (round (r * 10)) / 10; // округляем до одного знака после запятой
  return r;

  //reverse
  //t[град]=85,37 - 3,75 * Ti + 1,48 * Ti ^ 2 - 0,32 * Ti ^ 3 +0,41 * Ti ^ 4 - 0,92 * Ti ^ 5 +0,32 * Ti ^ 6 + 0,1 * Ti ^ 7 - 0,05 * Ti ^ 8
  //Где  Ti = ( К%об - 43,15 ) / 30,18
  //более точно
  // Темп=105.47* крепость^-0.065 - применима для растворов крепче 2%мас.

}

void set_boiling() {
  //Учитываем задержку измерения Т кипения
  if (!boil_started) {
    if (abs(TankSensor.avgTemp - b_t_temp_prev) > 0.1) {
      b_t_temp_prev = TankSensor.avgTemp;
      b_t_time_min = millis();
    } else if ((millis() - b_t_time_min) > 5 * 1000) {
      //6 секунд не было изменения температуры куба
      //d_s_temp_finish = 0;
      //d_s_time_min = 0;
      //началось кипение, запоминаем Т кипения
      boil_started = true;
      boil_temp = TankSensor.avgTemp;
      alcohol_s = get_alcohol(TankSensor.avgTemp);
    }
  }
}

bool check_boiling() {
  if (boil_started || !PowerOn) {
    return false;
  }
  //Определяем, что началось кипение - вода охлаждения начала нагреваться
  if (d_s_temp_prev > WaterSensor.avgTemp || d_s_temp_prev == 0) {
    d_s_temp_prev = WaterSensor.avgTemp;
  }
  if (WaterSensor.avgTemp - d_s_temp_prev > 10 || SteamSensor.avgTemp > CHANGE_POWER_MODE_STEAM_TEMP) {
#ifdef USE_WATER_PUMP
    wp_count = -10;
#endif
    set_boiling();
  }
  if (abs(WaterSensor.avgTemp - SamSetup.SetWaterTemp) < 5) {
#ifdef USE_WATER_PUMP
    //Началось кипение, значит надо времено увеличить подачу воды (на всякий случай)
    wp_count = -10;
#endif
    set_boiling();
  }
  if (boil_started) {
    SendMsg("Началось кипение в кубе! Спиртуозность " + format_float(alcohol_s, 1), WARNING_MSG);
  }
  return boil_started;
}

void start_self_test(void) {
  is_self_test = true;
  SendMsg(F("Запуск самотестирования."), NOTIFY_MSG);
  open_valve(true);
#ifdef USE_WATER_PUMP
  //включаем насос воды
  set_pump_pwm(PWM_START_VALUE * 10);
#endif
  //включаем шаговый двигатель
  stopService();
  stepper.setMaxSpeed(get_speed_from_rate(1));
  stepper.setSpeed(get_speed_from_rate(1));
  TargetStepps = 100 * SamSetup.StepperStepMl;
  stepper.setCurrent(0);
  stepper.setTarget(TargetStepps);
  startService();
  //включаем сервопривод
  set_capacity(1);
  while (capacity_num != 0 && capacity_num < 5) {
    vTaskDelay(1500 / portTICK_PERIOD_MS);
    next_capacity();
  }
  vTaskDelay(10000 / portTICK_PERIOD_MS);
  stop_self_test();
  SendMsg(F("Самотестирование закончено."), NOTIFY_MSG);
}

void stop_self_test(void) {
#ifdef USE_WATER_PUMP
  //выключаем насос воды
  set_pump_pwm(0);
#endif
  open_valve(false);
  set_capacity(0);
  stopService();
  is_self_test = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SAMOVAR_USE_POWER
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef SAMOVAR_USE_POWER
#ifndef SAMOVAR_USE_RMVK
void IRAM_ATTR triggerPowerStatus(void *parameter) {
  int i;
  String resp;
  while (true) {
    //T3EA3E80
    if (PowerOn) {
      Serial2.flush();
      vTaskDelay(100 / portTICK_PERIOD_MS);
      if (Serial2.available()) {
        resp = Serial2.readStringUntil('\r');
        i = resp.indexOf("T");
        if (i < 0) {
          resp = Serial2.readStringUntil('\r');
        }
        resp = resp.substring(i, resp.length());
#ifdef __SAMOVAR_DEBUG
        WriteConsoleLog("RESP_T =" + resp.substring(0, 1));
#endif
        if (resp.substring(0, 1) == "T") {
          resp = resp.substring(1, 9);
#ifdef __SAMOVAR_DEBUG
          WriteConsoleLog("RESP_F =" + resp.substring(1, 7));
          WriteConsoleLog("RESP_MODE =" + resp.substring(6, 7));
#endif
          int cpv = hexToDec(resp.substring(0, 3));
          //Если напряжение больше 300 или меньше 10 - не корректно получено значение от регулятора, оставляем старое значение
          if (cpv > 10 && cpv < 3000) {
            current_power_volt = cpv / 10.0F;
            target_power_volt = hexToDec(resp.substring(3, 6)) / 10.0F;
            current_power_mode = resp.substring(6, 7);
            vTaskDelay(300 / portTICK_PERIOD_MS);
          }
        }
      }
    } else {
      vTaskDelay(400 / portTICK_PERIOD_MS);
    }
  }
}
#else
void IRAM_ATTR triggerPowerStatus(void *parameter) {
  String resp;
  uint16_t v;
  while (true) {
    if (PowerOn) {
#ifdef SAMOVAR_USE_SEM_AVR
      if ( xSemaphoreTake( xSemaphoreAVR, ( TickType_t ) ((RMVK_DEFAULT_READ_TIMEOUT * 7) / portTICK_RATE_MS)) == pdTRUE)
      {
        vTaskDelay(100 / portTICK_RATE_MS);
        Serial2.flush();
        Serial2.print("АТ+SS?\r");
        vTaskDelay(400 / portTICK_RATE_MS);
        if (Serial2.available()) {
          current_power_mode = Serial2.readStringUntil('\r');
#ifdef __SAMOVAR_DEBUG
          WriteConsoleLog("CPM=" + current_power_mode);
#endif
        }
        xSemaphoreGive( xSemaphoreAVR );
      }
      vTaskDelay(RMVK_READ_DELAY / portTICK_PERIOD_MS);
      if ( xSemaphoreTake( xSemaphoreAVR, ( TickType_t ) ((RMVK_DEFAULT_READ_TIMEOUT * 7) / portTICK_RATE_MS)) == pdTRUE)
      {
        vTaskDelay(100 / portTICK_RATE_MS);
        Serial2.flush();
        Serial2.print("АТ+VO?\r");
        vTaskDelay(400 / portTICK_RATE_MS);
        if (Serial2.available()) {
          resp = Serial2.readStringUntil('\r');
#ifdef __SAMOVAR_DEBUG
          WriteConsoleLog("CPV=" + resp);
#endif
          current_power_volt = resp.toInt();
        }
        xSemaphoreGive( xSemaphoreAVR );
      }
      vTaskDelay(RMVK_READ_DELAY / portTICK_PERIOD_MS);
      if ( xSemaphoreTake( xSemaphoreAVR, ( TickType_t ) ((RMVK_DEFAULT_READ_TIMEOUT * 7) / portTICK_RATE_MS)) == pdTRUE)
      {
        vTaskDelay(100 / portTICK_RATE_MS);
        Serial2.flush();
        Serial2.print("АТ+VS?\r");
        vTaskDelay(400 / portTICK_RATE_MS);
        if (Serial2.available()) {
          resp = Serial2.readStringUntil('\r');
#ifdef __SAMOVAR_DEBUG
          WriteConsoleLog("TPV=" + resp);
#endif
          v = resp.toInt();
          if (v != 0) {
            target_power_volt = v;
          }
        }
        xSemaphoreGive( xSemaphoreAVR );
      }
#else
      current_power_volt = RMVK_get_out_voltge();
      vTaskDelay(RMVK_READ_DELAY / portTICK_PERIOD_MS);
      v = RMVK_get_store_out_voltge();
      vTaskDelay(RMVK_READ_DELAY / portTICK_PERIOD_MS);
      rmvk.on = RMVK_get_state() > 0;
      if (v != 0) {
        target_power_volt = v;
      }
#endif
    }
    vTaskDelay(RMVK_READ_DELAY / portTICK_PERIOD_MS);
  }
}
#endif

void check_power_error() {
#ifndef __SAMOVAR_DEBUG
  //Проверим, что заданное напряжение/мощность не сильно отличается от реального (наличие связи с регулятором, пробой семистора)
  if (SamSetup.CheckPower && current_power_mode == POWER_WORK_MODE && abs((current_power_volt - target_power_volt) / current_power_volt) > 0.2) {
    power_err_cnt++;
    if (power_err_cnt > 6) set_current_power(target_power_volt);
    if (power_err_cnt > 12) {
      delay(1000); //Пауза на всякий случай, чтобы прошли все другие команды
      set_buzzer(true);
      set_power(false);
      SendMsg(F("Аварийное отключение! Ошибка управления нагревателем."), ALARM_MSG);
    }
  } else
#endif
    power_err_cnt = 0;
}


//получаем текущие параметры работы регулятора напряжения
void IRAM_ATTR get_current_power() {
  if (!PowerOn) {
    current_power_volt = 0;
    target_power_volt = 0;
    current_power_mode = "N";
    current_power_p = 0;
    return;
  }
#ifndef SAMOVAR_USE_SEM_AVR
  //Считаем мощность V*V*K/R, K = 0,98~1
  current_power_p = current_power_volt * current_power_volt / SamSetup.HeaterResistant;
#else
  current_power_p = target_power_volt;
#endif
}

//устанавливаем напряжение для регулятора напряжения
void IRAM_ATTR set_current_power(float Volt) {
  if (!PowerOn) return;
#ifdef __SAMOVAR_DEBUG
  WriteConsoleLog("Set current power =" + (String)Volt);
#endif
  //vTaskDelay(100 / portTICK_PERIOD_MS); 5.13
  if (Volt < 40) {
    set_power_mode(POWER_SLEEP_MODE);
    return;
  } else {
    set_power_mode(POWER_WORK_MODE);
    //vTaskDelay(100/portTICK_PERIOD_MS);
  }
  target_power_volt = Volt;
  vTaskDelay(100 / portTICK_PERIOD_MS);
#ifdef SAMOVAR_USE_RMVK
#ifndef SAMOVAR_USE_SEM_AVR
  RMVK_set_out_voltge(Volt);
#else
  if ( xSemaphoreTake( xSemaphoreAVR, ( TickType_t ) ((RMVK_DEFAULT_READ_TIMEOUT * 7) / portTICK_RATE_MS)) == pdTRUE)
  {
    String Cmd;
    int V = Volt;
    if (V < 100) Cmd = "0";
    else
      Cmd = "";
    Cmd = Cmd + (String)V;
    vTaskDelay(200 / portTICK_PERIOD_MS);
    Serial2.flush();
    Serial2.print("АТ+VS=" + Cmd + "\r");
    vTaskDelay(300 / portTICK_PERIOD_MS);
    Serial2.flush();
    xSemaphoreGive( xSemaphoreAVR );
  }
#endif
#else
  String hexString = String((int)(Volt * 10), HEX);
  Serial2.print("S" + hexString + "\r");
#endif
  target_power_volt = Volt;
}

void IRAM_ATTR set_power_mode(String Mode) {
  if (current_power_mode == Mode) return;
  current_power_mode = Mode;
  vTaskDelay(50 / portTICK_PERIOD_MS);
#ifdef SAMOVAR_USE_RMVK
  if (Mode == POWER_SLEEP_MODE) {
#ifdef SAMOVAR_USE_SEM_AVR
    if ( xSemaphoreTake( xSemaphoreAVR, ( TickType_t ) ((RMVK_DEFAULT_READ_TIMEOUT * 7) / portTICK_RATE_MS)) == pdTRUE)
    {
      vTaskDelay(200 / portTICK_PERIOD_MS);
      Serial2.flush();
      Serial2.print("АТ+ON=0\r");
      vTaskDelay(300 / portTICK_PERIOD_MS);
      Serial2.flush();
      xSemaphoreGive( xSemaphoreAVR );
    }
#else
    RMVK_set_on(0);
#endif
  } else if (Mode == POWER_SPEED_MODE) {
#ifdef __SAMOVAR_DEBUG
    WriteConsoleLog("Set power mode=" + Mode);
#endif
#ifdef SAMOVAR_USE_SEM_AVR
    if ( xSemaphoreTake( xSemaphoreAVR, ( TickType_t ) ((RMVK_DEFAULT_READ_TIMEOUT * 7) / portTICK_RATE_MS)) == pdTRUE)
    {
      vTaskDelay(200 / portTICK_PERIOD_MS);
      Serial2.flush();
      Serial2.print("АТ+ON=1\r");
      vTaskDelay(300 / portTICK_PERIOD_MS);
      Serial2.flush();
      xSemaphoreGive( xSemaphoreAVR );
    }
#else
    RMVK_set_on(1);
    vTaskDelay(RMVK_READ_DELAY / portTICK_PERIOD_MS);
    RMVK_set_out_voltge(MAX_VOLTAGE);
    vTaskDelay(RMVK_READ_DELAY / portTICK_PERIOD_MS);
#endif
  }
#else
  Serial2.print("M" + Mode + "\r");
  vTaskDelay(300 / portTICK_PERIOD_MS);
#endif
}

unsigned int IRAM_ATTR hexToDec(String hexString) {
  unsigned int decValue = 0;
  int nextInt;

  for (byte i = 0; i < hexString.length(); i++) {

    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);

    decValue = (decValue * 16) + nextInt;
  }

  return decValue;
}
#endif
