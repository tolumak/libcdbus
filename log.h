/*
 * Log and traces definitions for S.I.S.E applications
 *
 * Copyright 2011-2014 S.I.S.E. S.A.
 * Author: Michel Lafon-Puyo <michel.lafonpuyo@gmail.com>
 *
 * This file is part of libcdbus
 *
 * libcdbus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include "config.h"

#define	LOG_EMERG	0	/* system is unusable */
#define	LOG_ALERT	1	/* action must be taken immediately */
#define	LOG_CRIT	2	/* critical conditions */
#define	LOG_ERR		3	/* error conditions */
#define	LOG_WARNING	4	/* warning conditions */
#define	LOG_NOTICE	5	/* normal but significant condition */
#define	LOG_INFO	6	/* informational */
#define	LOG_DEBUG	7	/* debug-level messages */

#define	LOG_DISABLED	255	/* messages disabled */

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#endif

#ifdef _NEWLIB_VERSION
#define logprintf iprintf
#else
#define logprintf printf
#endif

#define LOG(level, args...)			\
	do {					\
		if (level <= LOG_LEVEL) \
			logprintf( LOG_APP_NAME": " args);	\
	}while(0)
#define LOG_BARE(level, args...)			\
	do {					\
		if (level <= LOG_LEVEL) \
			logprintf(args);	\
	}while(0)

#endif
