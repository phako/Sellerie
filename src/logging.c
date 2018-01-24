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

#include "logging.h"
#include "widgets.h"

#include <stdio.h>

#include <glib/gi18n.h>

#define MAX_WRITE_ATTEMPTS 5

struct _GtLogging
{
    GObject parent_instance;
    gboolean active;
    gchar *LoggingFileName;
    FILE *LoggingFile;
    gchar *logfile_default;
};

G_DEFINE_TYPE (GtLogging, gt_logging, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_ACTIVE,
    N_PROPS
};

static GParamSpec *properties [N_PROPS];

GtLogging *
gt_logging_new (void)
{
    return g_object_new (GT_TYPE_LOGGING, NULL);
}

static void
gt_logging_finalize (GObject *object)
{
    GtLogging *self = (GtLogging *)object;

    g_clear_pointer (&self->LoggingFileName, g_free);
    g_clear_pointer (&self->LoggingFile, fclose);
    g_clear_pointer (&self->logfile_default, g_free);

    G_OBJECT_CLASS (gt_logging_parent_class)->finalize (object);
}

static void
gt_logging_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    GtLogging *self = GT_LOGGING (object);

    switch (prop_id)
      {
          case PROP_ACTIVE:
          g_value_set_boolean (value, self->active);
          break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
}

static void
gt_logging_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    GtLogging *self = GT_LOGGING (object);

    switch (prop_id)
      {
      case PROP_ACTIVE:
          self->active = g_value_get_boolean (value);
          break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
}

static void
gt_logging_class_init (GtLoggingClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gt_logging_finalize;
    object_class->get_property = gt_logging_get_property;
    object_class->set_property = gt_logging_set_property;

    properties[PROP_ACTIVE] = g_param_spec_boolean ("active", "active", "active",
                                                    FALSE,
                                                    G_PARAM_STATIC_STRINGS |
                                                    G_PARAM_READABLE |
                                                    G_PARAM_CONSTRUCT);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gt_logging_init (GtLogging *self)
{
    self->LoggingFileName = NULL;
    self->LoggingFile = NULL;
    self->logfile_default = NULL;
}

gboolean gt_logging_start (GtLogging *self, const gchar *filename)
{
    gchar *str;

    // open file and start logging
    if (!filename || g_ascii_strcasecmp (filename, "") == 0)
    {
        str = g_strdup_printf(_("Filename error\n"));
        show_message(str, MSG_ERR);
        g_free(str);
        return FALSE;
    }

    g_clear_pointer(&self->LoggingFile, fclose);
    self->active = FALSE;

    g_clear_pointer(&self->LoggingFileName, g_free);
    self->LoggingFileName = g_strdup(filename);

    self->LoggingFile = fopen(self->LoggingFileName, "a");
    if (self->LoggingFile == NULL) {
        str = g_strdup_printf(_("Cannot open file %s: %m\n"),
                              self->LoggingFileName);

        show_message(str, MSG_ERR);
        g_free(str);
        g_clear_pointer(&self->LoggingFileName, g_free);
    } else {
        g_clear_pointer(&self->logfile_default, g_free);
        self->logfile_default = g_strdup(self->LoggingFileName);
        self->active = TRUE;
    }

    g_object_notify (G_OBJECT(self), "active");

    return self->active;
}

void gt_logging_pause_resume (GtLogging *self)
{
    if (self->LoggingFile == NULL) {
        return;
    }

    self->active = !self->active;
    g_object_notify (G_OBJECT(self), "active");
}

void gt_logging_stop(GtLogging *self)
{
    if (self->LoggingFile == NULL) {
        return;
    }

    g_clear_pointer(&self->LoggingFile, fclose);
    g_clear_pointer(&self->LoggingFileName, g_free);

    self->active = FALSE;
    g_object_notify (G_OBJECT(self), "active");
}

void gt_logging_clear(GtLogging *self)
{
    if(self->LoggingFile == NULL)
    {
        return;
    }

    //Reopening with "w" will truncate the file
    self->LoggingFile = freopen(self->LoggingFileName, "w", self->LoggingFile);

    if (self->LoggingFile == NULL)
    {
        gchar *str = g_strdup_printf(_("Cannot open file %s: %m\n"),
                                     self->LoggingFileName);
        show_message(str, MSG_ERR);
        g_free(str);

        g_clear_pointer(&self->LoggingFileName, g_free);
    }
}

void gt_logging_log(GtLogging *self, const char *chars, size_t size)
{
   guint writeAttempts = 0;
   guint bytesWritten = 0;

    /* if we are not logging exit */
    if(self->LoggingFile == NULL || self->active == FALSE) {
        return;
    }

    while (bytesWritten < size)
    {
       if (writeAttempts < MAX_WRITE_ATTEMPTS)
       {
          bytesWritten += fwrite(&chars[bytesWritten], 1,
                                 size-bytesWritten, self->LoggingFile);
       }
       else
       {
           show_message(_("Failed to log data\n"), MSG_ERR);
           return;
       }
    }

    fflush(self->LoggingFile);
}

gboolean gt_logging_get_active(GtLogging *self)
{
    return self->active;
}

const char *gt_logging_get_default_file(GtLogging *self)
{
    return self->logfile_default;
}
