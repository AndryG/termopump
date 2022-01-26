const u8 PROGMEM ledZ[] = {LED_Z_SET};

/* ����������� */
char led[sizeof(ledZ)] = {S7_SPACE, S7_MINUS, S7_SPACE};

char buf[12];

// ������� ������
u8 btn;

struct conf_s {
  struct confWf{
    u8 wf; // �������� ������ ��� ��������
  } wf;
  struct confTr{
    i16 t; // ������������� �������
    u8 dt; // ���������� ����������� � ������� �������
  } tr;
  struct confSt{
    u8 power;  // 0-1 ��������� ���� ��� ���������
  } st;
  u8 crc; // ��� �������� ������ � ���
};

#define CONF_DEFAULT {WF_NONE}, {9, 10}, {0}

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
struct conf_s EEMEM conf_e = {CONF_DEFAULT, 168};

struct conf_s conf;


i16 dsReadValue;

u8 dsData[10];

u8 dsCRC;

void tLedAndKey();
void mcuInit();
uint8_t ds18b20Reader(u8 state);

void reset(){
  iopOutputLow(PORTD, bv(PD0));
}

u8 crcConf(u8 length){
  u8* b = (void*)(&conf);
  u8 crc = 7;
  for(u8 i = length; i > 0; i--){
    crc = w1CRCUpdate(crc, *(b++));
  }
  return crc;
}

/* �������� conf � eep, ��������, �������� ����������� �������� � ��������
2  @return true - �������� �������, false - ���� ��������� ��������� �� ���������
*/
bool loadConf(){
  // ������ ���, ��������� crc, ���� �� �������, �� ������������ � ��������
  eeprom_read_block(&conf, &conf_e, sizeof(struct conf_s));
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

/* ������� ��������� ��������, �������� � ����� workflow - �������� ��������*/
bool a0Boot(){
  static u8 a0State = 1;
  static u8 a0Tick = sizeof(ledZ) + 2;

  switch(a0State){
    default: // 1 3 5
      if(a0Tick && !(--a0Tick)){
        a0State++;
      }
    break;
    //case 0:  // ������ �����. ��������, ���� �������� ������ - ����� ��������� ����� Workflow
   ////   if(1){
  ////      char *p = led; *p++ = S7_C; *p++ = S7_r; *p = S7_C;
  ////    }
      //a0Tick = sizeof(ledZ) + 2;
      //a0State++;
     //break;
    case 2: // �������� �
      loadConf();
      led[0] = S7_F;
      {
        u8 wf;
        switch(btn & 0xf0){
          case 0: // ��������� ��� - ��������� ������
            a0State = 3;
            a0Tick = TICK_SEC(1);
            led[1] = pgm_read_byte(&S7[conf.wf.wf]);
            return false;  // RETURN ��� ������ � ����� switch
          default:           wf = WF_NONE;   break;
          case BTN_MINUS<<4: wf = WF_STABLE; break;
          case BTN_PLUS <<4: wf = WF_TERMO;  break;
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
        a0State++;
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
u16 blink = 1;

int main(void)
{
  iopOutputLow(PORTA, bv(PA1));
  while(0){
    iopLow(PORTA, bv(PA1));
    _delay_ms(250);
    iopHigh(PORTA, bv(PA1));
    _delay_ms(250);
  }
/*

    iopOutputLow(PORTA, PA0);
    _delay_ms(250);
    iopOutputHigh(PORTA, PA0);
    _delay_ms(250);
  }
*/

  u8 dsReadState = 1; // ��������� ������� �����������. �� ���� ��������� ������� �������������� (����� �� ������ �������� "85")
  volatile u16 tickDsRead = 1;
  mcuInit();

  //led[0] = loadConf() ? S7_1 : S7_0;
  //saveConf();
  //led[1] = loadConf() ? S7_1 : S7_0;
  ////conf.tr.dt = 20;
  //saveConf();
 //// led[2] = loadConf(&conf) ? S7_1 : S7_0;
 //// s7Str2fixPoint(itoa16(conf.crc, buf), led, 3, 0);

 //
//      conf.crc = 0;

  led[0] = S7_SPACE;
  led[1] = S7_MINUS;
  led[2] = S7_SPACE;
  u8 n = 0;
  u16 btnTick = 0;
  while (1)
  {
    if(TIFR & (1<<TOV0)){ // every 10ms

      TIFR = (1<<TOV0);
      TCNT0 = 0xff + 1 - F_CPU / 256 / F_TICK; // 0x64

      tLedAndKey();
      if(btnTick && !(--btnTick)){
          led[0] = pgm_read_byte(&S7[btn >> 4]);
          led[1] = pgm_read_byte(&S7[btn & 0x0F]);
          btnTick = 1;
          if(btn & 0x0f){
//            btnTick = TICK_MS(250);
            led[2] = led[1];
          }
      }          
      
/*
      if(btn){
        if(btn & BTN_MINUS){
          n -= 1;
        }
        if(btn & BTN_PLUS){
          n += 1;
        }
        if(btn & BTN_SET){
          n = 127;
        }
        btn = 0;
        s7Str2fixPoint(itoa16(n, buf), led, sizeof(led), 0);
      }*/

      if(tickDsRead && !(--tickDsRead)){ // ����� ������������. ��� ������� ����� ���������� ����� �� ������ 0 - ���������
//        tickDsRead = 25; // 1 ���
//        PORTD ^= bv(PD6);
        if((dsReadState = ds18b20Reader(dsReadState))){
   //       blink = 1;
     //     n = 0;
          s7Str2fixPoint(itoa16(dsReadValue, buf), led, sizeof(ledZ), 1);
          tickDsRead = TICK_SEC(3);
          }else{ // ������ ����� ��������� �������
          tickDsRead = TICK_SEC(1);  // ������������������� �� ������
        }
      //  tickDsRead = TICK_SEC(1);  // ������������������� �� ������
      }
      //a0Boot();
   ///   if(a0Boot()){
  //      led[2] = S7_MINUS;
   //   }
                                //  led[1] = S7_MINUS;
      if(blink && !(--blink)){
        blink = TICK_SEC(1);
        PINA = bv(PA1);
        if(n++ & 1){
          relayOn();
        }else{
          relayOff();
        }
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
void tLedAndKey(){
  static u8 ledCnt = 0;

  // ���� ��� �������
  iopHigh(LED_Z_PORT, LED_Z_MASK);

  // ���� ������
  iopInputP(LED_SEG_PORT, LED_BT_PIN_MASK); // ����� ������ �� ����
  iopOutputLow(LED_SEG_PORT, LED_BT_COMMON_MASK); // ��������� ������
  _delay_us(30); // ��� �� ���� �����. ����� ��������� ��������, ���� ���� )))
  btn = tbtnProcess(0x08 | (LED_BT_PIN_MASK & (iopPin(LED_SEG_PORT)))); // ����������, ����� ���� ���� � ������ ����� (���������)
  
  if(++ledCnt > sizeof(ledZ) - 1){
    ledCnt = 0;
  }

  // �������� ��������� ������  SEG-low, Z-high
  iopOutputLow(LED_SEG_PORT, 0xff);
  iopSet(LED_SEG_PORT, ~led[ledCnt]);
  iopLow(LED_Z_PORT, pgm_read_byte(&ledZ[ledCnt]));
}

uint8_t ds18b20Reader(u8 state){
  u8 err = w1Reset();
  if(err){
    return 10 + err;
  }
  if(state){ // ���� ���������� (������ ��� ������) - ������ ����� ����
    w1rw(0xCC); //SKIP ROM [CCh]
    w1rw(0x44); //CONVERT  [44h]
    return 0;
  }else{ // ������� ������ ������ ����, ������ ������ ������
    w1rw(0xCC);//SKIP ROM [CCh]
    w1rw(0xBE);//READ SCRATCHPAD [BEh]
    *((u8*)&dsReadValue) = w1rw(0xff);
    *((u8*)&dsReadValue+1) = w1rw(0xff);
/*
dsCRC = 0;
for(u8 i = 0; i < 9; i++){
  u8 b = dsData[i] = w1rw(0xff);
  dsCRC = w1CRCUpdate(dsCRC, b);
}
dsData[9] = dsCRC;*/
    //todo �������� ���������� crc � �������� ������
    //  val = val < 0 ? 0 : (buf.u >> 3) + (buf.u >> 1); // 908 ��� ��� ���������� � �������
    dsReadValue = dsReadValue * 10 / 16; //  932 ������� � ������������� ����. � ������� �������
 //dsReadValue >>= 4;
    //  val = val / 16; //  892  � ������������� ����. � ����� �������  ����� ������ �� �������
    //  val = val < 0 ? 0 : buf.i >> 4; //  898  � ����� ������� ��� �������������
    return 1; // ������ ������
  }
}

/*   �������� ������

  static u8 t = 0;
  if(t == 0){
    if(btn & 0x0f){
      led[0] = pgm_read_byte(&S7[btn & 0x0f]);
      t = 30;
    }
    }else{
    t--;
    if(t == 0){
      led[0] = S7_0;
    }
  }
  led[2] = pgm_read_byte(&S7[btn >> 4]) ;*/