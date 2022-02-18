#pragma once

// ������ ���������. ������ ����������� ���������� ����� -D � ������ ����������. ���� ���� ��������� � ������ ����� -include

#define F_CPU  8000000u
#define F_TICK 250uL     // ������� ��������� �����

#define TICK_US(n)                (uint16_t)(1e-6 * (n) * F_TICK + 0.5)
#define TICK_MS(n)                (uint16_t)(1e-3 * (n) * F_TICK + 0.5)
#define TICK_SEC(n)               (uint16_t)( 1.0 * (n) * F_TICK + 0.5)

#ifdef DEBUG
  #define T_SCRSVR  TICK_SEC(15) // SCReen SaVer
  #define T_DEFSCR  TICK_SEC(5)
  #define T_ADJUSTMENT TICK_SEC(3)
  #define T_SAVE    TICK_SEC(5) 
#else
  #define T_SCRSVR  TICK_SEC(180)
  #define T_DEFSCR  TICK_SEC(15)
  #define T_ADJUSTMENT TICK_SEC(600)  
  #define T_SAVE    TICK_SEC(5)
#endif

 #define USE_TX_LOG

#define BAUD 9600

#include "ss.lib/lib.h"
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/builtins.h>
#include <avr/wdt.h>
#include <util/setbaud.h>

#define QT_TASK_COUNT 5

#include "ss.lib/queuetask.h"

#define S7_SET S7_0, S7_1, S7_2, S7_3, S7_4, S7_5, S7_6, S7_7, S7_8, S7_9, S7_A, S7_b, S7_C, S7_d, S7_E, S7_F, \
        S7_SPACE, S7_MINUS, S7_t, S7_L

#define ZG_SPACE 0x10
#define ZG_d 0xd
#define ZG_MINUS (ZG_SPACE + 1)
#define ZG_t (ZG_MINUS + 1)
#define ZG_L (ZG_t + 1)

#define S7_SEG_A 7
#define S7_SEG_B 0
#define S7_SEG_C 2
#define S7_SEG_D 4
#define S7_SEG_E 5
#define S7_SEG_F 6
#define S7_SEG_G 1
#define S7_SEG_P 3

// ���� �������� ����������
#define LED_Z_PORT PORTD
// ����������� ����� ����� ��� ������� (����� ��������� � ������ const u8 PROGMEM ledZ[] = {LED_Z_SET}; )
#define LED_Z_SET  bv(PD6), bv(PD4), bv(PD5)
// ����� ��� ���� �����
#define LED_Z_MASK (bv(PD6)|bv(PD4)|bv(PD5))

// ���� ��������� ���������� (��� �� � ������)
#define LED_SEG_PORT PORTB

// ���� ������ ������
#define LED_BT_PIN_MASK 0x07
// ����� ���������� ������ (����� ����)
#define LED_BT_COMMON_MASK 0x80

#define BTN_SET   TBTN_1
#define BTN_MINUS TBTN_2
#define BTN_PLUS  TBTN_0
// ���� ������ � ������������
#define TBTN_REPEATE_MASK BTN_PLUS|BTN_MINUS

#define TBTN_DELAY_A      TICK_MS(750)

#define TBTN_DELAY_B      TICK_MS(275)

#define RELAY_PORT  PORTD
#define RELAY_BIT   PD2

#include "ss.lib/buttons.h"

// #define S7_SEG_INVERT

#include "ss.lib/s7.h"

#define W1_PORT PORTD

#define W1_BIT  PD3

#include "ss.lib/w1.h"

#include "ss.lib/itoa.h"


#ifdef DEBUG
  #define A0Low  iopLow(PORTA, bv(PA0))
  #define A0High iopHigh(PORTA, bv(PA0))
#else
  #define A0Low
  #define A0High
#endif



