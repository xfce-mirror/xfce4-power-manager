#ifndef __XFPM_ENUMS_H
#define __XFPM_ENUMS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef enum
{
    XFPM_DO_NOTHING,
    XFPM_DO_SUSPEND,
    XFPM_DO_HIBERNATE,
    XFPM_DO_SHUTDOWN
    
} XfpmActionRequest;

/* Icon tray */
typedef enum
{
    ALWAYS,
    PRESENT,
    CHARGING_OR_DISCHARGING
} XfpmShowIcon;

/* Battery State */
typedef enum
{
    NOT_PRESENT,
    LOW,
    CRITICAL,
    DISCHARGING,
    CHARGING,
    NOT_FULL,
    FULL
} XfpmBatteryState;

/* Battery type */
typedef enum
{
    UNKNOWN,
    PRIMARY,
    UPS,
    KEYBOARD,
    MOUSE
    
} XfpmBatteryType;

/* CPU Freq Linux governors */
typedef enum
{
	CPU_FREQ_CANNOT_BE_USED = (1<<0),
    POWERSAVE               = (1<<1),
    ONDEMAND                = (1<<2),
    PERFORMANCE             = (1<<3),
    CONSERVATIVE            = (1<<4),
    USERSPACE               = (1<<5)
    
} XfpmCpuGovernor;

#endif
