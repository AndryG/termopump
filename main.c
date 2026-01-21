const u8 PROGMEM ledZ[] = {LED_Z_SET};

#define LED_COUNT (sizeof(ledZ))

/* видеопамять */
char led[LED_COUNT] = {ZG_SPACE, ZG_SPACE, ZG_SPACE};

/* флаг моргания индикатора */
bool ledBlink;

//u32 sec;
u8 secf; // доли секунды register -10

// char buf[12];

typedef union {
    struct {
        u8 t;      // температурная уставка
        u8 dt;     // гистерезис температуры в десятых градуса
        u8 crc;    // для контроля данных в ееп
    };
    u8 bytes[3];   // "байтове представлення" структури
} conf_t;

// структура настроек в eep — вся заповнена 0x45
conf_t EEMEM conf_e = {
    .bytes = { [0 ... sizeof(((conf_t*)0)->bytes) - 1] = 0x45 }
};

conf_t conf;

u8 regErr;
#define E_DS_NO     0x01 // датчика нема або обрив лінії
#define E_DS_SC     0x02 // лінія замкнута
#define E_DS_CRC    0x04 // помилка контрольної суми
#define E_DS_MANY   0x08 // помилка контрольної суми
#define E_CONF_DT   0x10 // ошибка не установлен гистерезис температуры
#define E_CONF_CRC  0x20 // ошибка не устанвлен конфиг

#define  E_FATAL (E_DS_MANY | E_CONF_CRC | E_CONF_DT)

//register i16 dsValue asm("r4");  -28 байт
i16 dsValue;

// нажатые кнопки
u8 btn;//  asm("r3");

/* счетчик ошибок чтения ds.
  * При обнулении счетчика отключается реле, регистрируется ошибка "много ошибок чтения", регулировка не проводится
  * Изначально счетчик пустой - регулировка не будет проводится, но регистрации ошибок не будет (нет момента перехода в ноль).
  */
u8 dsErrCount;

/* К-во попыток чтения датчика (подряд) до отключения выхода */
#define DS_ERR_COUNT 6

void main(void) __attribute__((noreturn));
u8 a3();
void a3ResetDelay();
bool a5ShowErr();
u8 tLedAndKey();
void mcuInit();
void saveConf();
bool loadConf();
void numToLed(u16 value);
void relayOn();
void relayOff();

#ifdef USE_TX_LOG

void put(u8 b){
  while(0 == (UCSRA & bv(UDRE)));
  UDR = b;
}

void putLed(){
  for(u8 i = 0; i < 3; i++){
    if(led[i] < 10){
      put(led[i] + '0');
    }
  }
  put(' ');
}

void transmitLog(){
  numToLed(dsValue);  putLed();
  numToLed(conf.t);   putLed();
  numToLed(conf.dt);  putLed();
  numToLed(regErr);   putLed();
  numToLed(dsErrCount);putLed();
  put(iopBit(RELAY_PORT, RELAY_BIT) ? '1' : '0');
  put('\r');put('\n');
}
#endif


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

//bool a6Changed = false;

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

u16 a7Timer = T_SCRSVR;

enum {sleep = 0, showT=1, setT=2, showErr=3}  a7State = showT;

const u8 a7BtnSet[] PROGMEM = {showT, setT, showT, showT};
        
void a7(u8 err){
             
    u8 b = btn; 

    ledBlink = regErr & E_FATAL;
    
    if(err & 0x7F){// && sleep == a7State){ // Маска ошибок
        a7State = showErr;  
        a7Timer = T_DEFSCR;
    }
        
    if(a7Timer && !(--a7Timer)){
        if(setT == a7State){
            a7State = showT;
            a7Timer = T_DEFSCR;
        }else{
            a7State = sleep;
        }
    }
    
    if(b & 0x0f){
        //b = btn;
        a7Timer = T_DEFSCR;        
        if(b & BTN_SET){
            u8 prevErr;
            switch(a7State){
                case sleep:
                    a7State = regErr ? showErr :showT;
                    break;
                case showErr:
                    prevErr = regErr;
                    regErr &= ~0x07;
                    if((regErr != prevErr) && regErr){
                       a7State = showErr;
                       break;
                    }
                    a7State = (regErr & E_DS_MANY) ? setT : showT;
                    break;
                case showT:
                    a7State = setT;
                    break;
                case setT:
                    a7State = regErr ? showErr : showT;
                    break;
            }                   
/*            if(showErr == a7State){
                u8 prevErr = regErr;
                regErr &= ~0x07;
                a7State = (regErr == prevErr) | !regErr ? showT : showErr;
            }else if(sleep == a7State){
                a7State = regErr ? showErr :showT;
            }else if(showT == a7State){
                a7State = setT;
            }else{
              a7State = showT;  
            }*/
        }else if(setT == a7State){
            u8 d = 0;
            if(b & BTN_PLUS && conf.t < 30){
                d = 1;
            }
            if(b & BTN_MINUS && conf.t > 0){
                d -= 1;
            }
            if(d){
                conf.t += d;
                saveConf();
                a3ResetDelay();
            }
        }
        btn = 0;
    }
                   
    switch(a7State){
        case sleep: // sleep
            led[0] = led[1] = led[2] = ZG_SPACE; // screensaver
            break;
        case setT:
            numToLed(conf.t);
            led[0] = ZG_t;
            break;
        case showT:
            numToLed(dsValue);
            break;
       case showErr:
                led[0] = 0x0E;
                led[1] = regErr >> 4;
                led[2] = regErr & 0x0f;
            break;
    }
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

  ledBlink = 0 == dsErrCount;

  if(a5ShowErr()){
    return;
  }

  if(dsErrCount && 0 == a5Timer){
    if(a5Sht){
      led[0] = led[1] = led[2] = ZG_SPACE; // screensaver
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
 Автомат чтения сенсора
   @return u8
     0 - цикл чтения не закончен
     0х80  - все ОК
     иначе - код ошибки (младший нибл)
 */
u8 a3(){

  if(a3Tick){
    a3Tick -= 1;
    return 0;
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
      return 0;
    case 1:
      w1rw(0xCC);   // SKIP ROM [CCh]
      w1rw(0x44);   // CONVERT  [44h]
      a3Tick = TICK_SEC(3); // задержка на преобразование
      a3State += 1;
      return 0;
    case 3:
      w1rw(0xCC);  // SKIP ROM [CCh]
      w1rw(0xBE);  // READ SCRATCHPAD [BEh]
      a3ByteCnt = 0;
      a3State += 1;
      return 0;
    case 4:   // попробовать переделать на 4+9 состояний (для каждого байта)
      if(a3ByteCnt < 9){ // если счетчик больше нуля, значит мы читаем данные с линии
        dsData.a[a3ByteCnt++] = w1rw(0xff);
        return 0;
      }else if(0 == w1CRCBuf(&dsData, 9, 0)){ // вычитали все байты
        dsValue = dsData.t * 10 / 16;
        newErr = 0x80; // NO ERROR
      }else{
        newErr = E_DS_CRC; // CRC err code
      }
      goto errLabel;
  } // switch

errLabel: // ошибка - попытка провалена, ждем следующую
  a3Tick = T_ADJUSTMENT;
  a3State = 0;
  return newErr;
}

/*
  Регулировка температуры.  Вызывается после считывания сенсора
  Если набралось много ошибок сенсора или есть ошибки настроек - управление выключается
  Ведет подсчет неудачных попыток считывания сенсора
*/
bool a4(u8 sensorErr)
{
    /*
      Накопитель ошибок сенсора.
      Если ошибок много подряд, то поднимается флаг E_DS_MANY
      Если ошибок нет, то флаг сбрасывается
    */
    
    if(0x80 == sensorErr){
        regErr &= ~E_DS_MANY;
        dsErrCount = DS_ERR_COUNT;
    }else {
        regErr |= sensorErr;
        if(sensorErr & 0x0F && dsErrCount && !(--dsErrCount)){ // якщо помилка і лічильник не пустий і він став пустим
            regErr |= E_DS_MANY;
        }            
    }
    
    if(0 == conf.dt){
        regErr |= E_CONF_DT;
    }
    
    #ifdef USE_TX_LOG
    //u8 releOld = RELAY_PORT & bv(RELAY_BIT);
    #endif
        
    if(regErr & (E_DS_MANY | E_CONF_CRC | E_CONF_DT)){ // критична помилка
        relayOff();
    } else { // помилок нема - керуємо реле
        i16 T = conf.t * 10;
        #if EXECUTER_MODE == EXECUTER_MODE_HEATER
            if(dsValue >= T + conf.dt){
                relayOff();
            }
            if(dsValue < T){
                relayOn();
            }
        #elif EXECUTER_MODE == EXECUTER_MODE_COOLER
            if(dsValue > T + conf.dt){
                relayOn();
            }
            if(dsValue <= T){
                relayOff();
            }
        #else
            #error EXECUTER_MODE need define EXECUTER_MODE_HEATER or EXECUTER_MODE_COOLER
        #endif
        
        #ifdef USE_TX_LOG
        //    return  (RELAY_PORT & bv(RELAY_BIT)) != releOld;
        #endif        
    }
    return true;
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

      btn = tbtnProcess(tLedAndKey());
      
      u8 res = 0;

      if(0 == --secf){
        secf = TICK_SEC(1); // blink
      }

      if(bootTick){
        bootTick -= 1;
        if(0 == bootTick){
          setupMode = btn & (BTN_SET << 4);
        }
      }else if(setupMode){
        a6();
      }else if(setT != a7State){
          res = a3();
          if(res){
              if(a4(res)){
                #ifdef USE_TX_LOG
                transmitLog();
                #endif
              }
          }          
      }  
//       numToLed(dsValue);        
      a7(res);
    }// tick
  }
}

/* инициализация железа */
void mcuInit(){
  // ticks init
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

  #ifdef USE_TX_LOG
  UBRRH = UBRRH_VALUE;
  UBRRL = UBRRL_VALUE;
  UCSRB = (1<<TXEN);
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

/* загрузка conf с eep, проверка, останов в случае ошибки
   @return true - загрузка успешна, false - параметры не загружены
*/
bool loadConf(){
  // читаем ееп, проверяем crc
  eeprom_read_block(&conf, &conf_e, sizeof(conf_t));
  /* u8* p = (u8*)&conf;
  EEAR = (uintptr_t)(&conf_e);
  for(u8 i = sizeof(conf_t); i > 0; i--){
    while (EECR & (1<<EEPE));
    EECR |= (1<<EERE);
    *p++ = EEDR;
    EEAR += 1;
  }*/

  if(w1CRCBuf(&conf, sizeof(conf_t), 7)){
    return false;
  }
  return true;
}

void saveConf(){
  conf.crc = w1CRCBuf(& conf, sizeof(conf_t) - 1, 7);
  eeprom_write_block(&conf, &conf_e, sizeof(conf_t));
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

//c:\service\avrdude\avrdude.exe -p t2313 -c usbasp -P usb -V -D -U flash:w:"D:\mcu\termopump\Debug\termopump.hex":i -qq