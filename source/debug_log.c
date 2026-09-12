/*
 * Debug-only logging: mirrors every LOG() line to dbglogger (UDP) and to a
 * file under /data/GoldHEN, so a crash can be diagnosed over FTP even when
 * the network logger is unavailable. Compiled only with DEBUGLOG=1.
 */
#ifdef DEBUG_ENABLE_LOG
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <dbglogger.h>

#include "app.h"

void pm_debug_log(const char* fmt, ...)
{
	char line[1024];
	va_list ap;
	FILE* fp;

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);

	dbglogger_log("%s", line);

	/* Fails silently until root credentials make /data writable */
	fp = fopen(GOLDHEN_PATH "/plugin_manager.log", "a");
	if (fp)
	{
		fputs(line, fp);
		fputc('\n', fp);
		fflush(fp);
		fsync(fileno(fp));
		fclose(fp);
	}
}
#endif
