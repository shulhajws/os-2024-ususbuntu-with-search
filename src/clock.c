#include "header/cpu/portio.h"
#include "header/clock.h"

void out_byte(uint16_t port, uint8_t value){
      out(port, value);
}

uint8_t in_byte(uint16_t port){
      return in(port);
}

int century_register = 0x00;

unsigned char second;
unsigned char minute;
unsigned char hour;
unsigned char day;
unsigned char month;
unsigned int year;
 
enum {
      cmos_address = 0x70,
      cmos_data    = 0x71
};
 
int get_update_in_progress_flag() {
      out_byte(cmos_address, 0x0A);
      return (in_byte(cmos_data) & 0x80);
}
 
unsigned char get_RTC_register(int reg) {
      out_byte(cmos_address, reg);
      return in_byte(cmos_data);
}
 
void read_rtc() {
      unsigned char century;
      unsigned char last_second;
      unsigned char last_minute;
      unsigned char last_hour;
      unsigned char last_day;
      unsigned char last_month;
      unsigned char last_year;
      unsigned char last_century;
      unsigned char registerB;
 
      // Note: This uses the "read registers until you get the same values twice in a row" technique
      //       to avoid getting dodgy/inconsistent values due to RTC updates
 
      while (get_update_in_progress_flag());                // Make sure an update isn't in progress
      second = get_RTC_register(0x00);
      minute = get_RTC_register(0x02);
      hour = get_RTC_register(0x04);
      day = get_RTC_register(0x07);
      month = get_RTC_register(0x08);
      year = get_RTC_register(0x09);
      if(century_register != 0) {
            century = get_RTC_register(century_register);
      }
 
      do {
            last_second = second;
            last_minute = minute;
            last_hour = hour;
            last_day = day;
            last_month = month;
            last_year = year;
            last_century = century;
 
            while (get_update_in_progress_flag());           // Make sure an update isn't in progress
            second = get_RTC_register(0x00);
            minute = get_RTC_register(0x02);
            hour = get_RTC_register(0x04);
            day = get_RTC_register(0x07);
            month = get_RTC_register(0x08);
            year = get_RTC_register(0x09);
            if(century_register != 0) {
                  century = get_RTC_register(century_register);
            }
      } while( (last_second != second) || (last_minute != minute) || (last_hour != hour) ||
               (last_day != day) || (last_month != month) || (last_year != year) ||
               (last_century != century) );
 
      registerB = get_RTC_register(0x0B);
 
      // Convert BCD to binary values if necessary
 
      if (!(registerB & 0x04)) {
            second = (second & 0x0F) + ((second / 16) * 10);
            minute = (minute & 0x0F) + ((minute / 16) * 10);
            hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
            day = (day & 0x0F) + ((day / 16) * 10);
            month = (month & 0x0F) + ((month / 16) * 10);
            year = (year & 0x0F) + ((year / 16) * 10);
            if(century_register != 0) {
                  century = (century & 0x0F) + ((century / 16) * 10);
            }
      }
 
      // Convert 12 hour clock to 24 hour clock if necessary
 
      if (!(registerB & 0x02) && (hour & 0x80)) {
            hour = ((hour & 0x7F) + 12) % 24;
      }
 
      // Calculate the full (4-digit) year
      if(century_register != 0) {
            year += century * 100;
      } else {
            year += (CURRENT_YEAR / 100) * 100;
            if(year < CURRENT_YEAR) year += 100;
      }

// Adjust the hour for GMT+7
    hour += 7;
    if (hour >= 24){
        hour -= 24;
        day += 1;

        // Handle month overflow
        if ((month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) && day > 31) {
            day = 1;
            month += 1;
        } else if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) {
            day = 1;
            month += 1;
        } else if (month == 2) {
            if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
                if (day > 29) {
                    day = 1;
                    month += 1;
                }
            } else {
                if (day > 28) {
                    day = 1;
                    month += 1;
                }
            }
        }

        // Handle year overflow
        if (month > 12) {
            month = 1;
            year += 1;
        }
    }
}

// int main(void){
//       char buf[9];
//       read_syscall(request, &retcode);
//       while (true){
//             read_rtc();
//             custom_sprintf(buf, "%02d:%02d:%02d", hour, minute, second);
//             puts(buf, 9, 0xF);
//       }
// }