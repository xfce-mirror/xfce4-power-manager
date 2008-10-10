#ifndef __XFPM_ENUMS_H
#define __XFPM_ENUMS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Action to taken when battery charge level is critical */
typedef enum
{
    NOTHING,
    SHUTDOWN,
    HIBERNATE
} XfpmCriticalAction;

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
    POWERSAVE,
    ONDEMAND,
    PERFORMANCE,
    CONSERVATIVE,
    USERSPACE
    
} XfpmCpuGovernor;


/* button switch control */
typedef enum
{
    BUTTON_DO_NOTHING,
    BUTTON_DO_SUSPEND,
    BUTTON_DO_HIBERNATE,
    BUTTON_DO_SHUTDOWN

} XfpmButtonAction;

#endif
