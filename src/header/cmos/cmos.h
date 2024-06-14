#ifndef CMOS_H
#define CMOS_H

#include <stdint.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

uint8_t cmos_read(uint8_t reg);
void read_rtc(uint8_t* hours, uint8_t* minutes, uint8_t* seconds);

#endif // CMOS_H