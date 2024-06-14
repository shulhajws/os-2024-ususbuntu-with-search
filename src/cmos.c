#include "header/cmos/cmos.h"
#include "portio.h"

// Read a byte from the CMOS register
uint8_t cmos_read(uint8_t reg) {
    out(CMOS_ADDRESS, reg);
    return in(CMOS_DATA);
}

// Read the current time from the RTC
void read_rtc(uint8_t* hours, uint8_t* minutes, uint8_t* seconds) {
    *seconds = cmos_read(0x00);
    *minutes = cmos_read(0x02);
    *hours = cmos_read(0x04);

    // Convert BCD to binary values if necessary
    uint8_t bcd = cmos_read(0x0B);
    if (!(bcd & 0x04)) {
        *seconds = ((*seconds / 16) * 10) + (*seconds & 0x0F);
        *minutes = ((*minutes / 16) * 10) + (*minutes & 0x0F);
        *hours = ((*hours / 16) * 10) + (*hours & 0x0F);
    }
}