/*
MCP794xx driver
Written by: Erik Johnson

NOTE:
   - For CCS PIC C, a line such as "#use i2c(I2C1, MASTER, FAST=400000, FORCE_HW, STREAM=I2C_STREAM)" must appear before mcp794xx.h is included
   - For other compilers, the I2C commands in readI2C and writeI2C must be ported to your compiler

TODO: 
   - Oscillator frequency calibration (coarse/fine)
   - Power down/up log (reading/clearing)
   - readMulti / writeMulti (specifically for internal EEPROM)
*/

#include "MCP794xx.h"

struct_tm now;

typedef enum {
   MCP_CLOCK = 0,
   MCP_RAM,
   MCP_EEPROM,
   MCP_UID
} MCP794xx_block;

typedef enum {
   RTC_SECOND = 0x00,
   RTC_MINUTE,
   RTC_HOUR,
   RTC_WEEKDAY,
   RTC_DATE,
   RTC_MONTH,
   RTC_YEAR,
   RTC_CONTROL,
   RTC_OSCTRIM,
   RTC_EEUNLOCK,
   ALM0_SECOND = 0x0A,
   ALM0_MINUTE,
   ALM0_HOUR,
   ALM0_WEEKDAY,
   ALM0_DATE,
   ALM0_MONTH,
   ALM1_SECOND = 0x11,
   ALM1_MINUTE,
   ALM1_HOUR,
   ALM1_WEEKDAY,
   ALM1_DATE,
   ALM1_MONTH,
   POWERDOWN_MINUTE = 0x18,
   POWERDOWN_HOUR,
   POWERDOWN_DATE,
   POWERDOWN_MONTH,
   POWERUP_MINUTE = 0x1C,
   POWERUP_HOUR,
   POWERUP_DATE,
   POWERUP_MONTH
} MCP794xx_clock;

// PRIVATE PROTOTYPES - BEGIN
unsigned int8 readMCP794xx(MCP794xx_block, unsigned int8);
void writeMCP794xx(MCP794xx_block, unsigned int8, unsigned int8);
void unlockMCP794xx_EUIblock(void);
unsigned int8 readI2C(unsigned int8, unsigned int8);
void writeI2C(unsigned int8, unsigned int8, unsigned int8);
unsigned int8 decodeBCD(unsigned int8);
unsigned int8 encodeBCD(unsigned int8);
int1 isLeapYear(unsigned int16);
unsigned int8 daysInMonth(unsigned int8, int1);
// PRIVATE PROTOTYPES - END

/*
time()

   DESCRIPTION:   gets the current time from the RTC
   PARAMETERS:    none
   RETURNS:       the current date and time stored as time_t
*/
time_t time() {
   return mktime(readRTC());
}

/*
setTime(struct_tm *)

   DESCRIPTION:   sets the current time, correcting the dayofweek and dayofyear fields
   PARAMETERS:    struct_tm *          - pointer to the current date and time
   RETURNS:       none
*/
void setTime(struct_tm * nTime) {
   mktime(nTime);
   writeRTC(nTime);
}

/*
setTimeEpoch(time_t)

   DESCRIPTION:   sets the current time
   PARAMETERS:    time_t               - the current epoch time
   RETURNS:       none
*/
void setTimeEpoch(time_t sTime) {
   struct_tm * temp = localtime(&sTime);
   setTime(temp);
   free(temp);
}

/*
readRTC()

   DESCRIPTION:   reads the current date and time from the RTC module, updates the local copy of the current time
   PARAMETERS:    none
   RETURNS:       pointer to the current date and time stored as struct_tm
*/
struct_tm * readRTC() {
   now.tm_sec  = decodeBCD(readMCP794xx(MCP_CLOCK, RTC_SECOND) & 0x7F);
   now.tm_min  = decodeBCD(readMCP794xx(MCP_CLOCK, RTC_MINUTE) & 0x7F);
   now.tm_hour = decodeBCD(readMCP794xx(MCP_CLOCK, RTC_HOUR) & 0x7F);
   now.tm_wday = decodeBCD(readMCP794xx(MCP_CLOCK, RTC_WEEKDAY) & 0x07) - 1;
   now.tm_mday = decodeBCD(readMCP794xx(MCP_CLOCK, RTC_DATE) & 0x3F) - 1;
   now.tm_mon  = decodeBCD(readMCP794xx(MCP_CLOCK, RTC_MONTH) & 0x1F) - 1;
   now.tm_year = decodeBCD(readMCP794xx(MCP_CLOCK, RTC_YEAR) & 0xFF) + YEAR_OFFSET;
   return &now;
}

/*
writeRTC(struct_tm *)

   DESCRIPTION:   writes the current time to the RTC, ensuring that the clock is running
   PARAMETERS:    struct_tm *          - pointer to the current date and time
   RETURNS:       none
*/
void writeRTC(struct_tm * curr) {
   writeMCP794xx(MCP_CLOCK, RTC_SECOND, encodeBCD(curr->tm_sec) | 0x80);
   writeMCP794xx(MCP_CLOCK, RTC_MINUTE, encodeBCD(curr->tm_min));
   writeMCP794xx(MCP_CLOCK, RTC_HOUR, encodeBCD(curr->tm_hour));
   writeMCP794xx(MCP_CLOCK, RTC_WEEKDAY, encodeBCD(curr->tm_wday + 1));
   writeMCP794xx(MCP_CLOCK, RTC_DATE, encodeBCD(curr->tm_mday + 1));
   writeMCP794xx(MCP_CLOCK, RTC_MONTH, encodeBCD(curr->tm_mon + 1));
   writeMCP794xx(MCP_CLOCK, RTC_YEAR, encodeBCD(curr->tm_year - YEAR_OFFSET));
}

/*
isRTCrunning()

   DESCRIPTION:   checks if the RTC oscillator is running and stable
   PARAMETERS:    none
   RETURNS:       a bit indicating the status of the oscillator
*/
int1 isRTCrunning() {
   return ((readMCP794xx(MCP_CLOCK, RTC_WEEKDAY) & 0x20) == 0x20);
}

/*
hasRTCpowerFailed()

   DESCRIPTION:   checks if the RTC has experienced a power failure since the clock was last set
   PARAMETERS:    none
   RETURNS:       a bit indicationg the status of the RTC's power supply since the last clock set
*/
int1 hasRTCpowerFailed() {
   return ((readMCP794xx(MCP_CLOCK, RTC_WEEKDAY) & 0x10) == 0x10);
}

/*
setBackupSupply(int1)

   DESCRIPTION:   enables or disables the Vbat input to the RTC as well as the power fail switchover logic
   PARAMETERS:    int1                 - enables or disables the backup power system. One of ENABLE or DISABLE
   RETURNS:       none
*/
void setBackupSupply(int1 status) {
   writeMCP794xx(MCP_CLOCK, RTC_WEEKDAY, readMCP794xx(MCP_CLOCK, RTC_WEEKDAY) | (status << 3));
}

/*
setAlarm(MCP794xx_alarmOffset, MCP794xx_alarmMode, int1, MCP794xx_alarmMask, struct_tm *)

   DESCRIPTION:   enables or disables the specified alarm modue on the RTC, selects the mode and mask, and sets the alarm match date and time
   PARAMETERS:    MCP794xx_alarmOffset - selects which alarm is being modified. One of MCP_ALARM0 or MCP_ALARM1
                  MCP794xx_alarmMode   - selects the mode used to combine alarm signals. One of ALARM_AND or ALARM_NOR (eqivalent to ALARM_ACTLOW or ALARM_ACTHIGH)
                  int1                 - enables or disables the alarm. One of ENABLE or DISABLE
                  MCP794xx_alarmMask   - selects the registers used in the comparison to determine if an alarm has occurred. One of MATCH_SECOND, MATCH_MINUTE, MATCH_HOUR, MATCH_WEEKDAY, MATCH_DATE, or MATCH_ALL
                  struct_tm *          - the date and time to be stored in to the selected alarm's comparison registers
   RETURNS:       none
*/
void setAlarm(MCP794xx_alarmOffset alarm, MCP794xx_alarmMode mode, int1 status, MCP794xx_alarmMask match, struct_tm * curr) {
   if (curr != NULL) {
      writeMCP794xx(MCP_CLOCK, ALM0_SECOND + alarm, encodeBCD(curr->tm_sec));
      writeMCP794xx(MCP_CLOCK, ALM0_MINUTE + alarm, encodeBCD(curr->tm_min));
      writeMCP794xx(MCP_CLOCK, ALM0_HOUR + alarm, encodeBCD(curr->tm_hour));
      writeMCP794xx(MCP_CLOCK, ALM0_WEEKDAY + alarm, encodeBCD(curr->tm_wday + 1) | (match << 4) | mode);
      writeMCP794xx(MCP_CLOCK, ALM0_DATE + alarm, encodeBCD(curr->tm_mday + 1));
      writeMCP794xx(MCP_CLOCK, ALM0_MONTH + alarm, encodeBCD(curr->tm_mon + 1));
   } else {
      writeMCP794xx(MCP_CLOCK, ALM0_WEEKDAY + alarm, readMCP794xx(MCP_CLOCK, ALM0_WEEKDAY + alarm) & 0xF7);
   }
   writeMCP794xx(MCP_CLOCK, RTC_CONTROL, (readMCP794xx(MCP_CLOCK, RTC_CONTROL) & (0xCF | ((alarm == MCP_ALARM1) ? 0x10 : 0x20))) | (((alarm == MCP_ALARM0) ? 0x10 : 0x20) & status));
}

/*
checkAlarm(MCP794xx_alarmOffset)

   DESCRIPTION:   checks the status of the specified alarm
   PARAMETERS:    MCP794xx_alarmOffset - selects which alarm is being checked. One of MCP_ALARM0 or MCP_ALARM1
   RETURNS:       a bit indicating if the alarm is triggered
*/
int1 checkAlarm(MCP794xx_alarmOffset alarm) {
   return ((readMCP794xx(MCP_CLOCK, ALM0_WEEKDAY + alarm) & 0x08) == 0x08);
}

/*
clearAlarm(MCP794xx_alarmOffset, int1)

   DESCRIPTION:   clears the specified alarm, and optionally reenables it
   PARAMETERS:    MCP794xx_alarmOffset - selects which alarm is being cleared. One of MCP_ALARM0 or MCP_ALARM1
                  int1                 - re-enables or disables the alarm. One of ENABLE or DISABLE
   RETURNS:       none
*/
void clearAlarm(MCP794xx_alarmOffset alarm, int1 reenable) {
   writeMCP794xx(MCP_CLOCK, RTC_CONTROL, (readMCP794xx(MCP_CLOCK, RTC_CONTROL) & (0xCF | ((alarm == MCP_ALARM1) ? 0x10 : 0x20))) | (((alarm == MCP_ALARM0) ? 0x10 : 0x20) & reenable));
   writeMCP794xx(MCP_CLOCK, ALM0_WEEKDAY + alarm, (readMCP794xx(MCP_CLOCK, ALM0_WEEKDAY + alarm) & 0xF7));
}

/*
setOutput(int1, int1, MCP794xx_frequency)

   DESCRIPTION:   sets up the MFP output pin of the RTC
   PARAMETERS:    int1                 - selects the mode of the MFP output (0 - static, 1 - square wave)
                  int1                 - sets the static output level of the MFP (only valid for squareWave == 0)
                  MCP794xx_frequency   - selects the output frequency of the MFP (only valid for squareWave == 1). One of SQW_1HZ, SQW_4096HZ, SQW_8192HZ, or SQW_32768HZ
   RETURNS:       none
*/
void setOutput(int1 squareWave, int1 state, MCP794xx_frequency freq) {
   writeMCP794xx(MCP_CLOCK, RTC_CONTROL, (readMCP794xx(MCP_CLOCK, RTC_CONTROL) & 0x38) | (state << 7) | (squareWave << 6) | freq);
}

// -------- LOW LEVEL / HELPER FUNCTIONS --------

/*
decodeBCD(unsigned int8)

   DESCRIPTION:   takes a BCD integer and returns a standard integer
   PARAMETERS:    uint8                - the BCD number to convert
   RETURNS:       the converted integer value
*/
unsigned int8 decodeBCD(unsigned int8 BCD) {
   return (BCD & 0x0F) + (((BCD & 0xF0) >> 4) * 10);
}

/*
encodeBCD(unsigned int8)

   DESCRIPTION:   takes a standard integer and returns its BCD representation
   PARAMETERS:    uint8                - the integer to convert
   RETURNS:       the BCD representation of the value
*/
unsigned int8 encodeBCD(unsigned int8 num) {
   return (num % 10) + (((num / 10) & 0x0F) << 4);
}

/*
readMCP794xx(MCP794xx_block, uint8)

   DESCRIPTION:   reads a single byte from the MCP794xx
   PARAMETERS:    MCP794xx_block       - the block to read from. One of MCP_CLOCK, MCP_RAM, MCP_EEPROM, MCP_UID
                  uint8                - the address within the block
   RETURNS:       the byte stored at the specified address in the specified block
*/
unsigned int8 readMCP794xx(MCP794xx_block block, unsigned int8 address) {
   unsigned int8 result = 0, i2caddress = 0;
   
   switch (block) {
   case MCP_CLOCK:
      address &= 0x1F;
      i2caddress = 0xDE;
      break;
   case MCP_RAM:
      address &= 0x3F;
      address += 0x20;
      i2caddress = 0xDE;
      break;
   case MCP_EEPROM:
      address &= 0x7F;
      i2caddress = 0xAE;
      break;
   case MCP_UID:
      address &= 0x07;
      address += 0xF0;
      i2caddress = 0xAE;
      break;
   }
   
   return readI2C(i2caddress, address);
}

/*
writeMCP794xx(MCP794xx_block, uint8, uint8)

   DESCRIPTION:   writes a single byte from the MCP794xx
   PARAMETERS:    MCP794xx_block       - the block to write to. One of MCP_CLOCK, MCP_RAM, MCP_EEPROM, MCP_UID
                  uint8                - the address within the block
                  uint8                - the byte to be written
   RETURNS:       none
*/
void writeMCP794xx(MCP794xx_block block, unsigned int8 address, unsigned int8 data) {
   unsigned int8 i2caddress = 0;
   
   switch (block) {
   case MCP_CLOCK:
      address &= 0x1F;
      i2caddress = 0xDE;
      break;
   case MCP_RAM:
      address &= 0x3F;
      address += 0x20;
      i2caddress = 0xDE;
      break;
   case MCP_EEPROM:
      address &= 0x7F;
      i2caddress = 0xAE;
      break;
   case MCP_UID:
      address &= 0x07;
      address += 0xF0;
      i2caddress = 0xAE;
      unlockMCP794xx_EUIblock();
      break;
   }
   
   writeI2C(i2caddress, address, data);
}

/*
unlockMCP794xx_EUIblock()

   DESCRIPTION:   unlocks the UID block in the MCP794xx to enable writing
   PARAMETERS:    none
   RETURNS:       none
*/
void unlockMCP794xx_EUIblock() {
   writeI2C(0xDE, RTC_EEUNLOCK, 0x55);
   writeI2C(0xDE, RTC_EEUNLOCK, 0xAA);
}

/*
readI2C(uint8, uint8)

   DESCRIPTION:   reads a single byte from the I2C bus
   PARAMETERS:    uint8                - I2C address of the target device
                  uint8                - register address to read
   RETURNS:       the byte stored in the specified register
*/
unsigned int8 readI2C(unsigned int8 i2caddress, unsigned int8 regaddress) {
   i2c_start(I2C_STREAM);
   i2c_write(I2C_STREAM, i2caddress);
   i2c_write(I2C_STREAM, regaddress);
   i2c_start(I2C_STREAM);
   i2c_write(I2C_STREAM, i2caddress | 0x01);
   unsigned int8 result = i2c_read(I2C_STREAM, 0);
   i2c_stop(I2C_STREAM);
   return result;
}

/*
writeI2C(uint8, uint8, uint8)

   DESCRIPTION:   writes a single byte to the I2C bus
   PARAMETERS:    uint8                - I2C address of the target device
                  uint8                - register address to write
                  uint8                - the byte to store in the specified register
   RETURNS:       none
*/
void writeI2C(unsigned int8 i2caddress, unsigned int8 regaddress, unsigned int8 data) {
   i2c_start(I2C_STREAM);
   i2c_write(I2C_STREAM, i2caddress);
   i2c_write(I2C_STREAM, regaddress);
   i2c_write(I2C_STREAM, data);
   i2c_stop(I2C_STREAM);
}

// -------- TIME FUNCTIONS --------

time_t mktime(struct_tm * timeT) {
   time_t unixTime = 0;
   int1 leapYear = FALSE;
   unsigned int16 i = 0;
   
   if(timeT != NULL) {
      unixTime += timeT->tm_sec;
      unixTime += (unsigned int32)(timeT->tm_min) * 60;
      unixTime += (unsigned int32)(timeT->tm_hour) * 3600;
      
      leapYear = isLeapYear(timeT->tm_year);
      timeT->tm_mon %= 12;
      for(i = 1;i <= timeT->tm_mon;i++) { unixTime += (daysInMonth(i - 1, leapYear) * 86400); }
      
      timeT->tm_mday %= daysInMonth(timeT->tm_mon,leapYear);
      unixTime += (timeT->tm_mday) * 86400;
      
      if (leapYear) { timeT->tm_yday = (unixTime / 86400) % 366; }
      else          { timeT->tm_yday = (unixTime / 86400) % 365; }
      
      i = 70;
      if(timeT->tm_year - 70 >= 0) {
         while(i < (timeT->tm_year)) {
            leapYear = isLeapYear(i);
            if (leapYear) { unixTime += (31622400); } // seconds in 366 days
            else          { unixTime += (31536000); } // seconds in 365 days
            i++;
         }
      }
      timeT->tm_wday = ((unixTime / 86400) + 4) % 7;
   }

   return unixTime;
}

struct_tm * localtime(time_t * timer) {
   time_t timeCounter;
   int1 done = FALSE;
   int1 leapYear = FALSE; //1970 is not a leap year
   
   struct_tm * temp_tm = malloc(sizeof(struct_tm));
   
   if(timer != NULL)
   {
      timeCounter = *timer;
      temp_tm->tm_wday = ((timeCounter / 86400) + 4) % 7;//fill in the weekday
      temp_tm->tm_year = 70;//we are starting in 1970
      
      while(!done)
      {
         if ((timeCounter < (31622400)) && leapYear) // seconds in 366 days
         {
            temp_tm->tm_yday = (timeCounter / 86400);
            break;
         }
         else if (timeCounter < (31536000)) // seconds in 365 days
         {
            temp_tm->tm_yday = (timeCounter / 86400);
            break;
         }
         
         if (leapYear) { timeCounter -= 31622400; } // seconds in 366 days
         else          { timeCounter -= 31536000; } // seconds in 365 days
         temp_tm->tm_year++;
         leapYear = isLeapYear(temp_tm->tm_year);
      }
      
      temp_tm->tm_mon = 0;
      while(!done)
      {         
         if(timeCounter < daysInMonth(temp_tm->tm_mon,leapYear) * 86400) { break; }
         else if(timeCounter >= daysInMonth(temp_tm->tm_mon,leapYear) * 86400)
         {
            timeCounter -= daysInMonth(temp_tm->tm_mon,leapYear) * 86400;
            temp_tm->tm_mon++;
         }
      }  

      temp_tm->tm_mday = (timeCounter / (86400));
      timeCounter -= (temp_tm->tm_mday * (86400));
      
      temp_tm->tm_hour = (timeCounter / (3600));
      timeCounter -= ((unsigned int32)temp_tm->tm_hour) * 3600;
      
      temp_tm->tm_min = (timeCounter / 60);
      timeCounter -= (((unsigned int16)temp_tm->tm_min) * 60);
     
      temp_tm->tm_sec = timeCounter;
   }
   
   return temp_tm;
}

signed int32 difftime(time_t later, time_t earlier)
{
   return (later - earlier);
}

int1 isLeapYear(unsigned int16 year) {
   if( ((year + 1900) % 400 == 0) || 
       (((year + 1900) % 4 == 0) && ((year + 1900) % 100 != 0)) )
      return TRUE;
     
   return FALSE;
}

unsigned int8 daysInMonth(unsigned int8 month, int1 LeapYear) {
   switch(month)
   {
      case JANUARY:
      case MARCH:
      case MAY:
      case JULY:
      case AUGUST:
      case OCTOBER:
      case DECEMBER:
         return 31;

      case FEBRUARY:
         if(LeapYear)
            return 29;            
         return 28;

      case APRIL:
      case JUNE:
      case SEPTEMBER:
      case NOVEMBER:
         return 30;
   }
}
