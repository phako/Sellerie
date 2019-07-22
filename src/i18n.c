/*
 *   This file is part of Sellerie.
 *
 *   Sellerie is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Sellerie is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Sellerie.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "i18n.h"

#include <errno.h>
#include <langinfo.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

static char *
iconv_from_utf8_to_locale (const char *str, const char *fallback_string)
{
    char *result = g_locale_from_utf8 (str, strlen (str), NULL, NULL, NULL);

    if (result == NULL) {
        return g_strdup (fallback_string);
    }

    return result;
}

int
i18n_printf (const char *format, ...)
{
    char *new_format;
    int return_value;
    va_list args;

    new_format = iconv_from_utf8_to_locale (format, "");

    if (new_format != NULL) {
        va_start (args, format);
        return_value = vprintf (new_format, args);
        va_end (args);
        free (new_format);
    } else
        return_value = 0;

    return return_value;
}

int
i18n_fprintf (FILE *stream, const char *format, ...)
{
    char *new_format;
    int return_value;
    va_list args;

    new_format = iconv_from_utf8_to_locale (format, "");

    if (new_format != NULL) {
        va_start (args, format);
        return_value = vfprintf (stream, new_format, args);
        va_end (args);
        free (new_format);
    } else
        return_value = 0;

    return return_value;
}

void
i18n_perror (const char *s)
{
    char *conv_string;
    int errno_backup;

    errno_backup = errno;

    conv_string = iconv_from_utf8_to_locale (s, "");
    if (conv_string != NULL) {
        fprintf (stderr, "%s: %s\n", conv_string, g_strerror (errno_backup));
        free (conv_string);
    }
}

