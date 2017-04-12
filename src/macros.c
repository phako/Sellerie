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
#include "macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

void gt_macros_add_shortcut (GtkButton *button, gpointer pointer);
void gt_macros_remove_shortcut (GtkButton *button, gpointer pointer);
void gt_macros_save (GtkButton *button, gpointer pointer);
void gt_macros_show_help (GtkButton *button, gpointer pointer);

enum
  {
    COLUMN_SHORTCUT,
    COLUMN_ACTION,
    NUM_COLUMNS
  };

macro_t *macros = NULL;
static GtkWidget *window = NULL;

macro_t *get_shortcuts(gint *size)
{
  gint i = 0;

  if(macros != NULL)
    {
      while(macros[i].shortcut != NULL)
	i++;
    }
  *size = i;
  return macros;
}


static void shortcut_callback(gpointer *number)
{
  gchar *string;
  gchar *str;
  gint i, length;
  guchar a;
  guint val_read;

  string = macros[(long)number].action;
  length = strlen(string);

  for(i = 0; i < length; i++)
    {
      if(string[i] == '\\')
        {
	  if(g_unichar_isdigit((gunichar)string[i + 1]))
	    {
	      if((string[i + 1] == '0') && (string[i + 2] != 0))
		{
		  if(g_unichar_isxdigit((gunichar)string[i + 3]))
		    {
		      str = &string[i + 2];
		      i += 3;
		    }
		  else
		    {
		      str = &string[i + 1];
		      if(g_unichar_isxdigit((gunichar)string[i + 2]))
			i += 2;
		      else
			i++;
		    }
		}
	      else
		{
		  str = &string[i + 1];
		  if(g_unichar_isxdigit((gunichar)string[i + 2]))
		    i += 2;
		  else
		    i++;
		}
	      if(sscanf(str, "%02X", &val_read) == 1)
		a = (guchar)val_read;
	      else
		a = '\\';
	    }
	  else
	    {
	      switch(string[i + 1])
		{
		case 'a':
		  a = '\a';
		  break;
		case 'b':
		  a = '\b';
		  break;
		case 't':
		  a = '\t';
		  break;
		case 'n':
		  a = '\n';
		  break;
		case 'v':
		  a = '\v';
		  break;
		case 'f':
		  a = '\f';
		  break;
		case 'r':
		  a = '\r';
		  break;
		case '\\':
		  a = '\\';
		  break;
		default:
		  a = '\\';
		  i--;
		  break;
		}
	      i++;
	    }
	  send_serial((gchar*)&a, 1);
	}
      else
	{
	  send_serial(&string[i], 1);
	}
    }

  str = g_strdup_printf(_("Macro \"%s\" sent !"), macros[(long)number].shortcut);
  Put_temp_message(str, 800);
  g_free(str);
}

void create_shortcuts(macro_t *macro, gint size)
{
  macros = g_malloc((size + 1) * sizeof(macro_t));
  if(macros != NULL)
    {
      memcpy(macros, macro, size * sizeof(macro_t));
      macros[size].shortcut = NULL;
      macros[size].action = NULL;
    }
  else
    perror("malloc");
}

void add_shortcuts(void)
{
  long i = 0;
  guint acc_key;
  GdkModifierType mod;

  if(macros == NULL)
    return;

  while(macros[i].shortcut != NULL)
    {
      macros[i].closure = g_cclosure_new_swap(G_CALLBACK(shortcut_callback), (gpointer)i, NULL);
      gtk_accelerator_parse(macros[i].shortcut, &acc_key, &mod);
      if(acc_key != 0)
          gt_main_window_add_shortcut (acc_key, mod, macros[i].closure);
      i++;
    }
}

static void macros_destroy(void)
{
  gint i = 0;

  if(macros == NULL)
    return;

  while(macros[i].shortcut != NULL)
    {
      g_free(macros[i].shortcut);
      g_free(macros[i].action);
      /*
      g_closure_unref(macros[i].closure);
      */
      i++;
    }
  g_free(macros);
  macros = NULL;
}

void remove_shortcuts(void)
{
  gint i = 0;

  if(macros == NULL)
    return;

  while(macros[i].shortcut != NULL)
    {
        gt_main_window_remove_shortcut (macros[i].closure);
      i++;
    }

  macros_destroy();
}

static GtkTreeModel *create_model(GtkListStore *store)
{
  gint i = 0;
  GtkTreeIter iter;

  /* add data to the list store */
  if(macros != NULL)
    {
      while(1)
	{
	  if(macros[i].shortcut == NULL)
	    break;
	  gtk_list_store_append (store, &iter);
	  gtk_list_store_set (store, &iter,
			      COLUMN_SHORTCUT, macros[i].shortcut,
			      COLUMN_ACTION, macros[i].action,
			      -1);
	  i++;
	}
    }

  return GTK_TREE_MODEL(store);
}

static gboolean
shortcut_edited (GtkCellRendererText *cell,
		 const gchar         *path_string,
		 const gchar         *new_text,
		 gpointer             data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;

  gtk_tree_model_get_iter(model, &iter, path);

  gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_ACTION, new_text, -1);
  gtk_tree_path_free (path);

  return TRUE;
}

void gt_macros_add_shortcut (GtkButton *button, gpointer pointer)
{
  GtkTreeIter iter;
  GtkTreeModel *model = (GtkTreeModel *)pointer;

  gtk_list_store_append(GTK_LIST_STORE(model), &iter);

  gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, NULL, 1, NULL, -1);
}

void gt_macros_remove_shortcut (GtkButton *button, gpointer pointer)
{
    GtkTreeIter iter;
    GtkTreeView *treeview = GTK_TREE_VIEW (pointer);
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);

    if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }
}

void gt_macros_save (GtkButton *button, gpointer pointer)
{
  GtkTreeIter iter;
  GtkTreeView *treeview = (GtkTreeView *)pointer;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  gint i = 0;

  remove_shortcuts();

  if(gtk_tree_model_get_iter_first(model, &iter))
    {
      do
	{
	  i++;
	} while(gtk_tree_model_iter_next(model, &iter));

      gtk_tree_model_get_iter_first(model, &iter);

      macros = g_malloc((i + 1) * sizeof(macro_t));
      i = 0;
      if(macros != NULL)
	{
	  do
	    {
	      gtk_tree_model_get(model, &iter, COLUMN_SHORTCUT, &(macros[i].shortcut), \
				 COLUMN_ACTION, &(macros[i].action), \
				 -1);
	      i++;
	    } while(gtk_tree_model_iter_next(model, &iter));

	  macros[i].shortcut = NULL;
	  macros[i].action = NULL;
	}
    }

  add_shortcuts();
}

void gt_macros_show_help (GtkButton *button, gpointer pointer)
{
    GtkWidget *Dialog;

    Dialog = gtk_message_dialog_new(GTK_WINDOW (pointer),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_CLOSE,
            _("The \"action\" field of a macro is the data to be sent on the port. Text can be entered, but also special chars, like \\n, \\t, \\r, etc. You can also enter hexadecimal data preceded by a '\\'. The hexadecimal data should not begin with a letter (eg. use \\0FF and not \\FF)\nExamples :\n\t\"Hello\\n\" sends \"Hello\" followed by a Line Feed\n\t\"Hello\\0A\" does the same thing but the LF is entered in hexadecimal"));

    gtk_dialog_run(GTK_DIALOG (Dialog));
    gtk_widget_destroy(Dialog);
}

static void accel_set_func (GtkTreeViewColumn *tree_column,
                            GtkCellRenderer   *cell,
                            GtkTreeModel      *model,
                            GtkTreeIter       *iter,
                            gpointer           data)
{
    char *keycode;
    guint keyval = 0;
    GdkModifierType mask = 0;

    gtk_tree_model_get (model, iter, 0, &keycode, -1);
    if (keycode != NULL) {
        gtk_accelerator_parse (keycode, &keyval, &mask);
    }

    g_object_set (cell,
                  "visible", TRUE,
                  "sensitive", TRUE,
                  "editable", TRUE,
                  "accel-key", keyval,
                  "accel-mods", mask,
                  NULL);
    g_free (keycode);
}

static void
accel_edited_callback (GtkCellRendererAccel *cell,
                       gchar                *path_string,
                       guint                 keyval,
                       GdkModifierType       mask,
                       guint                 hardware_keycode,
                       GtkTreeView          *view)
{
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    char *name = NULL;

    model = gtk_tree_view_get_model (view);

    path = gtk_tree_path_new_from_string (path_string);
    if (!path)
        return;

    if (!gtk_tree_model_get_iter (model, &iter, path)) {
        gtk_tree_path_free (path);

        return;
    }
    gtk_tree_path_free (path);

    name = gtk_accelerator_name (keyval, mask);

    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, name, -1);
    g_free (name);
}

static void
accel_cleared_callback (GtkCellRendererAccel *cell,
                        gchar                *path_string,
                        GtkTreeView          *view)
{
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (view);

    path = gtk_tree_path_new_from_string (path_string);
    if (!path)
        return;

    if (!gtk_tree_model_get_iter (model, &iter, path)) {
        gtk_tree_path_free (path);
        return;
    }
    gtk_tree_path_free (path);

    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, NULL, -1);
}

void Config_macros(GtkWindow *parent)
{
    GtkBuilder *builder = NULL;
    GtkWidget *treeview = NULL;
    GtkTreeModel *model = NULL;
    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *column = NULL;

    builder = gtk_builder_new_from_resource ("/org/jensge/GtkTerm/macros.ui");
    gtk_builder_connect_signals (builder, NULL);
    window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog-macros"));
    gtk_window_set_transient_for (GTK_WINDOW (window), parent);
    treeview = GTK_WIDGET (gtk_builder_get_object (builder, "treeview"));
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    create_model (GTK_LIST_STORE (model));
    renderer = GTK_CELL_RENDERER (gtk_builder_get_object (builder,
                                                          "cellrenderer_action"));
    g_signal_connect (renderer, "edited", G_CALLBACK (shortcut_edited), model);

    column = GTK_TREE_VIEW_COLUMN (gtk_builder_get_object (builder,
                                                           "column_shortcut"));
    renderer  = GTK_CELL_RENDERER (gtk_builder_get_object (builder,
                                                           "cellrenderer_shortcut"));
    gtk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);

    g_signal_connect (renderer, "accel-edited",
                      G_CALLBACK (accel_edited_callback), treeview);
    g_signal_connect (renderer, "accel-cleared",
                      G_CALLBACK (accel_cleared_callback), treeview);


    gtk_widget_show (GTK_WIDGET (window));
    g_object_unref (builder);
}

