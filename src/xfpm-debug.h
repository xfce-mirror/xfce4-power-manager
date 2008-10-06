#ifndef __XFPM_DEBUG_H
#define __XFPM_DEBUG_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>

#ifdef DEBUG 

#define XFPM_DEBUG(...)              G_STMT_START{                          \
    fprintf(stderr, "TRACE[%s:%d] %s(): ",__FILE__,__LINE__,__func__);      \
    fprintf(stderr, __VA_ARGS__);                                           \
}G_STMT_END

#else

#define XFPM_DEBUG(...)

#endif

#endif  /*__XFPM_DEBUG_H */
