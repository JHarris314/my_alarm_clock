#ifndef CHECK_BUTTONS
#define CHECK_BUTTONS


//******************************************************************************/
//                            chk_buttons                                      
//Checks the state of the button number passed to it. It shifts in ones till
//the button is pushed. Function returns a 1 only once per debounced button
//push so a debounce and toggle function can be implemented at the same time.
//Adapted to check all buttons from Ganssel's "Guide to Debouncing" Expects
//active low pushbuttons on PINA port.  Debounce time is determined by external
//loop delay times 12. 
//******************************************************************************/
uint8_t chk_buttons(uint8_t button) {
	static uint16_t state[8] = {0}; //holds present state
	state[button] = (state[button] << 1) | (! bit_is_clear(PINA, button)) | 0xE000; //establishes state of button
	if (state[button] == 0xF000) return 1; //if 4 MSB's are high return 1
	return 0;
}

#endif
