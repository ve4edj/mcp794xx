#ifndef _MCP794xx_H
#define _MCP794xx_H

#include <stdlibm.h>

/* Types */
typedef signed int32 time_t;
//typedef unsigned int32 clock_t;

/* Enumerations */
typedef enum
{
   SUNDAY = 0,
   MONDAY,
   TUESDAY,
   WEDNESDAY,
   THURSDAY,
   FRIDAY,
   SATURDAY   
}  Weekday;

typedef enum
{
   JANUARY = 0,
   FEBRUARY,
   MARCH,
   APRIL,
   MAY,
   JUNE,
   JULY,
   AUGUST,
   SEPTEMBER,
   OCTOBER,
   NOVEMBER,
   DECEMBER
}  Month;

typedef struct
{
   unsigned int8 tm_sec;   // seconds after the minute (0-59)
   unsigned int8 tm_min;   // minutes after the hour (0-59)
   unsigned int8 tm_hour;  // hours since midnight (0-23)
   unsigned int8 tm_mday;  // day of the month (0-30)
   Month tm_mon;           // month of the year (0-11)
   unsigned int16 tm_year; // years since 1900
   Weekday tm_wday;        // day of the week (0-6) (Sunday=0)
   unsigned int16 tm_yday; // day of the year (0-365)
} struct_tm;

typedef enum {
   MATCH_SECOND = 0x00,
   MATCH_MINUTE,
   MATCH_HOUR,
   MATCH_WEEKDAY,
   MATCH_DATE,
   MATCH_ALL = 0x07
} MCP794xx_alarmMask;

typedef enum {
   MCP_ALARM0 = 0x00,
   MCP_ALARM1 = 0x07
} MCP794xx_alarmOffset;

typedef enum {
   ALARM_AND = 0x00,
   ALARM_NOR = 0x80,
   ALARM_ACTLOW = 0x00,
   ALARM_ACTHIGH = 0x80
} MCP794xx_alarmMode;

typedef enum {
   SQW_1HZ = 0x00,
   SQW_4096HZ,
   SQW_8192HZ,
   SQW_32768HZ
} MCP794xx_frequency;

/* Functions */
//clock_t clock(void);
time_t time(time_t * timer);
signed int32 difftime(time_t later, time_t earlier);
time_t mktime(struct_tm * timeT);
struct_tm * localtime(time_t * timer);

//char * asctime(struct_tm * timeptr, char *szTime);
//char * ctime(time_t * timer, char *szTime);

void setTime(struct_tm * nTime);
void setTimeEpoch(time_t sTime);
//void getTime(struct_tm * pRetTm);
//void timeInit(void);

struct_tm * readRTC(void);
void writeRTC(struct_tm *);
int1 isRTCrunning(void);
int1 hasRTCpowerFailed(void);
void setAlarm(MCP794xx_alarmOffset, MCP794xx_alarmMode, int1, MCP794xx_alarmMask, struct_tm *);
int1 checkAlarm(MCP794xx_alarmOffset);
void clearAlarm(MCP794xx_alarmOffset, int1);
void setOutput(int1, int1, MCP794xx_frequency);

#define YEAR_OFFSET  100

#define ENABLE  1
#define DISABLE 0

#include "MCP794xx.c"

#endif
