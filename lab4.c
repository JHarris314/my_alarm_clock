//****************************************************************************/
//Name: Jesse Harris
//Date: 06DEC2017
//Program: final lab (lab4.c)
//Purpose: The purpose of this lab is to develop an alarm clock from the
//already existing clock. The alarm will be a seperate mode from the clock and
//can be set using the push-buttons and the right encoder. The left encoder is
//used to change the volume of the alarm. Furthermore, a function that dims the
//display using the ADC and an external photo-resistor circuit is developed. is
//
//In addition, this code implements an internal temperature sensor and a radio.
//****************************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "hd44780.h"
#include "time.h"
#include "initialize.h"
#include "seven_seg.h"
#include "encoders.h"
#include "globals.h"
#include "check_buttons.h"
#include "lm73_functions.h"
#include "twi_master.h"
#include "si4734.h"

#define clr_bit(x, y) (x&=~(1<<y)); //clears a bit
#define set_bit(x, y) (x|=(1<<y));  //sets a bit 


//******************************************************************************/
//                           timer/counter0 ISR                          
//When the TCNT0 overflow occurs the second is incremented. If 60 seconds is
//hit the clock rolls over and one minute is added. Likewise, if 60 minutes is
//hit the clock rolls over and the hour increments. This function is also
//responsible for flashing the colon every second. 
//******************************************************************************/

ISR(TIMER0_OVF_vect) {
	static uint8_t j = 0;

	my_time.second++; 
	if (my_time.second > 59) { //add one minute
		my_time.minute++;
		if (my_time.minute > 59) { //roll over minutes and add one hour
			my_time.hour++;
			my_time.minute = 0;
		}
		my_time.second = 0; //roll over seconds
	}

	if (my_alarm.minute > 59) {
		my_alarm.hour++;
		my_alarm.minute = 0;
	}

	if (snooze_count > 0) {snooze_count--;}

	//Blink the colon when not in RADIO_MODE
	if (clock_mode != RADIO_MODE) {
		if (j == 0)
			segment_data[2] = dec_to_7seg[11]; //colon is illuminated
		else
			segment_data[2] = dec_to_7seg[10]; //colon is off
		j++; 

		if (j > 1) {j = 0;}

		alarm_toggle = !alarm_toggle; //toggle alarm sound each second

		twi_start_rd(LM73_ADDRESS, lm73_rd_buf, 2);
	}
}//ISR

//******************************************************************************/
//                           timer/counter1 ISR                          
//When TCNT1 counter is equal to the compare register value this ISR is called.
//The timer/counter1 ISR is responisble for toggleing PORTD bit 5 in order to
//generate a square wave at the desired frquency of 800 Hz.
//******************************************************************************/
ISR(TIMER1_COMPA_vect) {
	DDRD |= (1 << PD5); //set PD5 as an output
	if (alarm_engaged && alarm_toggle){ //sound alarm in one second increments
		PORTD ^= (1 << PD5);
	}//if
	else {PORTD = 0;}
}//ISR

//******************************************************************************/
//																alarm_handler	
//This function handles what occurs when the user selects the alarm to be armed. 
//It takes in a bool called alarm_armed and depending on the global snooze_count
//variable will display on the LCD that the alarm is on. If the alarm time and
//the clock time are the same, the disply changes and the alarm_engaged boolean
//is set true, which enables PORTD bit 5 to output.
//******************************************************************************/
void alarm_handler(bool alarm_armed) {
	if (alarm_armed && snooze_count == 0) {
		strcpy(alarm_array, "ALARM:ON");
		if (
				my_alarm.hour == my_time.hour &&
				my_alarm.minute == my_time.minute
			 ) {
			strcpy(alarm_array, "TIME TO RISE");
			alarm_engaged = true;
			return;
		}
	}
	alarm_engaged = false;
}


//******************************************************************************/
//                           timer/counter2 ISR                          
//When the TCNT2 overflow interrupt occurs, the count_7ms variable is    
//incremented. Every 7680 interrupts the minutes counter is incremented.
//TCNT0 interrupts come at 7.8125ms internals. write to bg, read from encoders
// 1/32768         = 30.517578uS
//(1/32768)*256    = 7.8125ms
//(1/32768)*256*64 = 500mS
//******************************************************************************/
ISR(TIMER2_OVF_vect) {
	static uint8_t encoder = 0; //stores current encoder state value (11, 10, 00, 01)
	static uint8_t past_encoder = 0; //stores previous encoder state value
	static uint8_t j = 0; //segment display variable
	static bool alarm_armed = false; //used to set arming

	//Load data from the encoders
	clr_bit(PORTE, PE6); //load data in the 165
	set_bit(PORTE, PE6); //shift data out the 165

	DDRA = 0x00;  //intitialize PORTA to inputs
	PORTA = 0xFF; //enable pull-ups
	PORTB = 0x70;

	//Check the buttons
	for(uint8_t i=0; i < 8; i++) {
		if(chk_buttons(i)) { //if button is pressed
			switch(i) { //cases for buttons pressed
				case 1: time = TIME_SELECT_HOUR; //choose hour using right encoder
								break;
				case 2: time = TIME_SELECT_MINUTE; //choose minute using right encoder
								break;
				case 3: clock_mode = (
										(clock_mode == TIME_MODE)
										? RADIO_MODE
										: TIME_MODE
										);
								clear_display();
								break;
				case 5: if (clock_mode != SNOOZE_MODE) { //set snooze mode
									clock_mode = SNOOZE_MODE;
									snooze_count = 10;
								}
								else {
									clock_mode = TIME_MODE; //else enter time mode
									snooze_count = 0;
								}
								break;
				case 6: alarm_armed = !alarm_armed; //toggle arming the alarm
								break;
				case 7: clock_mode = ( //ternary for mode selection
										(clock_mode == TIME_MODE)
										? ALARM_MODE
										: TIME_MODE
										);
								clear_display();	
								break;
			}//switch
		}//if			
	}//for

	switch (clock_mode) {
		case ALARM_MODE: //display the alarm time
			disp_value = (my_alarm.hour * 100) + my_alarm.minute;
		  strcpy(alarm_array, "SET ALARM"); //write to LCD display
			break;
		case TIME_MODE: //display the time
			disp_value = (my_time.hour * 100) + my_time.minute;
			strcpy(alarm_array, "ALARM:OFF"); //write to LCD display
			break;
		case SNOOZE_MODE: //set snooze
			alarm_engaged = false;
			strcpy(alarm_array, "SNOOZE");
			break;
		case RADIO_MODE:
		  segment_data[2] = dec_to_7seg[10];	
			disp_value = encoder_freq/10; //shift to rid display of trailing zero
			strcpy(alarm_array, "RADIO ON");
			break;
		default: break;
	} //switch

	alarm_handler(alarm_armed); //handle the alarm functionality

	SPDR = my_time.second; //shift seconds (will display on bar graph)
	while (bit_is_clear(SPSR, SPIF)){} //wait until the end of the load
	PORTB |= 0x01;  //rising edge for ss_n pin on 595
	PORTB &= ~0x01; //falling edge for ss_n pin on 595

	//Shift data from MISO into SPDR
	encoder = SPDR; //set encoder equal to the SPI data register
	encoder &= (0x0F); //set all encoder bits (3:0) high

	segsum(disp_value); //call segsum
	//Encoder values are stored in 4 bits. The upper two bits are for the
	//right encoder and the lower two are for the left. 
	right_encoder((past_encoder & 0x0C) >> 2, (encoder & 0x0C) >> 2);
	left_encoder((past_encoder & 0x03), (encoder & 0x03));

	if (my_time.hour > 24) {my_time.hour = 0;}
	if (my_time.minute > 59) {my_time.minute = 0;}
	past_encoder = encoder; //remember current state for next interrupt

	//Loop through segments
	if (j > 4) {j = 0;}
	DDRA = 0xFF;
	PORTA = segment_data[j];
	PORTB = (j << 4);
	j++;

	strncpy(temp_in_display, temperature, 16);
	strncpy(alarm_display, alarm_array, 16);
	refresh_lcd(lcd_string_array);
}//ISR

//******************************************************************************/
//																	ISR(ADC_vect)
//This ISR enables the ADC in such a way as to handle the photo-resistor and 
//provide a dimming effect in the presence of low light.
//******************************************************************************/
ISR(ADC_vect) {
	OCR2 = (ADC >> 2); //generates waveform on the OC2 pin (shift right to account for 10-bit value)
	ADCSRA |= (1 << ADSC); //write start conversion bit for single conversion mode
}

//******************************************************************************/
//																	ISR(USART0_vect)
//This ISR deals with the ATMega48 and its USART communication of external 
//temperature.
//******************************************************************************/
//ISR(USART0_RX_TX_vect) {
//	static uint8_t i;
//	out_temp = UDR0;	//get the outside temp.
//	lcd_str_array[i++] = out_temp;
//
//  //If at the end, set the flag and reset the index
//	if (out_temp == '\0') {
//		rcv_rdy = 1;
//		i=0;
//	}
//}
//******************************************************************************/
//																	ISR(INT7_vect)
//******************************************************************************/
ISR(INT7_vect) {STC_interrupt = TRUE;}
//******************************************************************************/
//                                main                                 
//******************************************************************************/
int main(){     
	DDRB |= 0xF0; //bits 4-7 outputs
	DDRE = 0x40;  //set bit 6 to output

	//Call initializing functions
	tcnt2_init();
	tcnt0_init();
	tcnt3_init();
	spi_init();  
	tcnt1_init();
	adc_init();  
	lcd_init(); 
	init_twi();	
	radio_init();

	//enable interrupts
	sei();

	//Fire up the radio
	fm_pwr_up();
	current_fm_freq = encoder_freq;
	fm_tune_freq();

	twi_start_wr(LM73_ADDRESS, lm73_wr_buf, 1);
	//radio_pwr_dwn();

	while(1){
		lm73_temp = lm73_rd_buf[0];   //Read in 16-bit temperature data
		lm73_temp = (lm73_temp << 8); 
		lm73_temp |= lm73_rd_buf[1];
		disp_temp = (lm73_temp/128);  //convert to celcius value to be displayed
		sprintf(temperature, "IN:%dC\xDFOUT: :[", disp_temp);
		end_of_string = strlen(temperature); 

		
    //Place spaces in empty index
		for (uint8_t i = end_of_string; i < 16; i++)
			temperature[i] = ' ';

    //
		if (clock_mode == RADIO_MODE) {
			fm_pwr_up();
			current_fm_freq = encoder_freq;
			fm_tune_freq();
			_delay_ms(100);
		}
		if (clock_mode != RADIO_MODE) {radio_pwr_dwn();}
	} //empty main while loop
} //main
