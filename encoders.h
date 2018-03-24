#ifndef ENCODERS_H
#define ENCODERS_H

#include "globals.h"
#include "time.h"
//******************************************************************************
//                              left_encoder
//Takes the past encoder value and the present encoder value and shifts                                                             
//them into an eight bit integer. This value is represented as a case in
//a switch statement. Four cases are used to increase the volume and four cases
//are used to decrease the volume. 
//******************************************************************************/
void left_encoder(uint8_t past_encoder, uint8_t  encoder) {
	uint8_t direc = (past_encoder << 2 | encoder);
	switch (direc) { //Determine whether to increase or decrease the volume
		//Increase volume
		case 0x01: 
		case 0x0E:	  
		case 0x08: 
		case 0x07: OCR3A += 4;
							 break;
		//Decrease volume
		case 0x04: 
		case 0x0B: 
		case 0x02: 
		case 0x0D: OCR3A -= 4;
							 break;
		default: break;
	}//switch

}//left_encoder

//******************************************************************************/
//                             right_ encoder
//Takes the past encoder value and the present encoder value and shifts
//them into an eight bit integer. This value is represented as a case in a
//switch statement. Each case is either skipped or increments the display value
//in accord with the selected state. 
//******************************************************************************/
void right_encoder(uint8_t past_encoder, uint8_t  encoder) {
	volatile Time *modifier;
	uint8_t direc = (past_encoder << 2 | encoder);
	//switch statement for clock_mode only
	switch (clock_mode) {
		case TIME_MODE:
		default:
			modifier = &my_time;
			break;
		case ALARM_MODE:
			modifier = &my_alarm;
			break;
	}

	if (clock_mode == TIME_MODE || clock_mode == ALARM_MODE) {
		//Determine direction of encoder for time adjustment
		switch (direc) { 
			case 0x07: //encoder turned right (increment)
				if (time == TIME_SELECT_MINUTE) {modifier->minute++;} 
				else if (time == TIME_SELECT_HOUR) {modifier->hour++;}	 
				break;

			case 0x0D: //encoder turned left (decrement)
				if (time == TIME_SELECT_MINUTE) {modifier->minute--;} 
				else if (time == TIME_SELECT_HOUR) {modifier->hour--;}
				break;
			default: break;
		}//switch
	}//if

	if (clock_mode == RADIO_MODE) {

		switch (direc) { 
			case 0x07: //encoder turned right (increment)
				//increase the current_fm_freq
				if (current_fm_freq < 10790) {
					encoder_freq += 20;
				}
				break;

			case 0x0D: //encoder turned left (decrement)
				//decrease the current_fm_freq
				if (encoder_freq > 8810) {
					encoder_freq -=20;
				}
				break;
			default: break;
		}//switch
		
	}
}//right_encoder

#endif
