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

#include "buffer.h"
#include "i18n.h"

/* For BUFFER_RECEPTION */
#include "serial-port.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#define BUFFER_SIZE (128 * 1024)

typedef struct {
    char *buffer;
    gboolean cr_received;
    unsigned int pointer;
    char *current_buffer;
    gboolean overlapped;
    GtBufferFunc write_func;
    gpointer user_data;
} GtBufferPrivate;

struct _GtBuffer {
    GObject parent_instance;
};

struct _GtBufferClass {
    GObjectClass parent_class;
};

enum {
    SIGNAL_NEW_BUFFER,
    SIGNAL_CLEARED,
    SIGNAL_COUNT,
} GtBufferSignals;

static guint SIGNALS[SIGNAL_COUNT] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (GtBuffer, gt_buffer, G_TYPE_OBJECT)

typedef struct {
    const char *file_name;
    gboolean result;
    GError **error;
} GtBufferSaveClosure;

/* GObject overrides */
static void
gt_buffer_finalize (GObject *object);

static void
gt_buffer_class_init (GtBufferClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    SIGNALS[SIGNAL_NEW_BUFFER] = g_signal_new ("buffer-updated",
                                               GT_TYPE_BUFFER,
                                               G_SIGNAL_RUN_FIRST,
                                               0,
                                               NULL,
                                               NULL,
                                               NULL,
                                               G_TYPE_NONE,
                                               2,
                                               G_TYPE_POINTER,
                                               G_TYPE_UINT);

    SIGNALS[SIGNAL_CLEARED] = g_signal_new ("cleared",
                                            GT_TYPE_BUFFER,
                                            G_SIGNAL_RUN_FIRST,
                                            0,
                                            NULL,
                                            NULL,
                                            NULL,
                                            G_TYPE_NONE,
                                            0);

    object_class->finalize = gt_buffer_finalize;
}

static void
gt_buffer_init (GtBuffer *self)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);
    priv->buffer = g_malloc0 (BUFFER_SIZE);
    priv->current_buffer = priv->buffer;
}

static void
gt_buffer_finalize (GObject *object)
{
    GtBuffer *self = GT_BUFFER (object);
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);
    GObjectClass *object_class = NULL;

    g_clear_pointer (&priv->buffer, g_free);

    object_class = G_OBJECT_CLASS (gt_buffer_parent_class);
    object_class->finalize (object);
}

GtBuffer *
gt_buffer_new (void)
{
    return GT_BUFFER (g_object_new (GT_TYPE_BUFFER, NULL));
}

void
gt_buffer_put_chars (GtBuffer *self,
                     char *chars,
                     unsigned int size,
                     gboolean crlf_auto)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);
    char *characters = NULL;

    g_return_if_fail (self != NULL);

    /* If the auto CR LF mode on, read the buffer to add \r before \n */
    if (crlf_auto) {
        /* BUFFER_RECEPTION*2 for worst case scenario, all \n or \r chars */
        char out_buffer[BUFFER_RECEPTION * 2];
        unsigned int i, out_size = 0;

        for (i = 0; i < size; i++) {
            if (chars[i] == '\r') {
                /* If the previous character was a CR too, insert a newline */
                if (priv->cr_received) {
                    out_buffer[out_size] = '\n';
                    out_size++;
                }
                priv->cr_received = TRUE;
            } else {
                if (chars[i] == '\n') {
                    /* If we get a newline without a CR first, insert a CR */
                    if (!priv->cr_received) {
                        out_buffer[out_size] = '\r';
                        out_size++;
                    }
                } else {
                    /* If we receive a normal char, and the previous one was a
                       CR insert a newline */
                    if (priv->cr_received) {
                        out_buffer[out_size] = '\n';
                        out_size++;
                    }
                }
                priv->cr_received = FALSE;
            }
            out_buffer[out_size] = chars[i];
            out_size++;
        }

        chars = out_buffer;
        size = out_size;
    }

    if (size > BUFFER_SIZE) {
        characters = chars + (size - BUFFER_SIZE);
        size = BUFFER_SIZE;
    } else {
        characters = chars;
    }

    if ((size + priv->pointer) >= BUFFER_SIZE) {
        memcpy (priv->current_buffer, characters, BUFFER_SIZE - priv->pointer);
        chars = characters + BUFFER_SIZE - priv->pointer;
        priv->pointer = size - (BUFFER_SIZE - priv->pointer);
        memcpy (priv->buffer, chars, priv->pointer);
        priv->current_buffer = priv->buffer + priv->pointer;
        priv->overlapped = TRUE;
    } else {
        memcpy (priv->current_buffer, characters, size);
        priv->pointer += size;
        priv->current_buffer += size;
    }

    g_signal_emit (self, SIGNALS[SIGNAL_NEW_BUFFER], 0, characters, size);
    /*if (priv->write_func != NULL)
        priv->write_func (characters, size, priv->user_data); */
}

void
gt_buffer_clear (GtBuffer *self)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);

    priv->overlapped = FALSE;
    memset (priv->buffer, 0, BUFFER_SIZE);
    priv->current_buffer = priv->buffer;
    priv->pointer = 0;
    priv->cr_received = FALSE;
}

void
gt_buffer_write (GtBuffer *self)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);

    if (priv->write_func == NULL) {
        return;
    }

    /* Write the second half of the ringbuffer first (contains start of data) */
    if (priv->overlapped) {
        g_signal_emit (self,
                       SIGNALS[SIGNAL_NEW_BUFFER],
                       0,
                       priv->current_buffer,
                       BUFFER_SIZE - priv->pointer);
        /*priv->write_func (
            priv->current_buffer, BUFFER_SIZE - priv->pointer, priv->user_data);
         */
    }
    // priv->write_func (priv->buffer, priv->pointer, priv->user_data);
    g_signal_emit (
        self, SIGNALS[SIGNAL_NEW_BUFFER], 0, priv->buffer, priv->pointer);
}

void
gt_buffer_write_with_func (GtBuffer *self,
                           GtBufferFunc write_func,
                           gpointer user_data)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);
    GtBufferFunc old_write_func = priv->write_func;
    gpointer old_user_data = priv->user_data;

    priv->write_func = write_func;
    priv->user_data = user_data;
    gt_buffer_write (self);

    priv->write_func = old_write_func;
    priv->user_data = old_user_data;
}

void
gt_buffer_set_display_func (GtBuffer *self,
                            GtBufferFunc write_func,
                            gpointer user_data)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);

    priv->write_func = write_func;
    priv->user_data = user_data;
}

void
gt_buffer_unset_display_func (GtBuffer *self)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);

    priv->write_func = NULL;
}

static void
on_write_to_file (char *buffer, unsigned int size, gpointer user_data)
{
    GtBufferSaveClosure *closure = (GtBufferSaveClosure *)user_data;

    closure->result = g_file_set_contents (
        closure->file_name, buffer, (gssize)size, closure->error);
}

gboolean
gt_buffer_write_to_file (GtBuffer *self, const char *file_name, GError **error)
{
    GtBufferSaveClosure closure = {file_name, FALSE, error};

    gt_buffer_write_with_func (self, on_write_to_file, &closure);

    return closure.result;
}
