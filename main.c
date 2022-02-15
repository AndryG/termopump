const u8 PROGMEM ledZ[] = {LED_Z_SET};

#define LED_COUNT (sizeof(ledZ))

/* видеопамять */
char led[LED_COUNT] = {ZG_SPACE, ZG_MINUS, ZG_SPACE};

/* флаг моргания индикатора */
bool ledBlink;

//u32 sec;
u8 secf; // доли секунды register -10

// char buf[12];

struct conf_s {
  u8 t;      // температурная уставка
  u8 dt;     // гистерезис температуры /-12.0 - 12.0/ в десятых градуса >0 режим охлаждения, <0 режим нагревателя
  u8 light;  // яркость индикатора
  u8 crc;    // для контроля данных в ееп
};

// параметры по умолчанию в случае беды с eep
#define CONF_DEFAULT 9, 5, 12

struct conf_s const PROGMEM conf_p = {CONF_DEFAULT};

// структура настроек в eep
struct conf_s EEMEM conf_e = {CONF_DEFAULT, 166};

struct conf_s conf;

u8 regErr = 0;
#define E_DS_CRC    0x04
#define E_CONF_CRC  0x10 // ошибка чтения настроек из eep

//register i16 dsValue asm("r4");  -28 байт
i16 dsValue;

// нажатые кнопки
u8 btn;//  asm("r3");

/* счетчик ошибок чтения ds.
  * При обнулении счетчика отключается реле, регистрируется ошибка "много ошибок чтения", регулировка не проводится
  * Изначально счетчик пустой - регулировка не будет проводится, но регистрации ошибок не будет (нет момента перехода в ноль).
  */
u8 dsErrCount = 0;

/* К-во попыток чтения датчика до отключения выхода */
#define DS_ERR_COUNT 6

void main(void) __attribute__((noreturn));
void a3();
void a3ResetDelay();
bool a5ShowErr();
u8 tLedAndKey();
void mcuInit();
void saveConf();
bool loadConf();
u8 w1CRCBuf(void* buf, u8 len, u8 crc);
void numToLed(u16 value);
void relayOn();
void relayOff();

bool a5ShowErr(){
  if(regErr){
    led[0] = 0x0E;
    led[1] = regErr >> 4;
    led[2] = regErr & 0x0f;
    if(btn & BTN_SET){
      regErr = 0;
    }
    return true;
  }
  return false;
}

bool a6Changed = false;

/* Интерфейс настройки параметров*/
void a6(){

  if(a5ShowErr()){
    return;
  }

  u8 d = 0;
  if(btn & BTN_PLUS && conf.dt < 30){
    d = 1;
  }
  if(btn & BTN_MINUS && conf.dt > 1){
    d -= 1;
  }
  if(d){
    conf.dt += d;
    ledBlink = true;
  }
  if(btn & BTN_SET && ledBlink){
    saveConf();
    loadConf();
    ledBlink = false;
  }
  numToLed(conf.dt);
  led[0] = ZG_d;
}

u16 a5Timer= T_SCRSVR;

bool a5Sht = true; // show t / show T

/* Вариант интерфейса "на ветках" */
void a5(){

  if(btn){
    u8 b = btn;
    if(0 == a5Timer){
      btn = 0;
    }
    if(b & BTN_SET){
      a5Timer = a5Sht ? T_SCRSVR : T_DEFSCR;
    }
    }else if(a5Timer){
    a5Timer--;
  }

  if(a5ShowErr()){
    return;
  }

  ledBlink = 0 == dsErrCount;

  if(dsErrCount && 0 == a5Timer){
    if(a5Sht){
      led[0] = ZG_SPACE; led[1] = ZG_MINUS; led[2] = ZG_SPACE; // screensaver
      return;
    }else{
      a5Sht = true; // истек таймер, переходим на показ уставки
      a5Timer = T_SCRSVR;
    }
  }

  if(btn & BTN_SET){
    a5Sht = !a5Sht;
  }

  if(a5Sht || 0 == dsErrCount){
    u8 d = 0;
    if(btn & BTN_PLUS && conf.t < 30){
      d = 1;
    }
    if(btn & BTN_MINUS && conf.t > 0){
      d -= 1;
    }
    if(d){
      conf.t += d;
      saveConf();
      a3ResetDelay();
    }
    numToLed(conf.t);
    led[0] = ZG_t;
  }else{ // show T
    numToLed(dsValue);
  }
}

u8 a3State = 0;
u16 a3Tick = 0;

/* Сбрасывает задержку и начинает новое чтение датчика */
void a3ResetDelay(){
  a3Tick = 0;
  a3State = 0;
}

union{
    u8 a[9];
    i16 t;
  } dsData;

u8 a3ByteCnt = 0; // к-во прочитанных байт

/*
 Автомат регулировки
 Релейное управление с гистерезисом. Указывается в десятых градуса, работает "вниз" от уставки.
 */
void a3(){

  if(a3Tick){
    a3Tick -= 1;
    return;
  }

  u8 newErr = 0; // код ошибки для регистрации

  switch(a3State){
    case 0: // жду, отдыхаю. Запускаю преобразование
    case 2: // буду читать температуру
      newErr = w1Reset();
      if(newErr){
        goto errLabel;
      }
      a3State += 1;
      return;
    case 1:
      w1rw(0xCC);   // SKIP ROM [CCh]
      w1rw(0x44);   // CONVERT  [44h]
      a3Tick = TICK_SEC(2); // задержка на преобразование
      a3State += 1;
      return;
    case 3:
      w1rw(0xCC);  // SKIP ROM [CCh]
      w1rw(0xBE);  // READ SCRATCHPAD [BEh]
      a3ByteCnt = 0;
      a3State += 1;
      return;
    case 4:
      if(a3ByteCnt < 9){ // если счетчик больше нуля, значит мы читаем данные с линии
        dsData.a[a3ByteCnt++] = w1rw(0xff);        
      }else if(0 == w1CRCBuf(&dsData, 9, 0)){ // вычитали все байты
        dsValue = dsData.t * 10 / 16;

        // управление реле
        i16 T = conf.t * 10;
        if(dsValue > T){
          relayOff();
        }
        if(dsValue <= T - conf.dt){
          relayOn();
        }

        dsErrCount = DS_ERR_COUNT; // перезапуск счетчика допущенных ошибок
        goto reDelayLabel;
      }else{
        newErr = E_DS_CRC; // CRC err code
        goto errLabel;
      }
      return;
  } // switch
  return;

errLabel: // обработка ошибок
  regErr |= newErr;
  if(!dsErrCount || !(--dsErrCount)){// досчитали счетчик ошибок - выключаем нагрузку и ждем, пока появятся верные данные
    relayOff();
  }

reDelayLabel:
  a3Tick = TICK_SEC(2);
  a3State = 0;
}

void main(void)
{
  mcuInit();
  if(!loadConf()){ // настройки не загрузились
    regErr |= E_CONF_CRC;
  }
  relayOff();
  u8 bootTick = 10;
  bool setupMode = 0;
  while (1){

    if(TIFR & (1<<TOV0)){ // tick

      TIFR = (1<<TOV0);
      TCNT0 = 0xff + 1 - F_CPU / 256 / F_TICK; // 0x64

      if(0 == --secf){
        secf = TICK_SEC(1);
      }

      btn = tbtnProcess(tLedAndKey());

      if(bootTick){
        bootTick -= 1;
        if(0 == bootTick){
          setupMode = btn & (BTN_SET << 4);
        }
      }else if(setupMode){
        a6(); //1720
      }else{
        a3();
        a5();  //1546
      }
    }// tick
  }
}

/* инициализация железа */
void mcuInit(){
  // ticks init
  //TIMSK = (1 << TOIE0);
  TCNT0 = 0xff;
  TCCR0B = (1 << CS02) | (0 << CS01) | (0 << CS00);
  // led init
  iopOutputLow(LED_Z_PORT, LED_Z_MASK);
  // релюшка
  iopOutputLow(RELAY_PORT, bv(RELAY_BIT));
  //  sei(); а прерываний нет!
  #ifdef DEBUG
  iopOutputHigh(PORTA, bv(PA0)|bv(PA1));
  #endif
}

/*
  Закидывает в led-буфер число с выравниванием вправо
  Возвращает длину числа
 */
void numToLed(u16 value){
  u8 digit = 0;
 // u8 len = 1;
  u8 flag = ZG_SPACE;
  while(value >= 100){
    value -= 100;
    digit++;
 //   len = 3;
  }
  if(digit){
    flag = 0;
  }else{
    digit = flag;
  }
  led[LED_COUNT - 3] = digit;

  digit = 0;
  while(value >= 10){
    value -= 10;
    digit++;
 //   len = 2;
  }
  if(digit){
    flag = 0;
  }else{
    digit = flag;
  }
  led[LED_COUNT - 2] = digit;

  led[LED_COUNT - 1] = value;
  //return len;
}

//register u8 ledIndex asm("r7");
/* динамическая индикация + сканирование кнопок */
u8 tLedAndKey(){
  static u8 ledIndex  = 0;  // register -10 байт

  // выкл все разряды
  iopHigh(LED_Z_PORT, LED_Z_MASK);

  // скан кнопок
  iopInputP(LED_SEG_PORT, LED_BT_PIN_MASK); // линии чтения на вход
  iopOutputLow(LED_SEG_PORT, LED_BT_COMMON_MASK); // заземляем кнопки
  _delay_us(5); // сам не знаю зачем. Чтобы электроны добежали, куда надо )))
  u8 b = 0x08 | (LED_BT_PIN_MASK & (iopPin(LED_SEG_PORT)));

  if(++ledIndex > LED_COUNT - 1){
    ledIndex = 0;
  }

  // зажигаем следующий разряд  SEG-low, Z-high
  if(!(ledBlink && (secf & 0x40))){
    iopOutputLow(LED_SEG_PORT, 0xff);
    iopSet(LED_SEG_PORT, ~pgm_read_byte(&S7[(u8)led[ledIndex]]));  //todo убрать приведение типа
    iopLow(LED_Z_PORT, pgm_read_byte(&ledZ[ledIndex]));
  }
  return b;
}

u8 w1CRCBuf(void* buf, u8 len, u8 crc){
  for(u8 i = len; i > 0; i--){
    u8 b = *((u8*)buf++);
    for (uint8_t p = 8; p; p--) {
      crc = ((crc ^ b) & 1) ? (crc >> 1) ^ 0b10001100 : (crc >> 1);
      b >>= 1;
    }
  }
  return crc;
}

/* загрузка conf с eep, проверка, загрузка правильного варианта с прошивки в случае ошибки
2  @return true - загрузка успешна, false - были загружены параметры по умолчанию
*/
bool loadConf(){
  // читаем ееп, проверяем crc, если не совпало, то перечитываем с прошивки
  eeprom_read_block(&conf, &conf_e, sizeof(struct conf_s));
/*  u8* p = (u8*)&conf;
  EEAR = (uintptr_t)(&conf_e);
  for(u8 i = sizeof(struct conf_s); i > 0; i--){
    while (EECR & (1<<EEPE));
    EECR |= (1<<EERE);
    *p++ = EEDR;
    EEAR += 1;
  }*/

  if(w1CRCBuf(&conf, sizeof(struct conf_s), 7)){ // ошибка - читаем с прошивки conf по умолчанию. В eep не пишем, чтобы каждый раз напоминать, что conf битый
    //memcpy_P(&conf, &conf_p, sizeof(struct conf_s)); // 1392
    u8* r = (u8*)&conf;
    u8* p = (u8*)&conf_p;
    for(u8 i = sizeof(struct conf_s); i > 0; i--){
      *(r++) = pgm_read_byte(p++);
    }
    return false;
  }
  return true;
}

void saveConf(){
  conf.crc = w1CRCBuf(& conf, sizeof(struct conf_s) - 1, 7);
  eeprom_write_block(&conf, &conf_e, sizeof(struct conf_s));
}

inline void relayOn(){
  iopHigh(RELAY_PORT, bv(RELAY_BIT));
}
inline void relayOff(){
  iopLow(RELAY_PORT, bv(RELAY_BIT));
}

/*
void reset(){
  wdt_enable(WDTO_15MS);
  while(1);
}

// Функция для отключения wdt после перезагрузки (wdt остается активированным)
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

void wdt_init(void){
    MCUSR = 0; // В примере даташита очищатся бит WDRF. хз зачем
    wdt_disable();
    return;
} */

// Индикация dsData
 /*
        if(n < 10){
          led[0] = pgm_read_byte(&S7[dsData[n] >> 4]);
          led[1] = pgm_read_byte(&S7[dsData[n] & 0x0F]);
          led[2] = pgm_read_byte(&S7[n]);
          n++;
        }else{
          n = 0;
          led[0] = S7_MINUS;
          led[2] = S7_MINUS;
          led[1] = S7_POINT;
//          blink = TICK_MS(3000);
          tickDsRead = TICK_SEC(20);
        }
*/
 //      s7Str2fixPoint(itoa16(n++, buf), led, sizeof(ledZ), 0);

/*

bool a0Boot(){
  static u8 a0State = 1;
  static u8 a0Tick = sizeof(ledZ) + 2; // показать весь индикатор, кнопки + запас
  // состояние при обнулении tick, если 0, то state + 1
  //  static u8 timerState = 0;

  switch(a0State){
    default: // 1 3 5
    if(a0Tick && !(--a0Tick)){
      a0State++;
      //      a0State = timerState ? timerState : a0State + 1;
    }
    break;
    //case 0:  // начало начал. Заставка, ждем загрузку кнопок - будем проверяем смену Workflow
    //   if(1){
    //      char *p = led; *p++ = S7_C; *p++ = S7_r; *p = S7_C;
    //    }
    //a0Tick = sizeof(ledZ) + 2;
    //a0State++;
    //break;
    case 2: // проверка
    loadConf();
    led[0] = S7_F;
    {
      u8 wf;
      switch(btn & 0xf0){ // удерживаемые при перезагрузке кнопки
        case 0: // переходов нет - двигаемся дальше
        a0State = 3;
        a0Tick = TICK_SEC(1);
        led[1] = pgm_read_byte(&S7[conf.wf.wf]);
        return false;  // RETURN для выхода с обоих switch
        default:            wf = WF_NONE;   break;
        case BTN_MINUS <<4: wf = WF_STABLE; break;
        case BTN_PLUS  <<4: wf = WF_TERMO;  break;
      }
      conf.wf.wf = wf; // сохраняем workflow в conf, обновляем заставку и идем ждать отпускания кнопок
      saveConf();
      a0State = 5;
      a0Tick = TICK_SEC(1);
      led[1] = pgm_read_byte(&S7[conf.wf.wf]);
      return false;
    }
    //      s7Str2fixPoint(itoa16(conf.crc, buf), led, 3, 0);
    //      a0Tick = TICK_SEC(0.75);
    case 4: return true;
    case 6:
    if(0 == (btn & 0xf0)){ // кнопки уже отпущены - уходим на резет
      reset();
      break;
    }
  }
  return false;
}


//    u8 tickDsRead = 1;
//      u8 dsReadState = 1; //

//void aDsRead(){    //1030
//
//if(tickDsRead && !(--tickDsRead)){ // опрос термодатчика. Для запуска нужно установить время до опроса 0 - выключено
//tickDsRead = 25; // 1 сек
//PORTD ^= bv(PD6);
//if((dsReadState = ds18b20Reader(dsReadState))){
//s7Str2fixPoint(itoa16(dsReadValue, buf), led, sizeof(ledZ), 1);
//}else{ // первая часть выполнена успешно
//tickDsRead = TICK_SEC(1);  // самопрограммируемся на вторую
//}
//}
//}



      if(dsTick && !(--dsTick)){ // опрос термодатчика. Для запуска нужно установить время до опроса 0 - выключено
        if((dsState = ds18b20Reader(dsState))){
          dsErr = dsState;
          s7Str2fixPoint(itoa16(dsValue, buf), led, sizeof(ledZ), 1);
          dsTick = TICK_SEC(2);
        }else{ // первая часть выполнена успешно
          dsTick = TICK_SEC(1);  // самопрограммируемся на второй вызов после преобразования
        }
      }

      //todo Доделать считывание crc и проверку данных
      //  val = val < 0 ? 0 : (buf.u >> 3) + (buf.u >> 1); // 908 БЕЗ ОТР ТЕМПЕРАТУР в десятых
      dsValue = dsValue * 10 / 16; //  932 вариант с отрицательной темп. в десятых градуса
      //  val = val / 16; //  892  с отрицательной темп. в целых градуса  САМЫЙ ЭКОНОМ ПО РАЗМЕРУ
      //  val = val < 0 ? 0 : buf.i >> 4; //  898  В целых градуса без отрицательных



*/