#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdbool.h>

//Declare global variables

//External variables
extern uint8_t lm73_wr_buf[];
extern uint8_t lm73_rd_buf[];
extern char    lcd_string_array[];

//Time variables
volatile Time my_time = {0, 0, 0};
volatile Time my_alarm = {0, 0, 0};
volatile TimeSelection time = TIME_SELECT_HOUR;
volatile TimeSelection alarm = TIME_SELECT_HOUR;
volatile ClockMode clock_mode = TIME_MODE;

//Int variables for counts
volatile uint8_t  snooze_count = 0; //snooze delay
volatile uint8_t  end_of_string = 0;
volatile uint16_t disp_value = 0; //7seg display
volatile uint16_t disp_temp = 0;
volatile uint16_t lm73_temp;

//Alarm bools
volatile bool alarm_toggle = false; 
volatile bool alarm_engaged= false;

//Display variables
char temperature [16];
char alarm_array [16];
char*  alarm_display    = &lcd_string_array[0];
char*  temp_in_display  = &lcd_string_array[16];

//Radio Variables
extern enum radio_band{FM, AM, SW};
extern volatile uint8_t STC_interrupt;
volatile enum radio_band current_radio_band = FM;

uint16_t eeprom_fm_freq;
uint16_t eeprom_am_freq;
uint16_t eeprom_sw_freq;
uint8_t  eeprom_volume;

volatile uint16_t current_fm_freq;
volatile uint16_t encoder_freq = 9990;
uint16_t current_am_freq;
uint16_t current_sw_freq;
uint8_t  current_volume;


#endif
