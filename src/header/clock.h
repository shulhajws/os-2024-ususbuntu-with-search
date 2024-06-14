#ifndef _CLOCK_H
#define _CLOCK_H

#define CURRENT_YEAR 2024 // Change this each year!

extern int century_register; // Set by ACPI table parsing code if possible
 
extern unsigned char second;
extern unsigned char minute;
extern unsigned char hour;
extern unsigned char day;
extern unsigned char month;
extern unsigned int year;

#include <stdint.h>

void out_byte(uint16_t port, uint8_t value);

uint8_t in_byte(uint16_t port);
 
int get_update_in_progress_flag();
 
unsigned char get_RTC_register(int reg);
 
void read_rtc();

#endif
