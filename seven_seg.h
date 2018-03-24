#ifndef SEVEN_SEG_H
#define SEVEN_SEG_H

#include "globals.h"
//holds data to be sent to the segments. logic zero turns segment on
volatile uint8_t segment_data[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

//decimal to 7-segment LED display encodings, logic "0" turns on segment
uint8_t dec_to_7seg[13] = {
	0b11000000, //number 0
	0b11111001, //number 1
	0b10100100, //number 2
	0b10110000, //number 3
	0b10011001, //number 4
	0b10010010, //number 5
	0b10000010, //number 6
	0b11111000, //number 7
	0b10000000, //number 8
	0b10011000, //number 9
	0b11111111, //all segments off
	0b11111100, //all segments on
  0b01111111
};

//******************************************************************************/
//                                   segment_sum                                    
//takes a 16-bit binary input value and places the appropriate equivalent 4 digit 
//BCD segment code in the array segment_data for display.                       
//array is loaded at exit as:  |digit3|digit2|colon|digit1|digit0|
//******************************************************************************/
void segsum(uint16_t sum) {
	//break up decimal sum into 4 digit-segments
	uint8_t digit3 = dec_to_7seg[(sum/1000)];     //thousands place
	uint8_t digit2 = dec_to_7seg[(sum/100) % 10]; //hundreds place
	uint8_t digit1 = dec_to_7seg[(sum/10)  % 10]; //tens place
	uint8_t digit0 = dec_to_7seg[(sum/1)   % 10]; //ones place 

	if (clock_mode == RADIO_MODE) {
		digit1 &= ~(1UL << 7);
		if ( digit3 == dec_to_7seg[0]) {
			digit3 = dec_to_7seg[10];
		}
	}
	//now move data to right place for misplaced colon position
	segment_data[0] = digit0;
	segment_data[1] = digit1;
	segment_data[3] = digit2;
	segment_data[4] = digit3;
}//segment_sum
#endif
