// TODO: 
// Oscillator frequency calibration (coarse/fine)
// Power down/up log (reading/clearing)

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
unsigned int8 decodeBCD(unsigned int8);
unsigned int8 encodeBCD(unsigned int8);
int1 isLeapYear(unsigned int16);
unsigned int8 daysInMonth(unsigned int8, int1);
// PRIVATE PROTOTYPES - END

time_t time() {
   return mktime(readRTC());
}

void setTime(struct_tm * nTime) {
   mktime(nTime);
   writeRTC(nTime);
}

void setTimeEpoch(time_t sTime) {
   struct_tm * temp = localtime(&sTime);
   setTime(temp);
   free(temp);
}

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

void writeRTC(struct_tm * curr) {
   writeMCP794xx(MCP_CLOCK, RTC_SECOND, encodeBCD(curr->tm_sec) | 0x80);
   writeMCP794xx(MCP_CLOCK, RTC_MINUTE, encodeBCD(curr->tm_min));
   writeMCP794xx(MCP_CLOCK, RTC_HOUR, encodeBCD(curr->tm_hour));
   writeMCP794xx(MCP_CLOCK, RTC_WEEKDAY, encodeBCD(curr->tm_wday + 1));
   writeMCP794xx(MCP_CLOCK, RTC_DATE, encodeBCD(curr->tm_mday + 1));
   writeMCP794xx(MCP_CLOCK, RTC_MONTH, encodeBCD(curr->tm_mon + 1));
   writeMCP794xx(MCP_CLOCK, RTC_YEAR, encodeBCD(curr->tm_year - YEAR_OFFSET));
}

int1 isRTCrunning() {
   return ((readMCP794xx(MCP_CLOCK, RTC_WEEKDAY) & 0x20) == 0x20);
}

int1 hasRTCpowerFailed() {
   return ((readMCP794xx(MCP_CLOCK, RTC_WEEKDAY) & 0x10) == 0x10);
}

void setBackupSupply(int8 status) {
   writeMCP794xx(MCP_CLOCK, RTC_WEEKDAY, readMCP794xx(MCP_CLOCK, RTC_WEEKDAY) | (status << 3));
}

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

int1 checkAlarm(MCP794xx_alarmOffset alarm) {
   return ((readMCP794xx(MCP_CLOCK, ALM0_WEEKDAY + alarm) & 0x08) == 0x08);
}

void clearAlarm(MCP794xx_alarmOffset alarm, int1 reenable) {
   writeMCP794xx(MCP_CLOCK, RTC_CONTROL, (readMCP794xx(MCP_CLOCK, RTC_CONTROL) & (0xCF | ((alarm == MCP_ALARM1) ? 0x10 : 0x20))) | (((alarm == MCP_ALARM0) ? 0x10 : 0x20) & reenable));
   writeMCP794xx(MCP_CLOCK, ALM0_WEEKDAY + alarm, (readMCP794xx(MCP_CLOCK, ALM0_WEEKDAY + alarm) & 0xF7));
}

void setOutput(int1 squareWave, int1 state, MCP794xx_frequency freq) {
   writeMCP794xx(MCP_CLOCK, RTC_CONTROL, (readMCP794xx(MCP_CLOCK, RTC_CONTROL) & 0x38) | (state << 7) | (squareWave << 6) | freq);
}

// LOW LEVEL HELPER FUNCTIONS

unsigned int8 decodeBCD(unsigned int8 BCD) {
   return (BCD & 0x0F) + (((BCD & 0xF0) >> 4) * 10);
}

unsigned int8 encodeBCD(unsigned int8 num) {
   return (num % 10) + (((num / 10) & 0x0F) << 4);
}

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
   
   i2c_start(I2C_STREAM);
   i2c_write(I2C_STREAM, i2caddress);
   i2c_write(I2C_STREAM, address);
   i2c_start(I2C_STREAM);
   i2c_write(I2C_STREAM, i2caddress | 0x01);
   result = i2c_read(I2C_STREAM, 0);
   i2c_stop(I2C_STREAM);
   
   return result;
}

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
   
   i2c_start(I2C_STREAM);
   i2c_write(I2C_STREAM, i2caddress);
   i2c_write(I2C_STREAM, address);
   i2c_write(I2C_STREAM, data);
   i2c_stop(I2C_STREAM);
}

void unlockMCP794xx_EUIblock() {
   i2c_start(I2C_STREAM);
   i2c_write(I2C_STREAM, 0xDE);
   i2c_write(I2C_STREAM, RTC_EEUNLOCK);
   i2c_write(I2C_STREAM, 0x55);
   i2c_stop(I2C_STREAM);
   i2c_start(I2C_STREAM);
   i2c_write(I2C_STREAM, 0xDE);
   i2c_write(I2C_STREAM, RTC_EEUNLOCK);
   i2c_write(I2C_STREAM, 0xAA);
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
