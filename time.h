#ifndef TIME_H
#define TIME_H

//Type declarations
typedef enum { //holds enums for time selection
	TIME_SELECT_HOUR = 0x10,
	TIME_SELECT_MINUTE = 0x01
} TimeSelection;

typedef enum { //holds enums for modes
	TIME_MODE,
	ALARM_MODE,
	SNOOZE_MODE,
	RADIO_MODE
} ClockMode;

typedef struct { //struct for clock and alarm
		uint8_t second;
		uint8_t minute;
		uint8_t hour;
} Time;

#endif
