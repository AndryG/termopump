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
  u8 regErr; // зарегестрированные ошибки
  u8 light;  // яркость индикатора
  u8 crc;    // для контроля данных в ееп
};

#define CONF_DEFAULT 9, 5, 0, 12

#define E_CONF_CRC  0x10 // ошибка чтения настроек из eep
// параметры по умолчанию в случае беды с eep

struct conf_s const PROGMEM conf_p = {CONF_DEFAULT};

// структура настроек в eep
struct conf_s EEMEM conf_e = {CONF_DEFAULT, 166};

struct conf_s conf;

//register i16 dsValue asm("r4");  -28 байт
i16 dsValue;

u8 dsErr = 0; // 0 - ок, 9 нет измерений
#define E_DS_CRC 0x04

// нажатые кнопки
u8 btn;//  asm("r3");

// "редактор на вилке"
u8* edValue = NULL;
u8 edMin, edMax;

/* К-во попыток чтения датчика до отключения выхода */
#define DS_ERR_COUNT 6

/* счетчик ошибок чтения ds.
  * При обнулении счетчика отключается реле, регистрируется ошибка "много ошибок чтения", регулировка не проводится
  * Изначально счетчик пустой - регулировка не будет проводится, но регистрации ошибок не будет (нет момента перехода в ноль).
  */
static u8 dsErrCount = 0;

void main(void) __attribute__((noreturn));
u8 tLedAndKey();
void mcuInit();
void tDSRead();
void reset();
void saveConf();
bool loadConf();
void numToLed(u16 value);
void relayOn();
void relayOff();

/* Сохранить настройки, выйти с редактора*/
u8 a4Save(u8 state){
  saveConf();
  edValue = NULL;
  return state & 0xf0;
}

/* Варинат интерфейса "X * Y" Горизонтальные экраны и настройками в каждом
  Номер вертикали в старшей тетраде. Номер состояния в вертикали - младшая тетрада
 */
void a4(){
  /* младшая тетрада ноль - признак   
  */
  static u8 state = 0x1f;

  u8 newState = state;

  ledBlink = true;

  if(0 == (state & 0x0f)){
    ledBlink = false;
    if(btn & BTN_PLUS){ // пролистывание по горизонтали T / t / Err / clr
      newState += 0x10;
    }else if(btn & BTN_MINUS){ // пролистывание в обратную сторону
      newState -= 0x10;
    }else if((btn & BTN_SET)){ // переход к настройкам, кроме нулевого
      newState += 1;
    }
    newState &= 0x3f;
  }else if(edValue){ // "редактор на вилках" состояние с маской 0b00xx00xx
    i8 d = 0;
    if(btn & BTN_PLUS && *edValue < edMax){
      d = 1;
    }
    if(btn & BTN_MINUS && *edValue > edMin){
      d -= 1;
    }

    numToLed(*edValue += d);

    if(btn & BTN_SET){ // сохранить результат
      newState = a4Save(state);
    }

  }
  switch(state){
    case 0x00: // screen
      led[0] = ZG_SPACE;
      led[1] = ZG_MINUS;
      led[2] = ZG_SPACE;
      edValue = &conf.light;
      edMin = 0;
      edMax = 12;
      break;
    case 0x01: // edit L
      led[0] = ZG_L;
      break;
    case 0x10:  // show T (текущая)
      numToLed(dsValue);
      edValue = &conf.dt;
      edMin = 0;
      edMax = 50;
      break;
    case 0x11: // edit dt
      led[0] = ZG_d;
      break;
    case 0x21: // edit t
    case 0x20: // show t (уставка)
      numToLed(conf.t);
      led[0] = ZG_t;
      edValue = &conf.t;
      edMin = 0;
      edMax = 27;
      break;
    case 0x31: // clear Err
      if(btn & BTN_MINUS){
        conf.regErr = 0;
        a4Save(state);
        newState = 0x2f;
      }
    case 0x30: // show Err
      edValue = NULL;
      led[0] = 0x0E;
      led[1] = conf.regErr >> 4;
      led[2] = conf.regErr & 0x0f;
      break;
    case 0x1f: // hard init
      led[1] = ZG_L; // load
      if(!loadConf()){ // настройки не загрузились
        conf.regErr |= E_CONF_CRC;
      }
      relayOff();
    case 0x2f: // soft init 
      newState =conf.regErr ? 0x30 : 0x10;
      break;

  } // switch

  state = newState;
}

/*
 Рабочий автомат режима TERMO
 Релейное управление с гистерезисом.
 Знак гистерезиса назначает режим работы (нагрев/охлаждение), указывает направления изм. температуры градусника при отк. печке/холодильнике.
 Отрицательный гистерезис для нагревателя, положительный - охладителя.
 Указывается в десятых градуса. Расчеты все ведутся в десятых градуса
 */
void a3TermoCore(){
  if(dsErrCount){ // ошибок еще не очень много
    
  }
  i16 T = conf.t * 10;
  if(dsValue > T){
    iopLow(RELAY_PORT, bv(RELAY_BIT));
  }
  if(dsValue <= T - conf.dt){
    iopHigh(RELAY_PORT, bv(RELAY_BIT));
  }
}


void main(void)
{
  mcuInit();
  while (1)
  {
    if(TIFR & (1<<TOV0)){ // tick
//      PINA = 0x02;

      TIFR = (1<<TOV0);
      TCNT0 = 0xff + 1 - F_CPU / 256 / F_TICK; // 0x64

      if(0 == --secf){
        secf = TICK_SEC(1);
      }

      tDSRead();
      btn = tbtnProcess(tLedAndKey());
      a4();
      //if(btn & 0x01){ a[0]++;   }
      //if(btn & 0x02){ a[1]++;   }
      //if(btn & 0x04){ a[2]++;   }
      //
      //led[0] = a[0] & 0x0f;
      //led[1] = a[1] & 0x0f;
      //led[2] = a[2] & 0x0f;
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
  iopOutputHigh(PORTA, bv(PA0));
  iopOutputHigh(PORTA, bv(PA1));
  #endif
}

/* #define BCD_Calc(digit, value, flag, buf, i, number)    do{digit = 0;\
  while(value >= number){digit++; value -= number;}   \
  if (digit) {digit += BCD_SYMBOL; flag = BCD_SYMBOL;}\
  else {digit = flag;}                                \
  BCD_SaveDataInBuf(digit, buf, i);                   \
BCD_SendData(digit); }while(0) */

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

/*
  чтение температуры
  return ошибка чтения: 0 - ок; 1 - no present; 2 - замыкание
*/
void tDSRead(){
  static u8 state = 0;
  static u16 tick = 1;

  union{
    i16 i;
    u8 u[2];
  } ds;

  if(tick && !(--tick)){

    u8 err = w1Reset();
    if(err){
      dsErr |= err;
      tick = TICK_SEC(3);
      return;
    }
    if(0 == state){ // запрос на преобразование

      w1rw(0xCC);   //SKIP ROM [CCh]
      w1rw(0x44);   //CONVERT  [44h]
      state++;
      tick = TICK_SEC(2);
    }else{         // 1 чтение и проверка данных
A0High;
      w1rw(0xCC);  //SKIP ROM [CCh]
      w1rw(0xBE);  //READ SCRATCHPAD [BEh]

      u8 crc = 0;
      for(u8 i = 0; i < 9; i++){
        PINA = 0x01;
        u8 b = w1rw(0xff);
        if(i < 2){
           ds.u[i] = b;
        }
        if(4 == i && !b){
          b = 57; // что выбить ошибку датчика
        }
        crc = w1CRCUpdate(crc, b);
      }
      if(crc){
        dsErr = E_DS_CRC;
      }else{
        dsValue = ds.i;
        dsValue = dsValue * 10 / 16;
      }
      state = 0;
      tick = TICK_MS(500);
    }
A0Low;
  }
}

u8 crcConf(u8 length){
   u8* b = (void*)(&conf);
  u8 crc = 7;
  for(u8 i = length; i > 0; i--){
    crc = w1CRCUpdate(crc, *(b++));
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

  if(crcConf(sizeof(struct conf_s))){ // ошибка - читаем с прошивки conf по умолчанию. В eep не пишем, чтобы каждый раз напоминать, что conf битый
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
  conf.crc = crcConf(sizeof(struct conf_s) - 1);
  eeprom_write_block(&conf, &conf_e, sizeof(struct conf_s));
}

inline void relayOn(){
  iopHigh(RELAY_PORT, bv(RELAY_BIT));
}
inline void relayOff(){
  iopLow(RELAY_PORT, bv(RELAY_BIT));
}

void reset(){
  wdt_enable(WDTO_15MS);
  while(1);
}

// Функция для отключения wdt после перезагрузки (wdt остается активированным)
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

void wdt_init(void){
    MCUSR = 0; /* В примере даташита очищатся бит WDRF. хз зачем*/
    wdt_disable();
    return;
}

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