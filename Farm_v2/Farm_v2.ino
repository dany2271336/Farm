// Библиотеки и дефайны
  #include <PinChangeInterrupt.h>
  #include <PinChangeInterruptBoards.h>
  #include <PinChangeInterruptPins.h>
  #include <PinChangeInterruptSettings.h>
  #include <I2C_RTC.h>
  #include <GyverIO.h>
  #include <EncButton.h>
  #include <LiquidCrystal_I2C.h>
  #include <Wire.h>
  #include <math.h>
  #include <EEPROM.h>

  #define R 5
  #define PUMP 6
  #define D A0


  EncButton eb(2, 3, 4);
  LiquidCrystal_I2C lcd(0x27, 16, 2);
  static DS1307 RTC;
//
//Переменные
  int MSdata[5];
  int PUMPLength=5;
  unsigned long Cooldown=20;
  int SoilCheckPeriod=2;
  bool LightState=0;
  bool LightStateMON=0;
  bool LightStateMOFF=0;
  bool TideState=0;
  bool TideFlag=0;
  bool LCD_active=1;
  byte TimeLOn=10;
  byte TimeLOff=22;
  byte Threshold=5;
  byte AMSdataMap=0;
  unsigned long M1=1500000;
  unsigned long M2=0;
  unsigned long TideStart=0;
  unsigned long TideCooldownStart=1000000;
  unsigned long LastDisturbed=0;
  unsigned long LCD_clock=0;
  int Menu_State=0;
  byte Menu_levels=7;
  bool Menu_mode=0;
  byte Light_cases=0;
  byte Water_cases=2;
  byte Lset_cases=2;
  byte Tset_cases=6;
  byte Day; byte Month; byte Year; byte Hour; byte Minutes; byte Seconds=0; byte Week=2;
  bool draw=1;
  byte i=0;
  bool ManualWater=0;
  bool Worktime=0;
  byte customChar[] = {
    B00010,
    B00110,
    B01100,
    B11110,
    B01111,
    B00110,
    B01100,
    B01000
  };
//

void setup() {
  attachInterrupt(0, isr, CHANGE);
  attachInterrupt(1, isr, CHANGE);
  attachPCINT(20, isr, FALLING);
  eb.setEncISR(true);
  EEPROMget();
  lcd.init();
  lcd.backlight();
  RTC.begin();
  Serial.begin(9600);
  pinMode(R, OUTPUT);
  pinMode(PUMP, OUTPUT);
  pinMode(D, INPUT);
  Startup();
  lcd.clear();
  lcd.createChar(0, customChar);
}

void loop() {
  eb.tick();
  DisplayResetTimer(); 
  Menu_Func();
  LightTurnCheck();
  if (Worktime==1) {
    SoilMoistureCheck();
    TideComes();}
  else digitalWrite(PUMP, LOW);
}


void Menu_LCD() {//Меню фронтенд
  switch (Menu_State) {
    case 0:// Главный экран с часами
      lcd.setCursor(1, 0);
      switch (RTC.getWeek()) {
        case 1:
          lcd.print("SUN");
          break;
        case 2:
          lcd.print("MON");
          break;
        case 3:
          lcd.print("TUE");
          break;
        case 4:
          lcd.print("WED");
          break;
        case 5:
          lcd.print("THU");
          break;
        case 6:
          lcd.print("FRI");
          break;
        case 7:
          lcd.print("SAT");
          break;
        }
      lcd.print(" ");
      if (RTC.getDay() < 10) lcd.print("0");
      lcd.print(RTC.getDay());
      lcd.print(".");
      if (RTC.getMonth() < 10) lcd.print("0");
      lcd.print(RTC.getMonth());
      lcd.print(".");
      lcd.print(RTC.getYear());

      lcd.setCursor(5, 1);
      if (RTC.getHours() < 10) lcd.print("0");
      lcd.print(RTC.getHours());
      lcd.print(":");
      if (RTC.getMinutes() < 10) lcd.print("0");
      lcd.print(RTC.getMinutes());
      if (Worktime==1) {
        lcd.setCursor(4, 1);
        lcd.write(0);
        lcd.setCursor(10, 1);
        lcd.write(0);
      }
      else {
        lcd.setCursor(4, 1);
        lcd.print(" ");
        lcd.setCursor(10, 1);
        lcd.print(" ");
      }
      break;
    case 1:// Экран показаний полива
      lcd.home();
      lcd.print("Moisture Sensor State:");
      lcd.setCursor(0, 1);
      lcd.print("M:");
      if (AMSdataMap!=10) {lcd.setCursor(2, 1); lcd.print("0");} 
      lcd.print(AMSdataMap);
      lcd.print("/10  ");
      lcd.print("C:");
      if ((TideState==1) && (TideFlag==1)) {lcd.setCursor(11, 1); lcd.print("Flow ");}
      else {
        int WaterTimeout=(((Cooldown*60*1000)-(millis()-TideCooldownStart))/1000);
        WaterTimeout=constrain(WaterTimeout, 0, 32000);
        if (WaterTimeout <= 60) {lcd.print(WaterTimeout); lcd.print("s    ");} 
        else if (WaterTimeout > 60) {
          byte WTOM=WaterTimeout/60;
          if (WTOM<10) lcd.print(0);
          lcd.print(WTOM);
          lcd.print(":");
          byte WTOS=WaterTimeout-(WTOM*60);
          if (WTOS<10) lcd.print(0);
          lcd.print(WTOS);
        }
      }
      break;
    case 4:// Экран режимов освещения
      lcd.setCursor(0, 0);
      lcd.print("Light Mode: ");
      switch (Light_cases) 
          {
        case 0:
          lcd.print("AUTO");
          lcd.setCursor(0, 1);
          lcd.print("Timer control   ");
          break;
        case 1:
          lcd.print("ON  ");
          lcd.setCursor(0, 1);
          lcd.print("Is always on    ");
          break;
        case 2:
          lcd.print("OFF    ");
          lcd.setCursor(0, 1);
          lcd.print("Is always off   ");
          break;
          }
      break;
    case 2:// Экран функции ручного полива
      lcd.home();
      lcd.print("Hold to water...");
      
      if (ManualWater==1) 
        {
        lcd.setCursor(0, 1);
        lcd.print("THE WATER COMES!");
        }
      if (ManualWater==0)
        {
        lcd.home();
        lcd.print("Hold to water...");
        lcd.setCursor(0, 1);
        lcd.print("                ");
        }
      break;
    case 3:// Экран настроек полива
      lcd.home();
      lcd.print("Water  Settings");
      lcd.setCursor(0, 1);
      lcd.print("TH:");
      if (Threshold<10) lcd.print("0");
      lcd.print(Threshold);
      lcd.setCursor(7, 1);
      lcd.print("SEC:");
      if (PUMPLength<10) lcd.print("0");
      if (PUMPLength<100) lcd.print("0");
      lcd.print(PUMPLength);
      switch (Water_cases) {
        case 2:
          lcd.setCursor(5, 1);
          lcd.print(" ");
          lcd.setCursor(14, 1);
          lcd.print(" ");
          break;
        case 1:
          lcd.setCursor(5, 1);
          lcd.print(char(127));
          break;
        case 0:
          lcd.setCursor(5, 1);
          lcd.print(" ");
          lcd.setCursor(14, 1);
          lcd.print(char(127));
          break;
      } 
      break;
    case 5:// Настройка времени работы освещения
      lcd.home();
      lcd.print("Light  Settings");
      lcd.setCursor(0, 1);
      if (TimeLOn<10) lcd.print("0"); 
      lcd.print(TimeLOn); lcd.print(":00");
      lcd.setCursor(7, 1);
      if (TimeLOff<10) lcd.print("0"); 
      lcd.print(TimeLOff); lcd.print(":00");

      switch (Lset_cases) {
        case 2:
          lcd.setCursor(5, 1);
          lcd.print("-");
          lcd.setCursor(6, 1);
          lcd.print("-");
        break;
        case 1:
          lcd.setCursor(5, 1);
          lcd.print(char(127));
          
        break;
        case 0:
          lcd.setCursor(5, 1);
          lcd.print("-");
          lcd.setCursor(6, 1);
          lcd.print(char(126));
        break;
      } 
      break;
    case 6:// Установка даты и времени
      switch (Tset_cases) {//клик переключает какой элемент даты и времени настраивается, поворот энкодера настраивает
        case 6:
          lcd.home();
          lcd.print("Time & Date set ");
          lcd.setCursor(0, 1);
          lcd.print("Click to set... ");
          break;
        case 5:
          if (draw==1) {
            lcd.clear();
            lcd.home();
            lcd.print(" Set date:      ");
            lcd.setCursor(0, 1);
            lcd.print("   .__.____");
            draw=0;
          }
          lcd.setCursor (1, 1);
          if (Day<10) lcd.print(0);
          lcd.print(Day);
          break;
        case 4:
          lcd.setCursor (4, 1);
          if (Month<10) lcd.print(0);
          lcd.print(Month);
          break;
        case 3:
          lcd.setCursor (7, 1);
          lcd.print("20");
          if (Year<10) lcd.print(0);
          lcd.print(Year);
          break;
        case 2:
          if (draw==1) {
            lcd.home();
            lcd.print(" Set time:      ");
            lcd.setCursor(0, 1);
            lcd.print("                ");
            lcd.setCursor(0, 1);
            lcd.print("   :__");
            draw=0;
          }
          lcd.setCursor (1, 1);
          if (Hour<10) lcd.print(0);
          lcd.print(Hour);
          break;
        case 1:
          lcd.setCursor(4, 1);
          if (Minutes<10) lcd.print(0);
          lcd.print(Minutes);
          break;
        case 0:
          if (draw==1) {
            lcd.setCursor(0, 1);
            lcd.print("                 ");
            lcd.setCursor(0, 0);
            lcd.print(" Set week:       ");
            draw=0;
          }

          lcd.setCursor(10, 0);
          switch (Week)
          {
            case 1:
              lcd.print("SUN");
              break;
            case 2:
              lcd.print("MON");
              break;
            case 3:
              lcd.print("TUE");
              break;
            case 4:
              lcd.print("WED");
              break;
            case 5:
              lcd.print("THU");
              break;
            case 6:
              lcd.print("FRI");
              break;
            case 7:
              lcd.print("SAT");
              break;
          }
      }
  }
}

void Menu_Func() {//Меню бэкенд. Наследует LCD_Shell
  LCD_Shell();
  switch (Menu_State) {
    case 0:// Главный экран с часами
      Menu_mode=0;
      break;
    case 1:// Экран показаний полива
      Menu_mode=0;
    case 4:// Экран режимов освещения
      Menu_mode=0; 
      if (eb.click()) 
        {
          Light_cases++;
          if (Light_cases > 2) {Light_cases=0;}
          Serial.println(Light_cases);
        }
      switch (Light_cases) 
          {
        case 0:
          LightStateMON=0;
          LightStateMOFF=0;
          break;
        case 1:
          LightStateMON=1;
          LightStateMOFF=0;
          break;
        case 2:
          LightStateMON=0;
          LightStateMOFF=1;
          break;
          }
        EEPROM.update(6, Light_cases);
      break;
    case 2:// Экран функции ручного полива
      Menu_mode=0;
      if (eb.hold()) {ManualWater=1; digitalWrite(PUMP, ManualWater); Serial.println("ManualWater");}
      if (eb.release()) {ManualWater=0; digitalWrite(PUMP, ManualWater);}
      break;
    case 3:// Экран настроек полива
      if (eb.hold()) Water_cases=2;
      if (eb.click()) {
        if (Water_cases==0) Water_cases=3; 
        Water_cases--;
        EEPROM.put(8, PUMPLength);
        EEPROM.update(10, Threshold);
      }
      switch (Water_cases) {
        case 2:
          Menu_mode=0;
          break;
        case 1:
          Menu_mode=1;
          if (eb.right()) {
            Threshold+=1;
            if (Threshold > 10) Threshold=0;
          }
          if (eb.left()) {
            if (Threshold==0) Threshold=11;
            Threshold-=1;
          }
          break;
        case 0:
          Menu_mode=1;
          if (eb.right()) {
            PUMPLength+=1;
            if (PUMPLength > 120) PUMPLength=0;
          }
          if (eb.left()) {
            if (PUMPLength==0) PUMPLength=121;
            PUMPLength-=1;
          }
          if (eb.rightH()) {
            PUMPLength+=10;
            if (PUMPLength > 120) PUMPLength=0;
          }
          if (eb.leftH()) {
            if (PUMPLength==0) PUMPLength=130;
            PUMPLength-=10;
          }
          break;
      } 
      break;
    case 5:// Настройка времени работы освещения
      if (eb.hold()) Lset_cases=2;
      if (eb.click()) {
        if (Lset_cases==0) Lset_cases=3; 
        Lset_cases--;
        EEPROM.update(11, TimeLOn);
        EEPROM.update(12, TimeLOff);
      }
      switch (Lset_cases) {
        case 2:
          if (TimeLOff==TimeLOn) {
            TimeLOff+=1;
            if (TimeLOff > 23) TimeLOff=0;
            }
          Menu_mode=0;
          break;
        case 1:
          Menu_mode=1;
          if (eb.right()) {
            TimeLOn+=1;
            if (TimeLOn > 23) TimeLOn=0;
          }
          if (eb.left()) {
            if (TimeLOn<1) TimeLOn=24;
            TimeLOn-=1;
          }
          break;
        case 0:
          Menu_mode=1;
          if (eb.right()) {
            TimeLOff+=1;
            if (TimeLOff > 23) TimeLOff=0;
          }
          if (eb.left()) {
            if (TimeLOff<1) TimeLOff=24;
            TimeLOff-=1;
          }
          break;
      } 
      break;
    case 6:// Установка даты и времени
      if (eb.click()) {
        if (Tset_cases==0) Tset_cases=7; 
        Tset_cases--;
      }
      if (eb.hold()) Tset_cases=6;

      switch (Tset_cases) {//клик переключает какой элемент даты и времени настраивается, поворот энкодера настраивает
        case 6:
          Menu_mode=0;
          draw=1;
          break;
        case 5:
          Menu_mode=1;
          if (eb.right()) {
            Day++;
            if (Day>31) Day=1;
          }
          if (eb.left()) {
            if (Day<=1) Day=32;
            Day--;
          }
          break;
        case 4:
          draw=1;
          Menu_mode=1;
          if (eb.right()) {
            Month++;
            if (Month>12) Month=1;
          }
          if (eb.left()) {
            if (Month<=1) Month=13;
            Month--;
          }
          break;
        case 3:
          Menu_mode=1;
          if (eb.right()) {
            Year++;
            if (Year>99) Year=1;
          }
          if (eb.left()) {
            if (Year<1) Year=100;
            Year--;
          }       
          break;
        case 2:
          Menu_mode=1;
          if (eb.right()) {
            Hour++;
            if (Hour>23) Hour=0;
          }
          if (eb.left()) {
            if (Hour==0) Hour=24;
            Hour--;
            }
          break;
        case 1:
          Menu_mode=1;
          draw=1;
          if (eb.right()) {
            Minutes++;
            if (Minutes>59) Minutes=0;
          }
          if (eb.left()) {
            if (Minutes==0) Minutes=60;
            Minutes--;
          }         
          break;
        case 0:
          Menu_mode=1;
          if (eb.right()) {
            Week++;
            if (Week>7) Week=1;
          }
          if (eb.left()) {
            if (Week<=1) Week=8;
            Week--;
          }
          RTC.setWeek(Week);
          RTC.setTime(Hour, Minutes, Seconds);
          RTC.setDate(Day, Month, Year);
          EEPROM.update(0, Hour);
          EEPROM.update(1, Minutes);
          EEPROM.update(2, Day);
          EEPROM.update(3, Month);
          EEPROM.update(4, Year);
          EEPROM.update(5, Week);
      }
  }
}

void isr() {// Аппаратное прерывание с пинов энкодера и обработка сигнала энкодера
  eb.tickISR();
}

void LCD_Shell() {// Обработчик меню. Регистрирует взаимодействие с меню и задает обновления
  if (LCD_active==1) {
    //Клик - Вход в УПРАВЛЕНИЕ на экране, если оно доступно в case конкретного экрана меню.
    if (eb.click()) {Menu_mode=!Menu_mode; Menu_LCD();} 
    //Поворот - Смена экрана меню, обновление экрана
    if (eb.left() && Menu_mode==0) {Menu_State--; lcd.clear(); Menu_LCD();}
    else if (eb.right() && Menu_mode==0) {Menu_State++; lcd.clear(); Menu_LCD();}
    //Закольцовка экранов.
    if (Menu_State>Menu_levels-1) {Menu_State=0;}
    else if (Menu_State<0) {Menu_State=Menu_levels-1;}
  }
  if (millis()-LCD_clock >= 100) {LCD_clock=millis(); Menu_LCD();}
}

void DisplayResetTimer() { // Засыпание экрана через 60 секунд
  //Пробуждение экрана с задержкой управления. Таким образом при повороте энкодера не меняется экран главного меню. Работает только после сна.
  if ((eb.turn() || eb.click() || eb.hold()) && (LCD_active==0)) {
    LastDisturbed=millis();
    lcd.backlight();
    delay(300);
    LCD_active=1;
  }//Если сна не было, но был сигнал с энкодера, таймер сна начинается заново. 
  else if (eb.turn() || eb.click() || eb.hold()) {
    LastDisturbed=millis();
  }//Если экран активен и не было сигнала с энкодера, через 60 сек меню сбрасывается до часов, экран гаснет.
  if ((millis()-LastDisturbed > 60000) && (LCD_active==1)) {
    lcd.noBacklight();
    Menu_mode=0;
    LCD_active=0;
    lcd.clear();
    Menu_State=0;
  }
}

void Startup() { // Анимация загрузки контроллера. Проигрывается при включении. Декорация.
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Farm is loading");
  lcd.setCursor(0, 1);
  for (byte i=0; i<16; i++) {
    lcd.setCursor(i, 1);
    lcd.print(char(255));
    delay(50);
  }
}

void LightTurnCheck() {// Проверка рабочего времени фермы. Каждые 10 сек.
  if ((millis()-M1 >= 10000)) {
    M1=millis();
    //Проверка рабочего времени по РТС.
    if ((TimeLOn < TimeLOff) && ((RTC.getHours() >= TimeLOn) && (RTC.getHours() < TimeLOff))) //Внутри одних суток
    {LightState=1; Serial.print("LS=1 by First");}
    else if ((TimeLOn > TimeLOff) && ((RTC.getHours() >= TimeLOn) || (RTC.getHours() < TimeLOff)))//С переходом на следующие сутки
    {LightState=1; Serial.print("LS=1 by Second");}
    else {LightState=0; Serial.print("LS=0 by Else");}
    if (Worktime==1) Serial.println(" - POWER ON");
    else Serial.println(" - POWER OFF");
  }
  //Проверка включения или отключения рабочего времени вручную через меню.
  if ((LightState==1 || LightStateMON==1) && LightStateMOFF==0) 
  {digitalWrite(R, 1); Worktime=1;} 
  else {digitalWrite(R, 0); Worktime=0;}
  
}

void SoilMoistureCheck() { // Проверка датчика влажности, запись значения х/10, сравнение с порогом полива
  // Опрос датчика по асинхронному таймеру.
  if ((millis()-M2) >= (SoilCheckPeriod*1000)) {
    M2=millis();
    // Сбор пяти показаний датчика, один раз в SoilCheckPeriod секунды, усреднение, маппинг.
    MSdata[i]=constrain(analogRead(D), 300, 500);
    i++;
    if (i>4) i=0;
    if ((MSdata[0]!=0) && (MSdata[1]!=0) && (MSdata[2]!=0) && (MSdata[3]!=0) && (MSdata[4]!=0)) { // проверка, что весь массив[5] заполнен показателями датчика
      int AMSdata=(MSdata[1]+MSdata[2]+MSdata[3]+MSdata[4]+MSdata[0])/(5); //усреднение
      Serial.print("Усредненный датчик - "); Serial.println(AMSdata);
      AMSdataMap=map(AMSdata, 300, 500, 10, 0);
      Serial.print("Датчик маппинг - ");
      Serial.println(AMSdataMap);
      /* Сравнение показателей с порогом начала полива. Если ниже порога, то переключается флаг поливалки. 
       При флаге поливалки =1, весь блок кода проверки датчиков прекращает работу */ 
      if (AMSdataMap<Threshold) TideState=1;
      else TideState=0;
      Serial.print("Необходимость полива - ");
      Serial.println(TideState);
      for (byte a=0; a<5; a++) {    //Очистка массива
        MSdata[a]=0;
      }
     }
    else Serial.println("Collecting data...");
  }
}

void TideComes() {// Функция полива
    //Если датчик ниже порога и настало время для следующего полива, то начать полив, записать время начала. 
    if ((TideState==1) && (TideFlag==0) && (millis()-TideCooldownStart > (Cooldown*60*1000))) 
      {
      Serial.println("Cooldown Ready!");
      TideStart=millis();
      TideFlag=1;
      digitalWrite(PUMP, TideState); 
      Serial.println("Полив включен");
      }
    //Затем по истечение длительности полива, прекратить полив, начать отсчет времени до следующего возможного полива.
    if ((TideState==1) && (TideFlag==1) && (((millis()-TideStart)/1000) >= PUMPLength)) 
      {
      TideState=0;
      TideFlag=0;
      TideCooldownStart=millis(); 
      digitalWrite(PUMP, TideState);
      Serial.println("Полив отключен");
      } 
}

void EEPROMget() {// Функция копирования настроек из EEPROM в SRAM для сохранения установок пользователя после перезагрузки
  EEPROM.get(0, Hour);
  EEPROM.get(1, Minutes);
  EEPROM.get(2, Day);
  EEPROM.get(3, Month);
  EEPROM.get(4, Year);
  EEPROM.get(5, Week);
  EEPROM.get(6, Light_cases);
  EEPROM.get(8, PUMPLength);
  EEPROM.get(10, Threshold);
  EEPROM.get(11, TimeLOn);
  EEPROM.get(12, TimeLOff);
}