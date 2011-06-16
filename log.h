/*
 * D-Bus C Bindings library
 *
 * Copyright 2011 S.I.S.E. S.A
 * Author: Michel Lafon-Puyo <mlafon-puyo@sise.fr>
 *
 */

#ifndef __LIBCDBUS_LOG_H
#define __LIBCDBUS_LOG_H

#include <syslog.h>
#include "config.h"

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#endif

#define LOG(level, args...)			\
	do {					\
		if (level <= LOG_LEVEL) \
			printf( "libcdbus: " args);	\
	}while(0)				\

#endif
