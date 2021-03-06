// @@@LICENSE
//
//      Copyright (c) 2007-2012 Hewlett-Packard Development Company, L.P.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// LICENSE@@@


// get GNU extensions from dlfcn.h (dladdr)
#define _GNU_SOURCE

#include "PmLogLib.h"
#include "PmLogLibPrv.h"

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/syslog.h>
#include <sys/shm.h>
#include <unistd.h>


// take advantage of glibc
extern const char*	__progname;


/***********************************************************************
 * gettid
 ***********************************************************************/
pid_t gettid(void)
{
	return (pid_t) syscall(__NR_gettid);
}


//#######################################################################


/***********************************************************************
 * COMPONENT_PREFIX
 ***********************************************************************/
#define COMPONENT_PREFIX	"PmLogLib: "


// don't check in with this on!
//#define DEBUG_ENABLED


#ifdef DEBUG_ENABLED

	/***********************************************************************
	 * DbgPrint
	 ***********************************************************************/
	#define DbgPrint(...) \
		{														\
			fprintf(stdout, COMPONENT_PREFIX __VA_ARGS__);		\
			syslog(LOG_DEBUG, COMPONENT_PREFIX __VA_ARGS__);	\
		}


	/***********************************************************************
	 * ErrPrint
	 ***********************************************************************/
	#define ErrPrint(...) \
		{														\
			fprintf(stderr, COMPONENT_PREFIX __VA_ARGS__);		\
			syslog(LOG_ERR, COMPONENT_PREFIX __VA_ARGS__);		\
		}

#else

	/***********************************************************************
	 * DbgPrint
	 ***********************************************************************/
	#define DbgPrint(...)


	/***********************************************************************
	 * ErrPrint
	 ***********************************************************************/
	#define ErrPrint(...) \
		{														\
			syslog(LOG_ERR, COMPONENT_PREFIX __VA_ARGS__);		\
		}

#endif // DEBUG_ENABLED


//#######################################################################


/***********************************************************************
 * DEBUG_LOGGING
 *
 * Because the PmLog global initialization will occur on first use that
 * means it may occur before PmLogDaemon is running.  In which case we
 * can't do logging or print debugging information as it will get
 * dropped.  To work around that we can direct our local logs to a
 * separate file.
 * ?? Later, consider having this integrated into the main bottleneck,
 * e.g. PrvLogWrite.  We can have a field in the shared memory that
 * is a flag set by PmLogDaemon when it is running.  If logging is done
 * before that we can manually append to either a separate or the main
 * log file.  That way we'll be sure not to drop any messages during
 * boot or if PmLogDaemon doesn't run for some reason.
 * To be revisited after the OE build switch is complete.
 ***********************************************************************/

// don't check in with this on!
//#define DEBUG_LOGGING


#ifdef DEBUG_LOGGING


/*********************************************************************/
/* PrintAppendToFile */
/**
@brief  Logs the specified formatted text to the specified context.
**********************************************************************/
static void PrintAppendToFile(const char* filePath, const char* fmt, ...)
{
	va_list args;
	FILE*	f;

	va_start(args, fmt);

	f = fopen(filePath, "a");
	if (f != NULL)
	{
		fprintf(f, "%s: ", __progname);

		vfprintf(f, fmt, args);

		(void) fclose(f);
	}

	va_end(args);
}


/***********************************************************************
 * DbgPrint
 ***********************************************************************/
#undef DbgPrint
#define DbgPrint(...) \
	{															\
		const char* path = "/var/log/pmlog.log";				\
		PrintAppendToFile(path, COMPONENT_PREFIX __VA_ARGS__);	\
	}

/***********************************************************************
 * ErrPrint
 ***********************************************************************/
#undef ErrPrint
#define ErrPrint	DbgPrint


#endif // DEBUG_LOGGING


//#######################################################################


/***********************************************************************
 * mystrcpy
 *
 * Easy to use wrapper for strcpy to make is safe against buffer
 * overflows and to report any truncations.
 ***********************************************************************/
static void mystrcpy(char* dst, size_t dstSize, const char* src)
{
	size_t	srcLen;

	if (dst == NULL)
	{
		ErrPrint("mystrcpy null dst\n");
		return;
	}

	if (dstSize < 1)
	{
		ErrPrint("mystrcpy invalid dst size\n");
		return;
	}

	dst[ 0 ] = 0;

	if (src == NULL)
	{
		ErrPrint("mystrcpy null src\n");
		return;
	}

	srcLen = strlen(src);
	if (srcLen >= dstSize)
	{
		ErrPrint("mystrcpy buffer overflow\n");
		srcLen = dstSize - 1;
	}

	memcpy(dst, src, srcLen);
	dst[ srcLen ] = 0;
}


#if 0
/***********************************************************************
 * mystrcat
 *
 * Easy to use wrapper for strcat to make is safe against buffer
 * overflows and to report any truncations.
 ***********************************************************************/
static void mystrcat(char* dst, size_t dstSize, const char* src)
{
	size_t	dstLen;
	size_t	srcLen;
	size_t	maxLen;

	if (dst == NULL)
	{
		ErrPrint("mystrcat null dst\n");
		return;
	}

	if (dstSize < 1)
	{
		ErrPrint("mystrcat invalid dst size\n");
		return;
	}

	dstLen = strlen(dst);
	if (dstLen >= dstSize)
	{
		ErrPrint("mystrcat invalid dst len\n");
		return;
	}

	if (src == NULL)
	{
		ErrPrint("mystrcat null src\n");
		return;
	}

	srcLen = strlen(src);
	if (srcLen < 1)
	{
		// empty string, do nothing
		return;
	}

	maxLen = (dstSize - 1) - dstLen;

	if (srcLen > maxLen)
	{
		ErrPrint("mystrcat buffer overflow\n");
		srcLen = maxLen;
	}

	if (srcLen > 0)
	{
		memcpy(dst + dstLen, src, srcLen);
		dst[ dstLen + srcLen ] = 0;
	}
}
#endif


/***********************************************************************
 * mysprintf
 *
 * Easy to use wrapper for sprintf to make is safe against buffer
 * overflows and to report any truncations.
 ***********************************************************************/
static void mysprintf(char* dst, size_t dstSize, const char* fmt, ...)
	__attribute__((format(printf, 3, 4)));


/***********************************************************************
 * mysprintf
 *
 * Easy to use wrapper for sprintf to make is safe against buffer
 * overflows and to report any truncations.
 ***********************************************************************/
static void mysprintf(char* dst, size_t dstSize, const char* fmt, ...)
{
	va_list 		args;
	int				n;

	if (dst == NULL)
	{
		ErrPrint("mysprintf null dst\n");
		return;
	}

	if (dstSize < 1)
	{
		ErrPrint("mysprintf invalid dst size\n");
		return;
	}

	dst[ 0 ] = 0;

	if (fmt == NULL)
	{
		ErrPrint("mysprintf null fmt\n");
		return;
	}

	va_start(args, fmt);

	n = vsnprintf(dst, dstSize, fmt, args);
	if (n >= dstSize)
	{
		ErrPrint("mysprintf buffer overflow\n");
		dst[ dstSize - 1 ] = 0;
	}

	va_end(args);
}


/***********************************************************************
 * ParseBool
 ***********************************************************************/
static bool ParseBool(const char* valStr, bool* bP,
	char* errMsg, size_t errMsgBuffSize)
{
	errMsg[ 0 ] = 0;

	if (strcasecmp(valStr, "false") == 0)
	{
		*bP = false;
		return true;
	}

	if (strcasecmp(valStr, "true") == 0)
	{
		*bP = true;
		return true;
	}

	mystrcpy(errMsg, errMsgBuffSize, "boolean value expected");
	return false;
}


/***********************************************************************
 * ParseKeyValue
 *
 * If the given string is of the form "KEY=VALUE" copy the given
 * key and value strings into the specified buffers and return true,
 * otherwise return false.
 * Key may not be empty string, but value may be.
 ***********************************************************************/
static bool ParseKeyValue(const char* arg, char* keyBuff, size_t keyBuffSize,
	char* valBuff, size_t valBuffSize)
{
	const char* sepStr;
	size_t		keyLen;
	const char* valStr;
	size_t		valLen;

	sepStr = strchr(arg, '=');
	if ((sepStr == NULL) || (sepStr <= arg))
		return false;

	keyLen = sepStr - arg;
	if (keyLen >= keyBuffSize)
		return false;

	memcpy(keyBuff, arg, keyLen);
	keyBuff[keyLen] = 0;

	valStr = sepStr + 1;
	valLen = strlen(valStr);
	if (valLen >= valBuffSize)
		return false;

	memcpy(valBuff, valStr, valLen);
	valBuff[valLen] = 0;

	return true;
}


//#######################################################################


/***********************************************************************
 * IntLabel
 *
 * Define a integer value => string label mapping.
 ***********************************************************************/
typedef struct
{
	const char* s;
	int			n;
}
IntLabel;


/***********************************************************************
 * PrvGetIntLabel
 *
 * Look up the string label for a given integer value from the given
 * mapping table.  Return NULL if not found.
 ***********************************************************************/
const char* PrvGetIntLabel(const IntLabel* labels, int n);


/***********************************************************************
 * PrvLabelToInt
 *
 * Look up the integer value matching a given string label from the
 * given mapping table.  Return NULL if not found.
 ***********************************************************************/
const int* PrvLabelToInt(const IntLabel* labels, const char* s);


//#######################################################################


/***********************************************************************
 * PrvGetIntLabel
 *
 * Look up the string label for a given integer value from the given
 * mapping table.  Return NULL if not found.
 ***********************************************************************/
const char* PrvGetIntLabel(const IntLabel* labels, int n)
{
	const IntLabel* p;

	for (p = labels; p->s != NULL; p++)
	{
		if (p->n == n)
		{
			return p->s;
		}
	}

	return NULL;
}


/***********************************************************************
 * PrvLabelToInt
 *
 * Look up the integer value matching a given string label from the
 * given mapping table.  Return NULL if not found.
 ***********************************************************************/
const int* PrvLabelToInt(const IntLabel* labels, const char* s)
{
	const IntLabel* p;

	for (p = labels; p->s != NULL; p++)
	{
		if (strcmp(p->s, s) == 0)
		{
			return &p->n;
		}
	}

	return NULL;
}


//#######################################################################


/***********************************************************************
 * kLogLevelLabels
 *
 * Define string labels for the supported logging levels.
 ***********************************************************************/
static const IntLabel kLogLevelLabels[] =
{
	// use the same labels as the syslog standards
	{ "none",		/* -1 */ kPmLogLevel_None },
	//-----------------------
	{ "emerg",		/*  0 */ kPmLogLevel_Emergency },
	{ "alert",		/*  1 */ kPmLogLevel_Alert     },
	{ "crit",		/*  2 */ kPmLogLevel_Critical  },
	{ "err",		/*  3 */ kPmLogLevel_Error     },
	{ "warning",	/*  4 */ kPmLogLevel_Warning   },
	{ "notice",		/*  5 */ kPmLogLevel_Notice    },
	{ "info",		/*  6 */ kPmLogLevel_Info      },
	{ "debug",		/*  7 */ kPmLogLevel_Debug     },
	//-----------------------
	{ NULL,			0 }
};


/***********************************************************************
 * PmLogLevelToString
 *
 * Given a numeric level value, returning the matching symbolic string.
 * -1 (kPmLogLevel_None)      => "none"
 *  0 (kPmLogLevel_Emergency) => "emerg"
 * etc.
 * Returns NULL if the level is not valid.
 ***********************************************************************/
const char* PmLogLevelToString(PmLogLevel level)
{
	const char*	s;

	s = PrvGetIntLabel(kLogLevelLabels, level);
	return s;
}


/***********************************************************************
 * PmLogStringToLevel
 *
 * Given a symbolic level string, returning the matching numeric value.
 * "none"  => -1 (kPmLogLevel_None)
 * "emerg" =>  0 (kPmLogLevel_Emergency)
 * etc.
 * Returns NULL if the level string is not mateched.
 ***********************************************************************/
const int* PmLogStringToLevel(const char* levelStr)
{
	const int*	nP;

	nP = PrvLabelToInt(kLogLevelLabels, levelStr);
	return nP;
}


//#######################################################################


/***********************************************************************
 * kLogFacilityLabels
 *
 * Define string labels for the supported logging facilities.
 ***********************************************************************/
static const IntLabel kLogFacilityLabels[] =
{
	//-----------------------
	{ "kern",		/*  (0<<3) */ LOG_KERN     },
	{ "user",		/*  (1<<3) */ LOG_USER     },
	{ "mail",		/*  (2<<3) */ LOG_MAIL     },
	{ "daemon",		/*  (3<<3) */ LOG_DAEMON   },
	{ "auth",		/*  (4<<3) */ LOG_AUTH     },
	{ "syslog",		/*  (5<<3) */ LOG_SYSLOG   },
	{ "lpr",		/*  (6<<3) */ LOG_LPR      },
	{ "news",		/*  (7<<3) */ LOG_NEWS     },
	{ "uucp",		/*  (8<<3) */ LOG_UUCP     },
	{ "cron",		/*  (9<<3) */ LOG_CRON     },
	{ "authpriv",	/* (10<<3) */ LOG_AUTHPRIV },
	{ "ftp",		/* (11<<3) */ LOG_FTP      },
	//-----------------------
	{ "local0",		/* (16<<3) */ LOG_LOCAL0   },
	{ "local1",		/* (17<<3) */ LOG_LOCAL1   },
	{ "local2",		/* (18<<3) */ LOG_LOCAL2   },
	{ "local3",		/* (19<<3) */ LOG_LOCAL3   },
	{ "local4",		/* (20<<3) */ LOG_LOCAL4   },
	{ "local5",		/* (21<<3) */ LOG_LOCAL5   },
	{ "local6",		/* (22<<3) */ LOG_LOCAL6   },
	{ "local7",		/* (23<<3) */ LOG_LOCAL7   },
	//-----------------------
	{ NULL,			0 }
};


/***********************************************************************
 * PmLogFacilityToString
 *
 * Given a numeric facility value, returning the matching symbolic string.
 * LOG_KERN => "kern"
 * LOG_USER => "user"
 * etc.
 * Returns NULL if the facility is not valid.
 ***********************************************************************/
const char* PmLogFacilityToString(int facility)
{
	const char*	s;

	s = PrvGetIntLabel(kLogFacilityLabels, facility);
	return s;
}


/***********************************************************************
 * PmLogStringToFacility
 *
 * Given a symbolic facility string, returning the matching numeric value.
 * "kern" => LOG_KERN
 * "user" =>  LOG_USER
 * etc.
 * Returns NULL if the facility string is not matched.
 ***********************************************************************/
const int* PmLogStringToFacility(const char* facilityStr)
{
	const int* nP;

	nP = PrvLabelToInt(kLogFacilityLabels, facilityStr);
	return nP;
}


//#######################################################################


/***********************************************************************
 * PrvGetLevelStr
 *
 *
 * Given a numeric level value, returning the matching symbolic string.
 * -1 (kPmLogLevel_None)      => "none"
 *  0 (kPmLogLevel_Emergency) => "emerg"
 * etc.
 * Return "?" if not recognized (should not occur).
 ***********************************************************************/
static const char* PrvGetLevelStr(int level)
{
	const char* s;

	s = PmLogLevelToString(level);
	if (s != NULL)
	{
		return s;
	}

	return "?";
}


/***********************************************************************
 * PrvParseConfigLevel
 *
 * "none" => -1 (kPmLogLevel_None)
 * "err"  => LOG_ERR (kPmLogLevel_Error),
 * etc.
 * Return true if parsed OK, else false.
 ***********************************************************************/
static bool PrvParseConfigLevel(const char* s, int* levelP)
{
	const int* nP;

	nP = PmLogStringToLevel(s);
	if (nP != NULL)
	{
		*levelP = *nP;
		return true;
	}

	*levelP = -1;
	return false;
}


/*********************************************************************/
/* PrvInitContext */
/**
@brief  Read the configuration file for pre-defined contexts and
		context levels.
**********************************************************************/
static bool PrvInitContext(const char* contextName, const char* levelStr,
	char* errMsg, size_t errMsgBuffSize)
{
	PmLogErr		logErr;
	PmLogContext	context;
	int				level;

	errMsg[ 0 ] = 0;

	DbgPrint("defining %s => %s\n", contextName, levelStr);

	level = kPmLogLevel_Debug;
	if (!PrvParseConfigLevel(levelStr, &level))
	{
		mystrcpy(errMsg, errMsgBuffSize, "Failed to parse level");
		return false;
	}

	context = NULL;
	logErr = PmLogGetContext(contextName, &context);
	if (logErr != kPmLogErr_None)
	{
		mysprintf(errMsg, errMsgBuffSize, "Error getting context: %s",
			PmLogGetErrDbgString(logErr));
		return false;
	}

	logErr = PmLogSetContextLevel(context, level);
	if (logErr != kPmLogErr_None)
	{
		mysprintf(errMsg, errMsgBuffSize, "Error setting context level: %s",
			PmLogGetErrDbgString(logErr));
		return false;
	}

	return true;
}


/*********************************************************************/
/* PrvInitContexts */
/**
@brief  Read the configuration file for pre-defined contexts and
		context levels.
**********************************************************************/
static bool PrvInitContexts(void)
{
	const char* kConfigFile = "/etc/PmLogContexts.conf";
	const char* kContextsSection = "[Contexts]";

	FILE*	f;
	int		err;
	int		lineNum;
	char	line[ 512 ];
	size_t	lineLen;
	bool	inContextsSection;
	bool	gotContextsSection;
	char	key[ 256 ];
	char	val[ 256 ];
	char	errMsg[ 256 ];

	DbgPrint("reading PmLogContexts.conf\n");

	f = fopen(kConfigFile, "r");
	if (f == NULL)
	{
		err = errno;
		ErrPrint("Config error on %s: Failed open: %s\n", kConfigFile,
			strerror(err));
		return false;
	}

	inContextsSection = false;
	gotContextsSection = false;

	lineNum = 0;
	for (;;)
	{
		if (fgets(line, sizeof(line), f) == NULL)
		{
			break;
		}

		lineNum++;
		lineLen = strlen(line);

		// trim trailing whitespace
		while ((lineLen > 0) && isspace(line[ lineLen - 1 ]))
		{
			line[ lineLen - 1 ] = 0;
			lineLen--;
		}

		// ignore comment lines
		if (line[ 0 ] == '#')
		{
			continue;
		}

		// a blank line ends the section
		if (line[ 0 ] == 0)
		{
			inContextsSection = false;
			continue;
		}

		if (inContextsSection)
		{
			if (!ParseKeyValue(line, key, sizeof(key),
				val, sizeof(val)))
			{
				ErrPrint("Config error on %s: Line %d: Failed to parse line\n",
					kConfigFile, lineNum);
				continue;
			}

			if (!PrvInitContext(key, val, errMsg, sizeof(errMsg)))
			{
				ErrPrint("Config error on %s: Line %d: Failed to parse: %s\n",
					kConfigFile, lineNum, errMsg);
				continue;
			}
		}
		else
		{
			if (strcmp(line, kContextsSection) == 0)
			{
				gotContextsSection = true;
				inContextsSection = true;
				continue;
			}

			//ErrPrint("Config error on %s: Line %d: Unrecognized line\n",
			//	kConfigFile, lineNum);
			continue;
		}
	}

	if (!gotContextsSection)
	{
		ErrPrint("Config error on %s: Section '%s' not defined\n",
			kConfigFile, kContextsSection);
	}

	(void) fclose(f);

	//---------------------------------------------------

	return true;
}


//#######################################################################


/*********************************************************************/
/* PrvSetFlag */
/**
@brief  Set or clear the flag bit as indicated.
**********************************************************************/
static void PrvSetFlag(int* flagsP, int flagValue, bool set)
{
	if (set)
	{
		*flagsP = *flagsP | flagValue;
	}
	else
	{
		*flagsP = *flagsP & (~flagValue);
	}
}


/*********************************************************************/
/* PrvReadConfigKey */
/**
@brief  Read the configuration file config section.
**********************************************************************/
static bool PrvReadConfigKey(PmLogGlobals* gGlobalsP,
	const char* keyStr, const char* valStr,	char* errMsg, size_t errMsgBuffSize)
{
	int*	flagsP = &gGlobalsP->flags;

	errMsg[0] = 0;

	//------------------------------------------------------
	if (strcmp(keyStr, "LogProcessIds") == 0)
	{
		bool bLogProcessIds = false;
		if (!ParseBool(valStr, &bLogProcessIds, errMsg, errMsgBuffSize))
		{
			return false;
		}

		PrvSetFlag(flagsP, kPmLogGlobalsFlag_LogProcessIds, bLogProcessIds);
		return true;
	}
	//------------------------------------------------------
	if (strcmp(keyStr, "LogThreadIds") == 0)
	{
		bool bLogThreadIds = false;
		if (!ParseBool(valStr, &bLogThreadIds, errMsg, errMsgBuffSize))
		{
			return false;
		}

		PrvSetFlag(flagsP, kPmLogGlobalsFlag_LogThreadIds, bLogThreadIds);
		return true;
	}
	//------------------------------------------------------
	if (strcmp(keyStr, "LogToConsole") == 0)
	{
		bool bLogConsole = false;
		if (!ParseBool(valStr, &bLogConsole, errMsg, errMsgBuffSize))
		{
			return false;
		}

		PrvSetFlag(flagsP, kPmLogGlobalsFlag_LogToConsole, bLogConsole);
		return true;
	}
	//------------------------------------------------------

	mysprintf(errMsg, errMsgBuffSize, "key '%s' not recognized", keyStr);
	return false;
}


/*********************************************************************/
/* PrvReadGlobalConfig */
/**
@brief  Read the configuration file.
**********************************************************************/
static bool PrvReadGlobalConfig(PmLogGlobals* gGlobalsP)
{
	const char* kConfigFile = "/etc/PmLogContexts.conf";
	const char* kConfigSection = "[Config]";

	FILE*	f;
	int		err;
	int		lineNum;
	char	line[ 512 ];
	size_t	lineLen;
	bool	inConfigSection;
	bool	gotConfigSection;
	char	key[ 256 ];
	char	val[ 256 ];
	char	errMsg[ 256 ];

	DbgPrint("reading global config PmLogContexts.conf\n");

	gGlobalsP->flags = 0;

	f = fopen(kConfigFile, "r");
	if (f == NULL)
	{
		err = errno;
		ErrPrint("Config error on %s: Failed open: %s\n", kConfigFile,
			strerror(err));
		return false;
	}

	inConfigSection = false;
	gotConfigSection = false;

	lineNum = 0;
	for (;;)
	{
		if (fgets(line, sizeof(line), f) == NULL)
		{
			break;
		}

		lineNum++;
		lineLen = strlen(line);

		// trim trailing whitespace
		while ((lineLen > 0) && isspace(line[ lineLen - 1 ]))
		{
			line[ lineLen - 1 ] = 0;
			lineLen--;
		}

		// ignore comment lines
		if (line[ 0 ] == '#')
		{
			continue;
		}

		// a blank line ends the section
		if (line[ 0 ] == 0)
		{
			inConfigSection = false;
			continue;
		}

		if (inConfigSection)
		{
			if (!ParseKeyValue(line, key, sizeof(key),
				val, sizeof(val)))
			{
				ErrPrint("Config error on %s: Line %d: Failed to parse line\n",
					kConfigFile, lineNum);
				continue;
			}

			if (!PrvReadConfigKey(gGlobalsP, key, val, errMsg, sizeof(errMsg)))
			{
				ErrPrint("Config error on %s: Line %d: Failed to parse: %s\n",
					kConfigFile, lineNum, errMsg);
				continue;
			}
		}
		else
		{
			if (strcmp(line, kConfigSection) == 0)
			{
				gotConfigSection = true;
				inConfigSection = true;
				continue;
			}

			//ErrPrint("Config error on %s: Line %d: Unrecognized line\n",
			//	kConfigFile, lineNum);
			continue;
		}
	}

	if (!gotConfigSection)
	{
		//ErrPrint("Config error on %s: Section '%s' not defined\n",
		//	kConfigFile, kConfigSection);
	}

	(void) fclose(f);

	//---------------------------------------------------

	return true;
}


//#######################################################################


// shared memory segment
static sem_t*			gSem			= SEM_FAILED;
static int				gShmId			= -1;
static uint8_t*			gShmData		= NULL;

// typed pointers to shared memory segment
static PmLogGlobals*	gGlobalsP		= NULL;
static PmLogContext_*	gGlobalContextP	= NULL;


/*********************************************************************/
/* kHexChars */
/**
@brief  Lookup for hex nybble output.
**********************************************************************/
static const char kHexChars[16] =
{
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};


/*********************************************************************/
/* init_function */
/**
@brief  Library constructor executes automatically in the loading
	process before any other library API is called.
**********************************************************************/
static void __attribute ((constructor)) init_function(void)
{
	const char* kPmLogLibSoFilePath = "/usr/lib/libPmLogLib.so";

	int			err;
	sem_t*		sem;
	key_t		key;
	int			shmid;
	char*		data;
	size_t		shmSize;
	bool		needInit;
	Dl_info		dlInfo;
	int			result;
	const char*	libFilePath;

	// get/create the PmLogLib semaphore

	// note: named semaphores are created in a virtual file system,
	// under /dev/shm, with names of the form sem.<name>
	DbgPrint("Opening sem\n");

	sem = sem_open("PmLogLib", O_CREAT, 0666, 1);
	if (sem == SEM_FAILED)
	{
		err = errno;
		ErrPrint("sem_open error: %s\n", strerror(err));
		return;
	}

	gSem = sem;

	//------------------------------------------------------------

	// determine this library's file path dynamically
	libFilePath = NULL;

	memset(&dlInfo, 0, sizeof(dlInfo));
	result = dladdr(init_function, &dlInfo);
	if (result)
	{
		libFilePath = dlInfo.dli_fname;
		DbgPrint("libFilePath: %s\n", libFilePath);
	}
	else
	{
		err = errno;
		ErrPrint("dladdr err: %s\n", strerror(err));
	}

	// if lookup failed for some reason, fall back to expected
	if (libFilePath == NULL)
	{
		libFilePath = kPmLogLibSoFilePath;
	}

	//------------------------------------------------------------

	DbgPrint("getting shm key\n");

	key = ftok(libFilePath, 'A');
	if (key == -1)
	{
		err = errno;
		ErrPrint("ftok error: %s\n", strerror(err));
		return;
	}

	//------------------------------------------------------------

	// lock the globals
	PmLogPrvLock();

	shmSize = sizeof(PmLogGlobals);
	DbgPrint("Getting shm size=%u\n", shmSize);

	// if shm is guaranteed initialized to 0, we can use
	// that to tell whether it needs initializing or not.
	// Otherwise, we can do a shmget without the IPC_CREAT,
	// and if returns errno == ENOENT we know it needs creation
	// and initialization.

	shmid = shmget(key, shmSize, 0666 | IPC_CREAT);
	if (shmid == -1)
	{
		err = errno;
		ErrPrint("shmget error: %s\n", strerror(err));
		return;
	}

	gShmId = shmid;

	data = (char*) shmat(shmid, NULL, 0 /* SHM_RDONLY */);
	if (data == (char*) -1)
	{
		err = errno;
		ErrPrint("shmat error: %s\n", strerror(err));
		return;
	}

	gShmData = (uint8_t*) data;

	// same as gShmData, but typecast for use
	gGlobalsP = (PmLogGlobals*) gShmData;
	gGlobalContextP = &gGlobalsP->globalContext;

	needInit = false;

	// use header as initialization flag
	//---------------------------------------------------------------
	if (gGlobalsP->signature == 0)
	{
		DbgPrint("initializing shared mem\n");

		gGlobalsP->signature = PMLOG_SIGNATURE;

		gGlobalsP->maxUserContexts = sizeof(gGlobalsP->userContexts) /
			sizeof(PmLogContext_);

		gGlobalsP->numUserContexts = 0;

		gGlobalsP->flags = 0;

		gGlobalsP->consoleConf.stdErrMinLevel = kPmLogLevel_Emergency;
		gGlobalsP->consoleConf.stdErrMaxLevel = kPmLogLevel_Error;
		gGlobalsP->consoleConf.stdOutMinLevel = kPmLogLevel_Warning;
		gGlobalsP->consoleConf.stdOutMaxLevel = kPmLogLevel_Debug;

		mystrcpy(gGlobalContextP->component,
			sizeof(gGlobalContextP->component), kPmLogGlobalContextName);

		gGlobalContextP->info.enabledLevel = kPmLogLevel_Debug;
		gGlobalContextP->info.flags = 0;

		needInit = true;
	}
	//---------------------------------------------------------------
	else if (gGlobalsP->signature == PMLOG_SIGNATURE)
	{
		DbgPrint("accessing shared mem\n");
	}
	//---------------------------------------------------------------
	else
	{
		ErrPrint("unrecognized shared mem\n");

		gGlobalsP = NULL;
		gGlobalContextP = NULL;
	}
	//---------------------------------------------------------------

	if (needInit)
	{
		(void) PrvReadGlobalConfig(gGlobalsP);
	}

	// release the globals lock
	PmLogPrvUnlock();

	// initialize contexts if this is the first time
	if (needInit)
	{
		(void) PrvInitContexts();
	}
}


/*********************************************************************/
/* fini_function */
/**
@brief  Library destructor executes automatically in the loading
	process as the library is being unloaded.
**********************************************************************/
static void __attribute ((destructor)) fini_function(void)
{
	int		status;
	int		err;

	//------------------------------------------------------------

	gGlobalsP = NULL;
	gGlobalContextP = NULL;

	if (gShmData != NULL)
	{
		DbgPrint("detaching shared mem\n");

		status = shmdt(gShmData);
		if (status != 0)
		{
			err = errno;
			ErrPrint("shmdt error: %s\n", strerror(err));
		}

		gShmData = NULL;
	}

	//------------------------------------------------------------

	if (gShmId != -1)
	{
		gShmId = -1;
	}

	//------------------------------------------------------------

	if (gSem != SEM_FAILED)
	{
		DbgPrint("closing sem\n");

		status = sem_close(gSem);
		if (status != 0)
		{
			err = errno;
			ErrPrint("sem_close error: %s\n", strerror(err));
		}

		gSem = SEM_FAILED;
	}

	//------------------------------------------------------------
}


/*********************************************************************/
/* PmLogPrvGlobals */
/**
@brief  Returns the pointer to the globals.
**********************************************************************/
PmLogGlobals* PmLogPrvGlobals(void)
{
	return gGlobalsP;
}


/*********************************************************************/
/* PmLogPrvLock */
/**
@brief  Acquires the semaphore for write access to the PmLog shared
		memory context.  This should be held as briefly as possible,
		then released by calling PmLogPrvUnlock.
**********************************************************************/
void PmLogPrvLock(void)
{
	int		status;
	int		err;

	status = sem_wait(gSem);
	if (status == -1)
	{
		err = errno;
		ErrPrint("sem_wait error: %s\n", strerror(err));
		return;
	}
}


/*********************************************************************/
/* PmLogPrvUnlock */
/**
@brief  Releases the semaphore for write access to the PmLog shared
		memory context, as previously acquired via PmLogPrvLock.
**********************************************************************/
void PmLogPrvUnlock(void)
{
	int		status;
	int		err;

	status = sem_post(gSem);
	if (status == -1)
	{
		err = errno;
		ErrPrint("sem_post error: %s\n", strerror(err));
		return;
	}
}


/*********************************************************************/
/* PrvResolveContext */
/**
@brief  Resolve a public PmLogContext pointer to the corresponding
		PmLogContext_ pointer.
		Also, resolve a NULL pointer to the real pointer of the
		global context.
**********************************************************************/
static inline PmLogContext_* PrvResolveContext(PmLogContext context)
{
	return (context == NULL) ? gGlobalContextP : (PmLogContext_*) context;
}


/*********************************************************************/
/* PrvExportContext */
/**
@brief  Convert a private PmLogContext_ pointer to the corresponding
		public PmLogContext pointer.
**********************************************************************/
static inline PmLogContext PrvExportContext(const PmLogContext_* contextP)
{
	return (contextP == NULL) ? NULL : &contextP->info;
}


/*********************************************************************/
/* PrvIsGlobalContext */
/**
@brief  Returns true if the specified context pointer is for the
		global context.
**********************************************************************/
static inline bool PrvIsGlobalContext(const PmLogContext_* contextP)
{
	// context should already have been resolved
	assert(contextP != NULL);
	return (contextP == gGlobalContextP);
}


/*********************************************************************/
/* PrvIsValidLevel */
/**
@brief  Checks if the level is recognized.
**********************************************************************/
static inline bool PrvIsValidLevel(PmLogLevel level)
{
	return
		(level >= kPmLogLevel_Emergency) &&
		(level <= kPmLogLevel_Debug);
}


/*********************************************************************/
/* PrvValidateContextName */
/**
@brief  Returns true if and only if the given string is a valid
		context name, i.e. meets the requirements for length
		and valid characters.
**********************************************************************/
static PmLogErr PrvValidateContextName(const char* contextName)
{
	size_t	n;
	size_t	i;
	char	c;

	// the global context name is a special case
	if (strcmp(contextName, kPmLogGlobalContextName) == 0)
	{
		return kPmLogErr_None;
	}

	n = strlen(contextName);
	if ((n < 1) || (n > PMLOG_MAX_CONTEXT_NAME_LEN))
	{
		return kPmLogErr_InvalidContextName;
	}

	for (i = 0; i < n; i++)
	{
		c = contextName[ i ];

		if ((c >= 'A') && (c <= 'Z'))
			continue;

		if ((c >= 'a') && (c <= 'z'))
			continue;

		if ((c >= '0') && (c <= '9'))
			continue;

		if ((c == '.') ||
			(c == '-') ||
			(c == '_'))
			continue;

		return kPmLogErr_InvalidContextName;
	}

	return kPmLogErr_None;
}


/*********************************************************************/
/* PmLogGetNumContexts */
/**
@brief  Returns the number of defined contexts, including the global
		context.

@return Error code:
			kPmLogErr_None
			kPmLogErr_InvalidParameter
**********************************************************************/
PmLogErr PmLogGetNumContexts(int* pNumContexts)
{
	if (pNumContexts == NULL)
	{
		return kPmLogErr_InvalidParameter;
	}

	*pNumContexts = 0;

	if (gGlobalsP == NULL)
	{
		return kPmLogErr_Unknown;
	}

	*pNumContexts = 1 + gGlobalsP->numUserContexts;
	return kPmLogErr_None;
}


/*********************************************************************/
/* PmLogGetIndContext */
/**
@brief  Returns the context by index where index = 0..numContexts - 1.

@return Error code:
			kPmLogErr_None
			kPmLogErr_InvalidParameter
**********************************************************************/
PmLogErr PmLogGetIndContext(int contextIndex, PmLogContext* pContext)
{
	PmLogContext_*	theContextP;

	if (pContext != NULL)
	{
		*pContext = NULL;
	}

	if ((contextIndex < 0) || (contextIndex > gGlobalsP->numUserContexts))
	{
		return kPmLogErr_InvalidContextIndex;
	}

	if (pContext == NULL)
	{
		return kPmLogErr_InvalidParameter;
	}

	if (contextIndex == 0)
	{
		theContextP = &gGlobalsP->globalContext;
	}
	else
	{
		theContextP = &gGlobalsP->userContexts[ contextIndex - 1 ];
	}

	*pContext = PrvExportContext(theContextP);
	return kPmLogErr_None;
}


/*********************************************************************/
/* PmLogFindContext */
/**
@brief  Returns the logging context for the named context, or
		NULL if the context does not exist.

@return Error code:
			kPmLogErr_None
			kPmLogErr_InvalidParameter
			kPmLogErr_InvalidContext
			kPmLogErr_InvalidContextName
**********************************************************************/
PmLogErr PmLogFindContext(const char* contextName, PmLogContext* pContext)
{
	PmLogErr		logErr;
	int				i;
	PmLogContext_*	theContextP;
	PmLogContext_*	contextP;

	if (pContext == NULL)
	{
		return kPmLogErr_InvalidParameter;
	}

	*pContext = NULL;

	if (gGlobalsP == NULL)
	{
		return kPmLogErr_Unknown;
	}

	if (contextName == NULL)
	{
		return kPmLogErr_InvalidParameter;
	}

	logErr = PrvValidateContextName(contextName);
	if (logErr != kPmLogErr_None)
	{
		return logErr;
	}

	// lock the globals
	PmLogPrvLock();

	theContextP = NULL;

	// look for a match on the context name
	for (i = -1; i < gGlobalsP->numUserContexts; i++)
	{
		if (i == -1)
		{
			contextP = &gGlobalsP->globalContext;
		}
		else
		{
			contextP = &gGlobalsP->userContexts[ i ];
		}

		if (strcmp(contextName, contextP->component) == 0)
		{
			theContextP = contextP;
			break;
		}
	}

	// release the globals lock
	PmLogPrvUnlock();

	if (theContextP != NULL)
	{
		*pContext = PrvExportContext(theContextP);
		return kPmLogErr_None;
	}

	return kPmLogErr_ContextNotFound;
}


/*********************************************************************/
/* PrvGetContextDefaults */
/**
@brief  Look up the default settings for the context.  If the
		component is part of a hierarchy, search up the ancestor
		chain and use the settings of the first ancestor found.
		Otherwise, use the settings from the global context.
**********************************************************************/
static const PmLogContextInfo* PrvGetContextDefaults(const char* contextName)
{
	// Note: this function is called only by PmLogGetContext when
	// the context globals are locked.

	int						i;
	const PmLogContext_*	contextP;
	char					parent[ PMLOG_MAX_CONTEXT_NAME_LEN + 1 ];
	char*					s;

	// copy the context name to a scratch buffer
	mystrcpy(parent, sizeof(parent), contextName);

	for (;;)
	{
		// if there is no 'parent' path, we're done
		s = strrchr(parent, '.');
		if (s == NULL)
		{
			break;
		}

		// else trim off the child name to get the parent
		*s = 0;

		// if a registered context matches the parent path,
		// use its level as the default for the child
		for (i = 0; i < gGlobalsP->numUserContexts; i++)
		{
			contextP = &gGlobalsP->userContexts[ i ];
			if (strcmp(parent, contextP->component) == 0)
			{
				return &contextP->info;
			}
		}
	}

	// otherwise use the global level as the default
	return &gGlobalContextP->info;
}


/*********************************************************************/
/* PmLogGetContext */
/**
@brief  Returns/creates the logging context for the named context.

		If contextName is NULL, returns the global context.

		Context names must be 1..31 characters long, and each
		character must be one of A-Z, a-z, 0-9, '_', '-', '.'.

		Component hierarchies can be indicated using '.' as the path
		separator.
		E.g. "FOO.BAR" would indicate the "BAR" subcomponent
		of the "FOO" component.

@return Error code:
			kPmLogErr_None
			kPmLogErr_InvalidParameter
			kPmLogErr_InvalidContext
			kPmLogErr_InvalidContextName
**********************************************************************/
PmLogErr PmLogGetContext(const char* contextName, PmLogContext* pContext)
{
	PmLogErr				logErr;
	int						i;
	PmLogContext_*			theContextP;
	PmLogContext_*			contextP;
	const PmLogContextInfo*	defaultsP;

	if (pContext == NULL)
	{
		return kPmLogErr_InvalidParameter;
	}

	*pContext = NULL;

	if (gGlobalsP == NULL)
	{
		return kPmLogErr_Unknown;
	}

	if (contextName == NULL)
	{
		*pContext = PrvExportContext(gGlobalContextP);
		return kPmLogErr_None;
	}

	logErr = PrvValidateContextName(contextName);
	if (logErr != kPmLogErr_None)
	{
		return logErr;
	}

	// lock the globals
	PmLogPrvLock();

	theContextP = NULL;
	logErr = kPmLogErr_None;

	// look for a match on the context name
	for (i = -1; i < gGlobalsP->numUserContexts; i++)
	{
		if (i == -1)
		{
			contextP = &gGlobalsP->globalContext;
		}
		else
		{
			contextP = &gGlobalsP->userContexts[ i ];
		}

		if (strcmp(contextName, contextP->component) == 0)
		{
			//DbgPrint("found context %s\n", contextName);
			theContextP = contextP;
			break;
		}
	}

	// if context not found, add it
	if (theContextP == NULL)
	{
		if (gGlobalsP->numUserContexts >= gGlobalsP->maxUserContexts)
		{
			DbgPrint("no more contexts available\n");
			logErr = kPmLogErr_TooManyContexts;
		}
		else
		{
			DbgPrint("adding context %s\n", contextName);
			theContextP = &gGlobalsP->userContexts[ gGlobalsP->numUserContexts ];
			gGlobalsP->numUserContexts++;

			mystrcpy(theContextP->component, sizeof(theContextP->component),
				contextName);

			defaultsP = PrvGetContextDefaults(contextName);

			theContextP->info.enabledLevel = defaultsP->enabledLevel;
			theContextP->info.flags = defaultsP->flags;
		}
	}

	// release the globals lock
	PmLogPrvUnlock();

	if (theContextP != NULL)
	{
		*pContext = PrvExportContext(theContextP);
		return kPmLogErr_None;
	}

	// in case of error, return the global context pointer
	*pContext = PrvExportContext(gGlobalContextP);
	return logErr;
}


/*********************************************************************/
/* PmLogGetContextInline */
/**
@brief  Returns the logging context for the named context.
**********************************************************************/
PmLogContext PmLogGetContextInline(const char* contextName)
{
	PmLogErr		logErr;
	PmLogContext	context;

	context = NULL;
	logErr = PmLogGetContext(contextName, &context);
	if (logErr == kPmLogErr_None)
	{
		return context;
	}

	return kPmLogGlobalContext;
}


/*********************************************************************/
/* PmLogGetContextName */
/**
@brief  Returns the name of the specified context into the specified
		string buffer.

@return Error code:
			kPmLogErr_None
			kPmLogErr_InvalidParameter
**********************************************************************/
PmLogErr PmLogGetContextName(PmLogContext context, char* contextName,
	size_t contextNameBuffSize)
{
	PmLogContext_*	contextP;

	// clear out result in case of error
	if ((contextName != NULL) && (contextNameBuffSize > 0))
	{
		contextName[ 0 ] = 0;
	}

	contextP = PrvResolveContext(context);
	if (contextP == NULL)
	{
		return kPmLogErr_InvalidContext;
	}

	if ((contextName == NULL) || (contextNameBuffSize <= 1))
	{
		return kPmLogErr_InvalidParameter;
	}

	mystrcpy(contextName, contextNameBuffSize, contextP->component);

	if (contextNameBuffSize < strlen(contextP->component) + 1)
	{
		return kPmLogErr_BufferTooSmall;
	}

	return kPmLogErr_None;
}


/*********************************************************************/
/* PmLogGetContextLevel */
/**
@brief  Gets the logging level for the specified context.
		May be used for the global context.  This should generally not
		be used by the components doing logging themselves.
		Instead just use the PmLogPrint etc. APIs which will do
		inline enabled checks.

@return Error code:
			kPmLogErr_None
			kPmLogErr_InvalidParameter
**********************************************************************/
PmLogErr PmLogGetContextLevel(PmLogContext context, PmLogLevel* levelP)
{
	PmLogContext_*	contextP;

	// clear out result in case of error
	if (levelP != NULL)
	{
		*levelP = kPmLogLevel_Debug;
	}

	contextP = PrvResolveContext(context);
	if (contextP == NULL)
	{
		return kPmLogErr_InvalidContext;
	}

	if (levelP == NULL)
	{
		return kPmLogErr_InvalidParameter;
	}

	*levelP = contextP->info.enabledLevel;

	return kPmLogErr_None;
}


/*********************************************************************/
/* PmLogSetContextLevel */
/**
@brief  Sets the logging level for the specified context.
		May be used for the global context.
**********************************************************************/
PmLogErr PmLogSetContextLevel(PmLogContext context, PmLogLevel level)
{
	PmLogContext_*	contextP;

	contextP = PrvResolveContext(context);
	if (contextP == NULL)
	{
		return kPmLogErr_InvalidContext;
	}

	if ((level != kPmLogLevel_None) && !PrvIsValidLevel(level))
	{
		return kPmLogErr_InvalidLevel;
	}

	// dummy reference to avoid unused function warning
	// when DbgPrint is compiled out
	(void) &PrvGetLevelStr;
	DbgPrint("SetContextLevel %s => %s\n", contextP->component,
		PrvGetLevelStr(level));

	contextP->info.enabledLevel = level;
	return kPmLogErr_None;
}


/*********************************************************************/
/* PrvCheckContext */
/**
@brief  Validate the context and check whether logging is enabled.
**********************************************************************/
static PmLogErr PrvCheckContext(const PmLogContext_* contextP,
	PmLogLevel level)
{
	// context should already have been resolved
	assert(contextP != NULL);

	if (!PrvIsValidLevel(level))
	{
		return kPmLogErr_InvalidLevel;
	}

	if (level > contextP->info.enabledLevel)
	{
		return kPmLogErr_LevelDisabled;
	}

	return kPmLogErr_None;
}


/*********************************************************************/
/* PrvLogToConsole */
/**
@brief  Echos the logged info + message to the output.
**********************************************************************/
static void PrvLogToConsole(FILE* out, const char* identStr,
	const char* ptidStr, const char* componentStr, const char* s)
{
	size_t		sLen;
	const char* endStr;

	sLen = strlen(s);

	if ((sLen > 0) && (s[sLen - 1] == '\n'))
	{
		endStr = "";
	}
	else
	{
		endStr = "\n";
	}

	fprintf(out, "%s%s%s%s%s", identStr, ptidStr, componentStr, s, endStr);
}


/***********************************************************************
 * HandleLogLibCommand
 ***********************************************************************/
static bool HandleLogLibCommand(const char* msg)
{
	const char*  kLogLibCmdPrefix    = "!loglib ";
	const size_t kLogLibCmdPrefixLen = 8;

	if (strncmp(msg, kLogLibCmdPrefix, kLogLibCmdPrefixLen) != 0)
	{
		return false;
	}

	msg += kLogLibCmdPrefixLen;

	if (strcmp(msg, "loadconf") == 0)
	{
		DbgPrint("HandleLogLibCommand: re-loading global config\n");
		(void) PrvReadGlobalConfig(gGlobalsP);
		return true;
	}

	return false;
}


/*********************************************************************/
/* PrvLogWrite */
/**
@brief  Logs the specified formatted text to the specified context.
**********************************************************************/
static PmLogErr PrvLogWrite(PmLogContext_* contextP, PmLogLevel level,
	const char* s)
{
	const char*	identStr;
	pid_t		pid;
	pid_t		tid;
	char		ptidStr[ 32 ];
	int			savedErrNo;
	char		componentStr[ 1 + PMLOG_MAX_CONTEXT_NAME_LEN + 3 +1 ]; // one character before, 3 after, \0 terminator

	// save and restore errno, so logging doesn't have side effects
	savedErrNo = errno;

	if (HandleLogLibCommand(s))
	{
		goto Exit;
	}

	identStr = __progname;

	if ((gGlobalsP->flags & kPmLogGlobalsFlag_LogProcessIds) ||
		(gGlobalsP->flags & kPmLogGlobalsFlag_LogThreadIds))
	{
		pid = getpid();
		tid = gettid();
		if (gGlobalsP->flags & kPmLogGlobalsFlag_LogThreadIds &&
			(tid != pid))
		{
			mysprintf(ptidStr, sizeof(ptidStr), "[%d:%d]: ", (int) pid,
				(int) tid);
		}
		else
		{
			mysprintf(ptidStr, sizeof(ptidStr), "[%d]: ", (int) pid);
		}
	}
	else
	{
		ptidStr[0] = 0;
	}

	if (PrvIsGlobalContext(contextP))
	{
		componentStr[0] = 0;
	}
	else
	{
		mysprintf(componentStr, sizeof(componentStr), "{%s}: ",
			contextP->component);
	}

	syslog(level, "%s%s%s", ptidStr, componentStr, s);

	if (gGlobalsP->flags & kPmLogGlobalsFlag_LogToConsole)
	{
		const PmLogConsole* consoleConfP = &gGlobalsP->consoleConf;

		if (ptidStr[0] == 0)
		{
			mystrcpy(ptidStr, sizeof(ptidStr), ": ");
		}

		if ((level >= consoleConfP->stdErrMinLevel) &&
			(level <= consoleConfP->stdErrMaxLevel))
		{
			PrvLogToConsole(stderr, identStr, ptidStr, componentStr, s);
		}

		if ((level >= consoleConfP->stdOutMinLevel) &&
			(level <= consoleConfP->stdOutMaxLevel))
		{
			PrvLogToConsole(stdout, identStr, ptidStr, componentStr, s);
		}
	}

Exit:
	// save and restore errno, so logging doesn't have side effects
	errno = savedErrNo;

	return kPmLogErr_None;
}


/*********************************************************************/
/* PrvLogVPrint */
/**
@brief  Logs the specified formatted text to the specified context.
**********************************************************************/
static PmLogErr PrvLogVPrint(PmLogContext_* contextP, PmLogLevel level,
	const char* fmt, va_list args)
{
	const size_t kLineBuffSize = 1024;

	PmLogErr		logErr;
	char			lineStr[ kLineBuffSize ];
	int				n;
	int				err;

	if ((fmt == NULL) || (fmt[0] == 0))
	{
		return kPmLogErr_InvalidFormat;
	}

	n = vsnprintf(lineStr, sizeof(lineStr), fmt, args);
	if (n < 0)
	{
		err = errno;
		ErrPrint("vsnprintf error %s\n", strerror(err));
		logErr = kPmLogErr_FormatStringFailed;
	}
	else
	{
		if (n >= sizeof(lineStr))
		{
			DbgPrint("vsnprintf truncation\n");
			lineStr[ sizeof(lineStr) - 1 ] = 0;
		}

		logErr = PrvLogWrite(contextP, level, lineStr);
	}

	return logErr;
}


/*********************************************************************/
/* PmLogPrint_ */
/**
@brief  Logs the specified formatted text to the specified context.
**********************************************************************/
PmLogErr PmLogPrint_(PmLogContext context, PmLogLevel level,
	const char* fmt, ...)
{
	PmLogContext_*	contextP;
	PmLogErr		logErr;
	va_list 		args;

	contextP = PrvResolveContext(context);
	if (contextP == NULL)
	{
		return kPmLogErr_InvalidContext;
	}

	logErr = PrvCheckContext(contextP, level);
	if (logErr != kPmLogErr_None)
	{
		return logErr;
	}

	va_start(args, fmt);

	logErr = PrvLogVPrint(contextP, level, fmt, args);

	va_end(args);

	return logErr;
}


/*********************************************************************/
/* PmLogVPrint_ */
/**
@brief  Logs the specified formatted text to the specified context.

		For efficiency, this API should not be used directly, but
		instead use the wrappers (PmLogVPrint, PmLogVPrintError, ...)
		that bypass the library call if the logging is not enabled.

@return Error code:
			kPmLogErr_None
			kPmLogErr_InvalidContext
			kPmLogErr_InvalidLevel
			kPmLogErr_InvalidFormat
**********************************************************************/
PmLogErr PmLogVPrint_(PmLogContext context, PmLogLevel level,
	const char* fmt, va_list args)
{
	PmLogContext_*	contextP;
	PmLogErr		logErr;

	contextP = PrvResolveContext(context);
	if (contextP == NULL)
	{
		return kPmLogErr_InvalidContext;
	}

	logErr = PrvCheckContext(contextP, level);
	if (logErr != kPmLogErr_None)
	{
		return logErr;
	}

	logErr = PrvLogVPrint(contextP, level, fmt, args);

	return logErr;
}


/*********************************************************************/
/* DumpData_OffsetHexAscii */
/**
@brief  Dump the specified data as hex dump with ASCII view too.
		This is similar to the "hexdump -C" output format, but that
		is not a requirement.  One difference is we don't output a
		trailing empty line with the final offset.

	000030c0  02 02 00 00 06 00 00 00  02 06 00 00 06 00 00 41  \
		|...............A|
**********************************************************************/
static PmLogErr DumpData_OffsetHexAscii(PmLogContext_* contextP,
	PmLogLevel level, const void* dataP, size_t dataSize)
{
	const size_t kMaxBytesPerLine = 16;

	const size_t kMaxLineLen = 8 + 2 + kMaxBytesPerLine * 3 + 2 +
		1 + kMaxBytesPerLine + 1;

	const uint8_t*	srcP;
	uint8_t			b;
	size_t			srcOffset;
	char			lineBuff[ kMaxLineLen + 1 ];
	size_t			lineBytes;
	size_t			i;
	char*			lineP;
	PmLogErr		logErr;

	logErr = kPmLogErr_NoData;

	srcP = (const uint8_t*) dataP;
	srcOffset = 0;

	while (srcOffset < dataSize)
	{
		lineBytes = dataSize - srcOffset;
		if (lineBytes > kMaxBytesPerLine)
		{
			lineBytes = kMaxBytesPerLine;
		}

		mysprintf(lineBuff, sizeof(lineBuff), "%08zX", srcOffset);

		lineP = lineBuff + 8;
		*lineP++ = ' ';
		*lineP++ = ' ';

		for (i = 0; i < kMaxBytesPerLine; i++)
		{
			if (i == 8)
			{
				*lineP++ = ' ';
			}

			if (i < lineBytes)
			{
				b = srcP[ i ];
				*lineP++ = kHexChars[ b >> 4 ];
				*lineP++ = kHexChars[ b & 0x0F ];
			}
			else
			{
				*lineP++ = ' ';
				*lineP++ = ' ';
			}

			*lineP++ = ' ';
		}

		*lineP++ = ' ';

		*lineP++ = '|';

		for (i = 0; i < lineBytes; i++)
		{
			b = srcP[ i ];
			if (!((b >= 0x20) && (b <= 0x7E)))
			{
				b = '.';
			}
			*lineP++ = (char) b;
		}

		*lineP++ = '|';

		// sanity check that the buffer was sized correctly
		assert((lineBytes < kMaxBytesPerLine) || ((lineP - lineBuff) == (ptrdiff_t) kMaxLineLen));
		*lineP = 0;

		logErr = PrvLogWrite(contextP, level, lineBuff);
		if (logErr != kPmLogErr_None)
		{
			break;
		}

		srcP += lineBytes;
		srcOffset += lineBytes;
	}

	return logErr;
}


/*********************************************************************/
/* PmLogDumpData_ */
/**
@brief  Logs the specified binary data as text dump to the specified context.
		Specify kPmLogDumpFormatDefault for the formatting parameter.
		For efficiency, this API should not be used directly, but
		instead use the wrappers (PmLogDumpData, ...) that
		bypass the library call if the logging is not enabled.
**********************************************************************/
PmLogErr PmLogDumpData_(PmLogContext context, PmLogLevel level,
	const void* data, size_t numBytes, const PmLogDumpFormat* format)
{

	PmLogContext_*	contextP;
	PmLogErr		logErr;
	const uint8_t*	pData;

	contextP = PrvResolveContext(context);
	if (contextP == NULL)
	{
		return kPmLogErr_InvalidContext;
	}

	logErr = PrvCheckContext(contextP, level);
	if (logErr != kPmLogErr_None)
	{
		return logErr;
	}

	if (numBytes == 0)
	{
		return kPmLogErr_NoData;
	}

	pData = (const uint8_t*) data;
	if (pData == NULL)
	{
		return kPmLogErr_InvalidData;
	}

	//?? TO DO: allow specifying format
	if (format != kPmLogDumpFormatDefault)
	{
		return kPmLogErr_InvalidFormat;
	}

	logErr = DumpData_OffsetHexAscii(contextP, level, data, numBytes);

	return logErr;
}


/***********************************************************************
 * PmLogGetErrDbgString
 *
 * Given the numeric error code value, returning a matching symbolic
 * string.  For debugging only, never to appear in user interface!
 ***********************************************************************/
const char* PmLogGetErrDbgString(PmLogErr logErr)
{
	#define DEFINE_ERR_STR(e)	\
		case kPmLogErr_##e: return #e

	switch (logErr)
	{
		/*   0 */ DEFINE_ERR_STR( None );
		//---------------------------------------------
		/*   1 */ DEFINE_ERR_STR( InvalidParameter );
		/*   2 */ DEFINE_ERR_STR( InvalidContextIndex );
		/*   3 */ DEFINE_ERR_STR( InvalidContext );
		/*   4 */ DEFINE_ERR_STR( InvalidLevel );
		/*   5 */ DEFINE_ERR_STR( InvalidFormat );
		/*   6 */ DEFINE_ERR_STR( InvalidData );
		/*   7 */ DEFINE_ERR_STR( NoData );
		/*   8 */ DEFINE_ERR_STR( TooMuchData );
		/*   9 */ DEFINE_ERR_STR( LevelDisabled );
		/*  10 */ DEFINE_ERR_STR( FormatStringFailed );
		/*  11 */ DEFINE_ERR_STR( TooManyContexts );
		/*  12 */ DEFINE_ERR_STR( InvalidContextName );
		/*  13 */ DEFINE_ERR_STR( ContextNotFound );
		/*  14 */ DEFINE_ERR_STR( BufferTooSmall );
		//---------------------------------------------
		/* 999 */ DEFINE_ERR_STR( Unknown );
	}

	#undef DEFINE_ERR_STR

	// default
	return "?";
}


/*********************************************************************/
/* PmLogPrvTestReadMem */
/**
@brief  This is a private function to be used only by PmLog components
		for test and development purposes.
**********************************************************************/
static PmLogErr PmLogPrvTestReadMem(void* data)
{
	const unsigned long* p;
	unsigned long n;

	p = (const unsigned long*) data;
	printf("PmLogPrvTestReadMem 0x%08lX...\n", (unsigned long) p);
	n = *p;
	printf("PmLogPrvTestReadMem result = 0x%08lX...\n", n);

	return kPmLogErr_None;
}


/*********************************************************************/
/* PmLogPrvTest */
/**
@brief  This is a private function to be used only by PmLog components
		for test and development purposes.
**********************************************************************/
PmLogErr PmLogPrvTest(const char* cmd, void* data)
{
	if (strcmp(cmd, "ReadMem") == 0)
	{
		return PmLogPrvTestReadMem(data);
	}

	return kPmLogErr_InvalidParameter;
}


