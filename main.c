const u8 PROGMEM ledZ[] = {LED_Z_SET};

/* ����������� */
char led[sizeof(ledZ)] = {S7_SPACE, S7_MINUS, S7_SPACE};

char buf[12];

struct conf_s {
  struct confWf{
    u8 wf; // �������� ������ ��� ��������
  } wf;
  struct confTr{
    i16 t; // ������������� �������
    i8 dt; // ���������� ����������� /-12.0 - 12.0/ � ������� ������� >0 ����� ����������, <0 ����� �����������
  } tr;
  struct confSt{
    u8 power;  // 0-1 ��������� ���� ��� ���������
  } st;
  u8 crc; // ��� �������� ������ � ���
};

#define CONF_DEFAULT {WF_TERMO}, {9, 10}, {}

// ������� ��������
u8 workFlow;
#define WF_NONE    0
#define WF_STABLE  1
#define WF_TERMO   2
#define WF_ERR     3
#define WF_NO_SET  9

// ��������� �� ��������� � ������ ���� � eep
struct conf_s const PROGMEM conf_p = {CONF_DEFAULT};

// ��������� �������� � eep
struct conf_s EEMEM conf_e = {CONF_DEFAULT, 166};

struct conf_s conf;

i16 dsValue;

u8 dsErr = 9; // 0 - ��, 9 ��� ���������

// ������� ������
u8 btn;

//u8 dsData[10];

//u8 dsCRC;

void main(void) __attribute__((noreturn));
u8 tLedAndKey();
void mcuInit();
void tDSRead();
//uint8_t ds18b20Reader(u8 state);
void reset();
void saveConf();
bool loadConf();

/*
 ������� ������� ������ TERMO
 �������� ���������� � ������������.
 ���� ����������� ��������� ����� ������ (������/����������), ��������� ����������� ���. ����������� ���������� ��� ���. �����/������������.
 ������������� ���������� ��� �����������, ������������� - ����������.
 ����������� � ������� �������. ������� ��� ������� � ������� �������
 */
void a3TermoCore(){
  i16 T = conf.tr.t * 10;
  i8 dt =  conf.tr.dt;
  if(0 == dt){
    // error
  }else{
    if((dt < 0) == (dsValue > T)){
      iopLow(RELAY_PORT, bv(RELAY_BIT));
    }
    if((dt < 0) == (dsValue <= T + dt)){
      iopHigh(RELAY_PORT, bv(RELAY_BIT));
    }
  }
}

void a2(){
  static u8 state = 0;
  static u16 restoreCounter = 0; // ������� �������������� - ������� � ������� ���������

  u8 newState = state;
  if(restoreCounter && !(--restoreCounter)){ // ��������� ���� �� ���� � ���������� �� ������� ���������
    newState = 0;
  }else{
    switch(state){
      case 0: // ����� t (def mode)
        //TODO �������� �������� ������� ������ ��� �����������
        s7Str2fixPoint(itoa16(dsValue, buf), led, 3, 1);
        if(btn & BTN_SET){
          newState++;
        }
        a3TermoCore();
        break;
      case 1: // ����� T (�������)
        if(btn & BTN_SET){
          newState++;
        }
        break;
      case 2: // ��������� T (�������)
        if(btn & BTN_SET){    // 1248
          restoreCounter = 1;
        }
        if(btn & (BTN_MINUS | BTN_PLUS)){
          i16 t = conf.tr.t;
          t += (btn & BTN_PLUS) ? 1 : -1;
          #define T_MAX 27
          #define T_MIN 0
          if(t > T_MAX){ t = T_MAX;}
          if(t < T_MIN){ t = T_MIN;}
          conf.tr.t = t;
          s7Str2fixPoint(itoa16(t, buf), &led[1], 2, 0);
          restoreCounter = TICK_SEC(5);
        }
        break;
    }
  }

  if(state != newState){ // ���� � ����� ���������
    switch(newState){
      case 0:
        if(2 == state){ // ���������� conf ��� ������ �� "��������� T (�������)"
          saveConf();
        }
      case 1: // ���� � ��������� "����� T (�������)"
      case 2:
        led[0] = S7_t;
        s7Str2fixPoint(itoa16(conf.tr.t, buf), &led[1], 2, 0);
        restoreCounter = TICK_SEC(3);
        break;
      case 3:
        newState = 0;
        break;
    }
  }
  state = newState;
}


/*
  ������� ��������� ��������, �������� � ����� workflow - �������� ��������
  ����� ������ (workflow) �������� reset � ������� + ��� -
  F0 - workflow �� �������� (������� ���������, ������� ����� set + reset, �� ����. eeprom)
  return true - ������� �������� ������, conf ��������
  return false - ������ ��������, ��������� ������ ������ ��� ������
 */
bool a0Boot(){
  static u8 state = 2;
  static u8 a0Tick = sizeof(ledZ) + 200; // �������� ���� ���������, ������ + �����

  if(a0Tick && --a0Tick){ // ���� ��������
    return false;
  }

  if(0 == state){
    return true;
  }

  if(1 == state){
    if(0 == (btn & 0xf0)){ // ������ ��� �������� - ������ �� �����
      reset();
    }
  }else if(2 == state){
    loadConf();
    u8 wf;
    u8 b = btn & 0xf0; // ������������ ��� ������������ ������
    if(b){
      if(b == BTN_MINUS << 4){
        wf = WF_STABLE;
      }else if(b == BTN_PLUS << 4){
        wf = WF_TERMO;
      }else{
        wf = WF_NONE;
      }
      conf.wf.wf = wf; // ��������� workflow � conf, ��������� �������� � ���� ����� ���������� ������
      saveConf();
   //   s7Str2fixPoint(itoa16(conf.crc, buf), &led[0], 3, 0);
      state = 1;
    }else{
      state = 0;
    }
    a0Tick = TICK_SEC(1);
    led[0] = S7_F;
    led[1] = pgm_read_byte(&S7[conf.wf.wf]);
  }
  return false;
}

void main(void)
{

  mcuInit();
  led[0] = S7_SPACE;
  led[1] = S7_MINUS;
  led[2] = S7_SPACE;

//  u16 dsTick  = 1;
//  u8 dsState  = 0; // ��������� ������� �����������. �� ���� ��������� ������� �������������� (����� �� ������ �������� "85")
  u16 tickBlink   = 0;
  u8 blinkCnt     = 0;

  while (1)
  {
    if(TIFR & (1<<TOV0)){ // tick

      TIFR = (1<<TOV0);
      TCNT0 = 0xff + 1 - F_CPU / 256 / F_TICK; // 0x64

      tDSRead();
      btn = tbtnProcess(tLedAndKey()); // ����������, ����� ���� ���� � ������ ����� (���������)

      if(a0Boot()){
        switch(conf.wf.wf){
          case WF_TERMO: a2(); break;
        }
      }

      // blink
      if(tickBlink && !(--tickBlink)){
        tickBlink = TICK_SEC(1);
        PINA = bv(PA1);
        if(blinkCnt++ & 1){
          relayOn();
        }else{
          relayOff();
        }
      }
    }// tick
  }
}

/* ������������� ������ */
void mcuInit(){
  // ticks init
  //TIMSK = (1 << TOIE0);
  TCNT0 = 0xff;
  TCCR0B = (1 << CS02) | (0 << CS01) | (0 << CS00);
  // led init
  iopOutputLow(LED_Z_PORT, LED_Z_MASK);
  // �������
  iopOutputLow(RELAY_PORT, bv(RELAY_BIT));
  //  sei(); � ���������� ���!
}

/* ������������ ��������� + ����������� ������ 10�� */
u8 tLedAndKey(){
  static u8 ledIndex = 0;

  // ���� ��� �������
  iopHigh(LED_Z_PORT, LED_Z_MASK);

  // ���� ������
  iopInputP(LED_SEG_PORT, LED_BT_PIN_MASK); // ����� ������ �� ����
  iopOutputLow(LED_SEG_PORT, LED_BT_COMMON_MASK); // ��������� ������
  _delay_us(5); // ��� �� ���� �����. ����� ��������� ��������, ���� ���� )))
  u8 b = 0x08 | (LED_BT_PIN_MASK & (iopPin(LED_SEG_PORT)));

  if(++ledIndex > sizeof(ledZ) - 1){
    ledIndex = 0;
  }

  // �������� ��������� ������  SEG-low, Z-high
  iopOutputLow(LED_SEG_PORT, 0xff);
  iopSet(LED_SEG_PORT, ~led[ledIndex]);
  iopLow(LED_Z_PORT, pgm_read_byte(&ledZ[ledIndex]));
  return b;
}

/*
  ������ �����������
  return ������ ������: 0 - ���; 1 - no present; 2 - ���������
*/
void tDSRead(){
  static u8 state = 0;
  static u16 tick = 1;
  u8 err;

  if(tick && !(--tick)){
    err = w1Reset();
    if(err){
      dsErr = err;
      tick = TICK_SEC(3);
      return;
    }
    if(0 == state){ // ������ �� �������������� � ����
      w1rw(0xCC); //SKIP ROM [CCh]
      w1rw(0x44); //CONVERT  [44h]
      state++;
      tick = TICK_SEC(1);
    }else{ // 1 ������ � ��������
      w1rw(0xCC);//SKIP ROM [CCh]
      w1rw(0xBE);//READ SCRATCHPAD [BEh]
      *((u8*)&dsValue) = w1rw(0xff);
      *((u8*)&dsValue+1) = w1rw(0xff);
/*
dsCRC = 0;
for(u8 i = 0; i < 9; i++){
  u8 b = dsData[i] = w1rw(0xff);
  dsCRC = w1CRCUpdate(dsCRC, b);
}
dsData[9] = dsCRC;*/
    //todo �������� ���������� crc � �������� ������
    //  val = val < 0 ? 0 : (buf.u >> 3) + (buf.u >> 1); // 908 ��� ��� ���������� � �������
      dsValue = dsValue * 10 / 16; //  932 ������� � ������������� ����. � ������� �������
    //  val = val / 16; //  892  � ������������� ����. � ����� �������  ����� ������ �� �������
    //  val = val < 0 ? 0 : buf.i >> 4; //  898  � ����� ������� ��� �������������
      state = 0;
      tick = TICK_SEC(1);
    }
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

/* �������� conf � eep, ��������, �������� ����������� �������� � �������� � ������ ������
2  @return true - �������� �������, false - ���� ��������� ��������� �� ���������
*/
bool loadConf(){
  // ������ ���, ��������� crc, ���� �� �������, �� ������������ � ��������
  eeprom_read_block(&conf, &conf_e, sizeof(struct conf_s));
/*  u8* p = (u8*)&conf;
  EEAR = (uintptr_t)(&conf_e);
  for(u8 i = sizeof(struct conf_s); i > 0; i--){
    while (EECR & (1<<EEPE));
    EECR |= (1<<EERE);
    *p++ = EEDR;
    EEAR += 1;
  }*/

  if(crcConf(sizeof(struct conf_s))){ // ������ - ������ � �������� conf �� ���������. � eep �� �����, ����� ������ ��� ����������, ��� conf �����
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

void reset(){
  wdt_enable(WDTO_15MS);
  while(1);
}

// ������� ��� ���������� wdt ����� ������������ (wdt �������� ��������������)
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

void wdt_init(void){
//    MCUSR = 0;
    wdt_disable();
    return;
}

// ��������� dsData
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
  static u8 a0Tick = sizeof(ledZ) + 2; // �������� ���� ���������, ������ + �����
  // ��������� ��� ��������� tick, ���� 0, �� state + 1
  //  static u8 timerState = 0;

  switch(a0State){
    default: // 1 3 5
    if(a0Tick && !(--a0Tick)){
      a0State++;
      //      a0State = timerState ? timerState : a0State + 1;
    }
    break;
    //case 0:  // ������ �����. ��������, ���� �������� ������ - ����� ��������� ����� Workflow
    //   if(1){
    //      char *p = led; *p++ = S7_C; *p++ = S7_r; *p = S7_C;
    //    }
    //a0Tick = sizeof(ledZ) + 2;
    //a0State++;
    //break;
    case 2: // ��������
    loadConf();
    led[0] = S7_F;
    {
      u8 wf;
      switch(btn & 0xf0){ // ������������ ��� ������������ ������
        case 0: // ��������� ��� - ��������� ������
        a0State = 3;
        a0Tick = TICK_SEC(1);
        led[1] = pgm_read_byte(&S7[conf.wf.wf]);
        return false;  // RETURN ��� ������ � ����� switch
        default:            wf = WF_NONE;   break;
        case BTN_MINUS <<4: wf = WF_STABLE; break;
        case BTN_PLUS  <<4: wf = WF_TERMO;  break;
      }
      conf.wf.wf = wf; // ��������� workflow � conf, ��������� �������� � ���� ����� ���������� ������
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
    if(0 == (btn & 0xf0)){ // ������ ��� �������� - ������ �� �����
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
//if(tickDsRead && !(--tickDsRead)){ // ����� ������������. ��� ������� ����� ���������� ����� �� ������ 0 - ���������
//tickDsRead = 25; // 1 ���
//PORTD ^= bv(PD6);
//if((dsReadState = ds18b20Reader(dsReadState))){
//s7Str2fixPoint(itoa16(dsReadValue, buf), led, sizeof(ledZ), 1);
//}else{ // ������ ����� ��������� �������
//tickDsRead = TICK_SEC(1);  // ������������������� �� ������
//}
//}
//}



      if(dsTick && !(--dsTick)){ // ����� ������������. ��� ������� ����� ���������� ����� �� ������ 0 - ���������
        if((dsState = ds18b20Reader(dsState))){
          dsErr = dsState;
          s7Str2fixPoint(itoa16(dsValue, buf), led, sizeof(ledZ), 1);
          dsTick = TICK_SEC(2);
        }else{ // ������ ����� ��������� �������
          dsTick = TICK_SEC(1);  // ������������������� �� ������ ����� ����� ��������������
        }
      }


*/