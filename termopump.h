#pragma once

//  онфиг библиотек. ¬место подключени€ параметров через -D в строке параметров. Ётот файл подключен в проект через -include

#define F_CPU  8000000u
#define F_TICK 250uL     // частота системных тиков

#define TICK_US(n)                (uint16_t)(1e-6 * (n) * F_TICK + 0.5)
#define TICK_MS(n)                (uint16_t)(1e-3 * (n) * F_TICK + 0.5)
#define TICK_SEC(n)               (uint16_t)( 1.0 * (n) * F_TICK + 0.5)

// #define TASK(f, ms)   qtTask(f, (F_TICKS * (ms) / 1000 + 0.5))

#include "ss.lib/lib.h"
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/builtins.h>
#include <avr/wdt.h>


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

// порт разр€дов индикатора
#define LED_Z_PORT PORTD
// пор€зр€дные маски пинов дл€ массива (будет вставлено в массив const u8 PROGMEM ledZ[] = {LED_Z_SET}; )
#define LED_Z_SET  bv(PD6), bv(PD4), bv(PD5)
// маска дл€ всех пинов
#define LED_Z_MASK (bv(PD6)|bv(PD4)|bv(PD5))

// порт сегментов индикатора (тут же и кнопки)
#define LED_SEG_PORT PORTB

// пины чтени€ кнопок
#define LED_BT_PIN_MASK 0x07
// общее заземление кнопок (маска пина)
#define LED_BT_COMMON_MASK 0x80

#define BTN_SET   TBTN_1
#define BTN_MINUS TBTN_2
#define BTN_PLUS  TBTN_0
// биты кнопок с автоповтором
#define TBTN_REPEATE_MASK BTN_PLUS|BTN_MINUS

#define TBTN_DELAY_A      TICK_MS(750)

#define TBTN_DELAY_B      TICK_MS(175)

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
  #define a0High
#endif
    




