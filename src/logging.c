/*
 *   This file is part of GtkTerm.
 *
 *   GtkTerm is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   GtkTerm is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with GtkTerm.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "widgets.h"
#include "buffer.h"
#include "logging.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#define MAX_WRITE_ATTEMPTS 5

static gboolean	  Logging;
static gchar     *LoggingFileName = NULL;
static FILE      *LoggingFile;
static gchar     *logfile_default = NULL;

static gint OpenLogFile(const gchar *filename)
{
    gchar *str;

    // open file and start logging
    if (!filename || (strcmp(filename, "") == 0)) {
        str = g_strdup_printf(_("Filename error\n"));
        show_message(str, MSG_ERR);
        g_free(str);
        return FALSE;
    }

    if (LoggingFile != NULL) {
        fclose(LoggingFile);
        LoggingFile = NULL;
        Logging = FALSE;
    }

    g_clear_pointer(&LoggingFileName, g_free);
    LoggingFileName = g_strdup(filename);

    LoggingFile = fopen(LoggingFileName, "a");
    if (LoggingFile == NULL) {
        str = g_strdup_printf(_("Cannot open file %s: %s\n"), LoggingFileName, strerror(errno));

        show_message(str, MSG_ERR);
        g_free(str);
        g_clear_pointer(&LoggingFileName, g_free);
    } else {
        g_clear_pointer(&logfile_default, g_free);
        logfile_default = g_strdup(LoggingFileName);
        Logging = TRUE;
    }

    return FALSE;
}

void logging_start(const char *file_name)
{
    OpenLogFile(file_name);
}

void logging_clear(void)
{
    if(LoggingFile == NULL)
    {
        return;
    }

    //Reopening with "w" will truncate the file
    LoggingFile = freopen(LoggingFileName, "w", LoggingFile);

    if (LoggingFile == NULL)
    {
        gchar *str = g_strdup_printf(_("Cannot open file %s: %m\n"), LoggingFileName);
        show_message(str, MSG_ERR);
        g_free(str);

        g_clear_pointer(&LoggingFileName, g_free);
    }
}

void logging_pause_resume(void)
{
    if(LoggingFile == NULL) {
        return;
    }

    Logging = !Logging;
}

void logging_stop(void)
{
    if (LoggingFile == NULL) {
        return;
    }

    g_clear_pointer(&LoggingFile, fclose);
    g_clear_pointer(&LoggingFileName, g_free);

    Logging = FALSE;
}

void log_chars(const char *chars, size_t size)
{
   guint writeAttempts = 0;
   guint bytesWritten = 0;

    /* if we are not logging exit */
    if(LoggingFile == NULL || Logging == FALSE) {
    	return;
    }

    while (bytesWritten < size)
    {
       if (writeAttempts < MAX_WRITE_ATTEMPTS)
       {
          bytesWritten += fwrite(&chars[bytesWritten], 1,
                                 size-bytesWritten, LoggingFile);
       }
       else
       {
           show_message(_("Failed to log data\n"), MSG_ERR);
          return;
       }
    }

    fflush(LoggingFile);
}

gboolean gt_logging_get_active(void)
{
    return Logging;
}

const char *gt_logging_get_default_file(void)
{
    return logfile_default;
}
