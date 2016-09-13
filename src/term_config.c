/***********************************************************************/
/* term_config.c                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Configuration of the serial port                               */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.7 : Refactor to use newer gtk widgets                   */
/*                 Add ability to use arbitrary baud                   */
/*                 Add rs458 capability - Marc Le Douarain             */
/*                 Remove auto cr/lf stuff - (use macros instead)      */
/*      - 0.99.5 : Make the combo list for the device editable         */
/*      - 0.99.3 : Configuration for VTE terminal                      */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.1 : fixed memory management bug                         */
/*                 test if there are devices found                     */
/*      - 0.99.0 : fixed enormous memory management bug ;-)            */
/*                 save / read macros                                  */
/*      - 0.98.5 : font saved in configuration                         */
/*                 bug fixed in memory management                      */
/*                 combos set to non editable                          */
/*      - 0.98.3 : configuration file                                  */
/*      - 0.98.2 : autodetect existing devices                         */
/*      - 0.98 : added devfs devices                                   */
/*                                                                     */
/***********************************************************************/

#include "serial-port.h"
#include "term_config.h"
#include "widgets.h"
#include "parsecfg.h"
#include "macros.h"
#include "i18n.h"

#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <vte/vte.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define DEVICE_NUMBERS_TO_CHECK 12

extern GtSerialPort *serial_port;

static const gchar *devices_to_check[] = {
    "/dev/ttyS%d",
    "/dev/tts/%d",
    "/dev/ttyUSB%d",
    "/dev/ttyACM%d",
    "/dev/usb/tts/%d",
    NULL
};

/* Configuration file variables */
static gchar **port;
static gint *speed;
static gint *bits;
static gint *stopbits;
static gchar **parity;
static gchar **flow;
static gint *wait_delay;
static gint *wait_char;
static gint *rts_time_before_tx;
static gint *rts_time_after_tx;
static gint *echo;
static gint *crlfauto;
static cfgList **macro_list = NULL;
static gchar **font;

static gint *show_cursor;
static gint *rows;
static gint *columns;
static gint *scrollback;
static gint *visual_bell;
static gint *foreground_red;
static gint *foreground_blue;
static gint *foreground_green;
static gint *background_red;
static gint *background_blue;
static gint *background_green;


static cfgStruct cfg[] = {
    {"port", CFG_STRING, &port},
    {"speed", CFG_INT, &speed},
    {"bits", CFG_INT, &bits},
    {"stopbits", CFG_INT, &stopbits},
    {"parity", CFG_STRING, &parity},
    {"flow", CFG_STRING, &flow},
    {"wait_delay", CFG_INT, &wait_delay},
    {"wait_char", CFG_INT, &wait_char},
    {"rs485_rts_time_before_tx", CFG_INT, &rts_time_before_tx},
    {"rs485_rts_time_after_tx", CFG_INT, &rts_time_after_tx},
    {"echo", CFG_BOOL, &echo},
    {"crlfauto", CFG_BOOL, &crlfauto},
    {"font", CFG_STRING, &font},
    {"macros", CFG_STRING_LIST, &macro_list},
    {"term_show_cursor", CFG_BOOL, &show_cursor},
    {"term_rows", CFG_INT, &rows},
    {"term_columns", CFG_INT, &columns},
    {"term_scrollback", CFG_INT, &scrollback},
    {"term_visual_bell", CFG_BOOL, &visual_bell},
    {"term_foreground_red", CFG_INT, &foreground_red},
    {"term_foreground_blue", CFG_INT, &foreground_blue},
    {"term_foreground_green", CFG_INT, &foreground_green},
    {"term_background_red", CFG_INT, &background_red},
    {"term_background_blue", CFG_INT, &background_blue},
    {"term_background_green", CFG_INT, &background_green},
    {NULL, CFG_END, NULL}
};

static gchar *config_file = NULL;

struct configuration_port config;

typedef struct {
  gboolean show_cursor;
  gint rows;
  gint columns;
  gint scrollback;
  gboolean visual_bell;
  GdkRGBA foreground_color;
  GdkRGBA background_color;
  PangoFontDescription *font;
} display_config_t;

static display_config_t term_conf;

static GtkWidget *Entry;

static gint Grise_Degrise(GtkWidget *bouton, gpointer pointeur);
static void read_font_button(GtkFontButton *fontButton);
static void Hard_default_configuration(void);
static void Copy_configuration(int);

static void Select_config(gchar *, void *);
static void Save_config_file(void);
static void load_config(GtkDialog *, gint, GtkTreeSelection *);
static void delete_config(GtkDialog *, gint, GtkTreeSelection *);
static void save_config(GtkDialog *, gint, GtkWidget *);
static void really_save_config(GtkDialog *, gint, gpointer);
static gint remove_section(gchar *, gchar *);
static void Curseur_OnOff(GtkWidget *, gpointer);
static void Selec_couleur(GdkRGBA *, gfloat, gfloat, gfloat);
static void config_fg_color(GtkWidget *button, gpointer data);
static void config_bg_color(GtkWidget *button, gpointer data);
static void scrollback_set(GtkSpinButton *spin_button, gpointer data);

extern GtkWidget *display;

void Config_Port_Fenetre(GtkWindow *parent)
{
    GtkBuilder *builder;
    GtkDialog *dialog;
    GtkWidget *combo;
    GtkWidget *entry;
    GList *device_list = NULL;
    int device_list_length = 0;
    GList *it = NULL;
    char *rate = NULL;
    struct stat my_stat;
    int result = GTK_RESPONSE_CANCEL;

    const gchar **dev = NULL;
    guint i;

    for(dev = devices_to_check; *dev != NULL; dev++)
    {
        for(i = 0; i < DEVICE_NUMBERS_TO_CHECK; i++)
        {
            gchar *device_name = NULL;

            device_name = g_strdup_printf(*dev, i);
            if (stat(device_name, &my_stat) == 0) {
                device_list = g_list_prepend (device_list, device_name);
                device_list_length++;
            }
            else
                g_free (device_name);
        }
    }

    device_list = g_list_reverse (device_list);

    if (device_list == NULL)
    {
        show_message(_("No serial devices found!\n\n"
                    "Searched the following paths:\n"
                    "\t/dev/ttyS*\n\t/dev/tts/*\n\t/dev/ttyUSB*\n\t/dev/usb/tts/*\n\n"
                    "Enter a different device path in the 'Port' box.\n"), MSG_WRN);
    }

    builder = gtk_builder_new_from_resource ("/org/jensge/GtkTerm/settings-port.ui");
    dialog = GTK_DIALOG (gtk_builder_get_object (builder, "dialog-settings-port"));
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-device"));

    for (it = device_list; it != NULL; it = it->next) {
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                        (const gchar *) it->data);
    }

    g_list_free_full (device_list, g_free);

    /* Set values on first page */
    if (config.port != NULL && config.port[0] != '\0') {
        entry = gtk_bin_get_child (GTK_BIN (combo));

        gtk_entry_set_text (GTK_ENTRY (entry), config.port);
    } else {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
    }

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-baud-rate"));
    rate = g_strdup_printf ("%d", config.vitesse);
    entry = gtk_bin_get_child (GTK_BIN (combo));
    g_signal_connect (G_OBJECT (entry),
                      "insert-text",
                      G_CALLBACK (check_text_input),
                      isdigit);

    if (config.vitesse == 0) {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "9600");
    } else {
        if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), rate)) {
            gtk_entry_set_text (GTK_ENTRY (entry), rate);
        }
    }
    g_free (rate);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-parity"));
    rate = g_strdup_printf ("%d", config.parite);
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), rate);
    g_free (rate);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-bits"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (combo), config.bits);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-stopbits"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (combo), config.stops);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-flow"));
    rate = g_strdup_printf ("%d", config.flux);
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), rate);
    g_free (rate);

    /* Set values on second page */
    {
        GtkWidget *spin;
        spin = GTK_WIDGET (gtk_builder_get_object (builder, "spin-eol-delay"));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), (gfloat) config.delai);

        combo = GTK_WIDGET (gtk_builder_get_object (builder, "check-use-wait-char"));

        g_signal_connect (G_OBJECT (combo), "toggled", G_CALLBACK (Grise_Degrise), spin);

        Entry = combo = GTK_WIDGET (gtk_builder_get_object (builder, "entry-wait-char"));

        if (config.car != -1) {
            gtk_entry_set_text (GTK_ENTRY (combo), &(config.car));
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo), TRUE);
        }
    }

    /* Set values on third page */
    {
        combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-rs485-on"));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (combo),
                                   (gfloat) config.rs485_rts_time_before_transmit);

        combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-rs485-off"));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (combo),
                                   (gfloat) config.rs485_rts_time_after_transmit);
    }

    result = gtk_dialog_run (dialog);
    gtk_widget_hide (GTK_WIDGET (dialog));
    if (result == GTK_RESPONSE_OK) {
        Lis_Config (builder);
    }

    g_object_unref (builder);

    gtk_widget_destroy (GTK_WIDGET (dialog));
}

gint Lis_Config(GtkBuilder *builder)
{
    GObject *widget;
    gchar *message;

    widget = gtk_builder_get_object (builder, "combo-device");
    message = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));
    strcpy(config.port, message);
    g_free(message);

    widget = gtk_builder_get_object (builder, "combo-baud-rate");
    message = (char *) gtk_combo_box_get_active_id (GTK_COMBO_BOX (widget));
    config.vitesse = atoi(message);

    widget = gtk_builder_get_object (builder, "spin-bits");
    config.bits = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

    widget = gtk_builder_get_object (builder, "spin-eol-delay");
    config.delai = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

    widget = gtk_builder_get_object (builder, "spin-rs485-on");
    config.rs485_rts_time_before_transmit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

    widget = gtk_builder_get_object (builder, "spin-rs485-off");
    config.rs485_rts_time_after_transmit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

    widget = gtk_builder_get_object (builder, "combo-parity");
    message = (char *)gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
    config.parite = atoi (message);

    widget = gtk_builder_get_object (builder, "spin-bits");
    config.stops = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

    widget = gtk_builder_get_object (builder, "combo-flow");
    message = (char *)gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
    config.flux = atoi (message);

    widget = gtk_builder_get_object (builder, "check-use-wait-char");
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
    {
        widget = gtk_builder_get_object (builder, "entry-wait-char");
        config.car = *gtk_entry_get_text(GTK_ENTRY(widget));
        config.delai = 0;
    }
    else
        config.car = -1;

    gt_serial_port_config (serial_port);

    return FALSE;
}

gint Grise_Degrise(GtkWidget *bouton, gpointer pointeur)
{
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bouton)))
    {
	gtk_widget_set_sensitive(GTK_WIDGET(Entry), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(pointeur), FALSE);
    }
    else
    {
	gtk_widget_set_sensitive(GTK_WIDGET(Entry), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(pointeur), TRUE);
    }
    return FALSE;
}

void read_font_button(GtkFontButton *fontButton)
{
    const char *font_name = gtk_font_button_get_font_name (fontButton);
    PangoFontDescription *old_font = term_conf.font;

    if (font_name == NULL) {
        return;
    }

    term_conf.font = pango_font_description_from_string (font_name);
    if (term_conf.font != NULL) {
        g_clear_pointer (&old_font, pango_font_description_free);
        vte_terminal_set_font (VTE_TERMINAL(display), term_conf.font);
    } else {
        term_conf.font = old_font;
    }
}


void select_config_callback(GtkAction *action, gpointer data)
{
	Select_config(_("Load configuration"), G_CALLBACK(load_config));
}

void save_config_callback(GtkAction *action, gpointer data)
{
	Save_config_file();
}

void delete_config_callback(GtkAction *action, gpointer data)
{
	Select_config(_("Delete configuration"), G_CALLBACK(delete_config));
}

void Select_config(gchar *title, void *callback)
{
    GtkWidget *dialog;
    GtkWidget *content_area;
    gint i, max;

    GtkWidget *Frame, *Scroll, *Liste, *Label;
    gchar *texte_label;

    GtkListStore *Modele_Liste;
    GtkTreeIter iter_Liste;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *Colonne;
    GtkTreeSelection *Selection_Liste;

    enum
    {
	N_texte,
	N_COLONNES
    };

    /* Parse the config file */

    max = cfgParse(config_file, cfg, CFG_INI);

    if(max == -1)
    {
	show_message(_("Cannot read configuration file!\n"), MSG_ERR);
	return;
    }

    else
    {
	gchar *Titre[]= {_("Configurations")};

	dialog = gtk_dialog_new_with_buttons (title,
					      NULL,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
                          _("_Cancel"),
					      GTK_RESPONSE_NONE,
                          _("_OK"),
					      GTK_RESPONSE_ACCEPT,
					      NULL);

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	Modele_Liste = gtk_list_store_new(N_COLONNES, G_TYPE_STRING);

	Liste = gtk_tree_view_new_with_model(GTK_TREE_MODEL(Modele_Liste));
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(Liste), N_texte);

	Selection_Liste = gtk_tree_view_get_selection(GTK_TREE_VIEW(Liste));
	gtk_tree_selection_set_mode(Selection_Liste, GTK_SELECTION_SINGLE);

	Frame = gtk_frame_new(NULL);

	Scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(Frame), Scroll);
	gtk_container_add(GTK_CONTAINER(Scroll), Liste);

	renderer = gtk_cell_renderer_text_new();

	g_object_set(G_OBJECT(renderer), "xalign", (gfloat)0.5, NULL);
	Colonne = gtk_tree_view_column_new_with_attributes(Titre[0], renderer, "text", 0, NULL);
	gtk_tree_view_column_set_sort_column_id(Colonne, 0);

	Label=gtk_label_new("");
	texte_label = g_strdup_printf("<span weight=\"bold\" style=\"italic\">%s</span>", Titre[0]);
	gtk_label_set_markup(GTK_LABEL(Label), texte_label);
	g_free(texte_label);
	gtk_tree_view_column_set_widget(GTK_TREE_VIEW_COLUMN(Colonne), Label);
	gtk_widget_show(Label);

	gtk_tree_view_column_set_alignment(GTK_TREE_VIEW_COLUMN(Colonne), 0.5f);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(Colonne), FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(Liste), Colonne);


	for(i = 0; i < max; i++)
	{
	    gtk_list_store_append(Modele_Liste, &iter_Liste);
	    gtk_list_store_set(Modele_Liste, &iter_Liste, N_texte, cfgSectionNumberToName(i), -1);
	}

	gtk_widget_set_size_request(GTK_WIDGET(dialog), 200, 200);

	g_signal_connect(GTK_WIDGET(dialog), "response", G_CALLBACK (callback), GTK_TREE_SELECTION(Selection_Liste));
	g_signal_connect_swapped(GTK_WIDGET(dialog), "response", G_CALLBACK(gtk_widget_destroy), GTK_WIDGET(dialog));

	gtk_box_pack_start (GTK_BOX (content_area), Frame, TRUE, TRUE, 0);

	gtk_widget_show_all (dialog);
    }
}

void Save_config_file(void)
{
    GtkWidget *dialog, *content_area, *label, *box, *entry;

    dialog = gtk_dialog_new_with_buttons (_("Save configuration"),
					  NULL,
					  GTK_DIALOG_DESTROY_WITH_PARENT,
                      _("_Cancel"),
					  GTK_RESPONSE_NONE,
                      _("_OK"),
					  GTK_RESPONSE_ACCEPT,
					  NULL);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG(dialog));

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    label = gtk_label_new(_("Configuration name: "));

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), "default");
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);

    //validate input text (alpha-numeric only)
    g_signal_connect(GTK_ENTRY(entry),
		     "insert-text",
		     G_CALLBACK(check_text_input), isalnum);
    g_signal_connect(GTK_WIDGET(dialog), "response", G_CALLBACK(save_config), GTK_ENTRY(entry));
    g_signal_connect_swapped(GTK_WIDGET(dialog), "response", G_CALLBACK(gtk_widget_destroy), GTK_WIDGET(dialog));

    gtk_box_pack_start(GTK_BOX(content_area), box, TRUE, FALSE, 5);

    gtk_widget_show_all (dialog);
}

void really_save_config(GtkDialog *dialog, gint response_id, gpointer data)
{
    int max, cfg_num, i;
    gchar *string = NULL;

    cfg_num = -1;

    if(response_id == GTK_RESPONSE_ACCEPT)
    {
	max = cfgParse(config_file, cfg, CFG_INI);

	if(max == -1)
	    return;

	for(i = 0; i < max; i++)
	{
	    if(!strcmp((char *)data, cfgSectionNumberToName(i)))
		cfg_num = i;
	}

	/* not overwriting */
	if(cfg_num == -1)
	{
	    max = cfgAllocForNewSection(cfg, (char *)data);
	    cfg_num = max - 1;
	}
	else
	{
	    if(remove_section(config_file, (char *)data) == -1)
	    {
		show_message(_("Cannot overwrite section!"), MSG_ERR);
		return;
	    }
	    if(max == cfgParse(config_file, cfg, CFG_INI))
	    {
		show_message(_("Cannot read configuration file!"), MSG_ERR);
		return;
	    }
	    max = cfgAllocForNewSection(cfg, (char *)data);
	    cfg_num = max - 1;
	}

	Copy_configuration(cfg_num);
	cfgDump(config_file, cfg, CFG_INI, max);

	string = g_strdup_printf(_("Configuration [%s] saved\n"), (char *)data);
	show_message(string, MSG_WRN);
	g_free(string);
    }
    else
	Save_config_file();
}

void save_config(GtkDialog *dialog, gint response_id, GtkWidget *edit)
{
	int max, i;
	const gchar *config_name;

	if(response_id == GTK_RESPONSE_ACCEPT)
	{
		max = cfgParse(config_file, cfg, CFG_INI);

		if(max == -1)
			return;

		config_name = gtk_entry_get_text(GTK_ENTRY(edit));

		for(i = 0; i < max; i++)
		{
			if(!strcmp(config_name, cfgSectionNumberToName(i)))
			{
				GtkWidget *message_dialog;
				message_dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(dialog),
				                     GTK_DIALOG_DESTROY_WITH_PARENT,
				                     GTK_MESSAGE_QUESTION,
				                     GTK_BUTTONS_NONE,
				                     _("<b>Section [%s] already exists.</b>\n\nDo you want to overwrite it ?"),
				                     config_name);

				gtk_dialog_add_buttons(GTK_DIALOG(message_dialog),
                                       _("_Cancel"),
				                       GTK_RESPONSE_NONE,
                                       _("_OK"),
				                       GTK_RESPONSE_ACCEPT,
				                       NULL);

				if (gtk_dialog_run(GTK_DIALOG(message_dialog)) == GTK_RESPONSE_ACCEPT)
					really_save_config(NULL, GTK_RESPONSE_ACCEPT, (gpointer)config_name);

				gtk_widget_destroy(message_dialog);

				i = max + 1;
			}
		}
		if(i == max) /* Section does not exist */
			really_save_config(NULL, GTK_RESPONSE_ACCEPT, (gpointer)config_name);
	}
}

void load_config(GtkDialog *dialog, gint response_id, GtkTreeSelection *Selection_Liste)
{
    GtkTreeIter iter;
    GtkTreeModel *Modele;
    gchar *txt;

    if(response_id == GTK_RESPONSE_ACCEPT)
    {
	if(gtk_tree_selection_get_selected(Selection_Liste, &Modele, &iter))
	{
	    gtk_tree_model_get(GTK_TREE_MODEL(Modele), &iter, 0, (gint *)&txt, -1);
	    Load_configuration_from_file(txt);
	    Verify_configuration();
        gt_serial_port_config (serial_port);
	    add_shortcuts();
	}
    }
}

void delete_config(GtkDialog *dialog, gint response_id, GtkTreeSelection *Selection_Liste)
{
    GtkTreeIter iter;
    GtkTreeModel *Modele;
    gchar *txt;

    if(response_id == GTK_RESPONSE_ACCEPT)
    {
	if(gtk_tree_selection_get_selected(Selection_Liste, &Modele, &iter))
	{
	    gtk_tree_model_get(GTK_TREE_MODEL(Modele), &iter, 0, (gint *)&txt, -1);
	    if(remove_section(config_file, txt) == -1)
		show_message(_("Cannot delete section!"), MSG_ERR);
	}
    }
}

gint Load_configuration_from_file(const gchar *config_name)
{
    int max, i, j, size;
    size_t k;
    gchar *string = NULL;
    gchar *str;
    macro_t *macros = NULL;
    cfgList *t;

    max = cfgParse(config_file, cfg, CFG_INI);

    if(max == -1)
	return -1;

    else
    {
	for(i = 0; i < max; i++)
	{
	    if(!strcmp(config_name, cfgSectionNumberToName(i)))
	    {
		Hard_default_configuration();

		if(port[i] != NULL)
		    strcpy(config.port, port[i]);
		if(speed[i] != 0)
		    config.vitesse = speed[i];
		if(bits[i] != 0)
		    config.bits = bits[i];
		if(stopbits[i] != 0)
		    config.stops = stopbits[i];
		if(parity[i] != NULL)
		{
		    if(!g_ascii_strcasecmp(parity[i], "none"))
			config.parite = 0;
		    else if(!g_ascii_strcasecmp(parity[i], "odd"))
			config.parite = 1;
		    else if(!g_ascii_strcasecmp(parity[i], "even"))
			config.parite = 2;
		}
		if(flow[i] != NULL)
		{
		    if(!g_ascii_strcasecmp(flow[i], "none"))
			config.flux = 0;
		    else if(!g_ascii_strcasecmp(flow[i], "xon"))
			config.flux = 1;
		    else if(!g_ascii_strcasecmp(flow[i], "rts"))
			config.flux = 2;
		    else if(!g_ascii_strcasecmp(flow[i], "rs485"))
			config.flux = 3;
		}

		config.delai = wait_delay[i];

		if(wait_char[i] != 0)
		    config.car = (signed char)wait_char[i];
		else
		    config.car = -1;

		config.rs485_rts_time_before_transmit = rts_time_before_tx[i];
		config.rs485_rts_time_after_transmit = rts_time_after_tx[i];

		if(echo[i] != -1)
		    config.echo = (gboolean)echo[i];
		else
		    config.echo = FALSE;

		if(crlfauto[i] != -1)
		    config.crlfauto = (gboolean)crlfauto[i];
		else
		    config.crlfauto = FALSE;

        g_clear_pointer (&term_conf.font, pango_font_description_free);
        term_conf.font = pango_font_description_from_string (font[i]);

		t = macro_list[i];
		size = 0;
		if(t != NULL)
		{
		    size++;
		    while(t->next != NULL)
		    {
			t = t->next;
			size++;
		    }
		}

		if(size != 0)
		{
		    t = macro_list[i];
		    macros = g_malloc(size * sizeof(macro_t));
		    if(macros == NULL)
		    {
			perror("malloc");
			return -1;
		    }
		    for(j = 0; j < size; j++)
		    {
			for(k = 0; k < (strlen(t->str) - 1); k++)
			{
			    if((t->str[k] == ':') && (t->str[k + 1] == ':'))
				break;
			}
			macros[j].shortcut = g_strndup(t->str, k);
			str = &(t->str[k + 2]);
			macros[j].action = g_strdup(str);

			t = t->next;
		    }
		}

		remove_shortcuts();
		create_shortcuts(macros, size);
		g_free(macros);

		if(show_cursor[i] != -1)
		    term_conf.show_cursor = (gboolean)show_cursor[i];
		else
		    term_conf.show_cursor = FALSE;

		if(rows[i] != 0)
		    term_conf.rows = rows[i];

		if(columns[i] != 0)
		    term_conf.columns = columns[i];

		if(scrollback[i] != 0)
		    term_conf.scrollback = scrollback[i];

		if(visual_bell[i] != -1)
		    term_conf.visual_bell = (gboolean)visual_bell[i];
		else
		    term_conf.visual_bell = FALSE;

		term_conf.foreground_color.red = (double) foreground_red[i] / G_MAXUINT16;
		term_conf.foreground_color.green = (double) foreground_green[i] / G_MAXUINT16;
		term_conf.foreground_color.blue = (double) foreground_blue[i] / G_MAXUINT16;

		term_conf.background_color.red = (double) background_red[i] / G_MAXUINT16;
		term_conf.background_color.green = (double) background_green[i] / G_MAXUINT16;
		term_conf.background_color.blue = (double) background_blue[i] / G_MAXUINT16;

		/* rows and columns are empty when the conf is autogenerate in the
		   first save; so set term to default */
		if(rows[i] == 0 || columns[i] == 0)
		{
		    term_conf.show_cursor = TRUE;
		    term_conf.rows = 80;
		    term_conf.columns = 25;
		    term_conf.scrollback = DEFAULT_SCROLLBACK;
		    term_conf.visual_bell = FALSE;

		    term_conf.foreground_color.red = 43253;
		    term_conf.foreground_color.green = 43253;
		    term_conf.foreground_color.blue = 43253;

		    term_conf.background_color.red = 0;
		    term_conf.background_color.green = 0;
		    term_conf.background_color.blue = 0;
		}

		i = max + 1;
	    }
	}
	if(i == max)
	{
	    string = g_strdup_printf(_("No section \"%s\" in configuration file\n"), config_name);
	    show_message(string, MSG_ERR);
	    g_free(string);
	    return -1;
	}
    }

    vte_terminal_set_font (VTE_TERMINAL(display), term_conf.font);

    vte_terminal_set_size (VTE_TERMINAL(display), term_conf.rows, term_conf.columns);
    vte_terminal_set_scrollback_lines (VTE_TERMINAL(display), term_conf.scrollback);
    vte_terminal_set_color_foreground (VTE_TERMINAL(display), (const GdkRGBA *)&term_conf.foreground_color);
    vte_terminal_set_color_background (VTE_TERMINAL(display), (const GdkRGBA *)&term_conf.background_color);
    gtk_widget_queue_draw(display);

    return 0;
}

void Verify_configuration(void)
{
    gchar *string = NULL;

    switch(config.vitesse)
    {
	case 300:
	case 600:
	case 1200:
	case 2400:
	case 4800:
	case 9600:
	case 19200:
	case 38400:
	case 57600:
	case 115200:
	    break;

	default:
	    string = g_strdup_printf(_("Unknown rate: %d baud\nMay not be supported by all hardware"), config.vitesse);
	    show_message(string, MSG_ERR);
	    g_free(string);
    }

    if(config.stops != 1 && config.stops != 2)
    {
	string = g_strdup_printf(_("Invalid number of stop-bits: %d\nFalling back to default number of stop-bits number: %d\n"), config.stops, DEFAULT_STOP);
	show_message(string, MSG_ERR);
	config.stops = DEFAULT_STOP;
	g_free(string);
    }

    if(config.bits < 5 || config.bits > 8)
    {
	string = g_strdup_printf(_("Invalid number of bits: %d\nFalling back to default number of bits: %d\n"), config.bits, DEFAULT_BITS);
	show_message(string, MSG_ERR);
	config.bits = DEFAULT_BITS;
	g_free(string);
    }

    if(config.delai < 0 || config.delai > 500)
    {
	string = g_strdup_printf(_("Invalid delay: %d ms\nFalling back to default delay: %d ms\n"), config.delai, DEFAULT_DELAY);
	show_message(string, MSG_ERR);
	config.delai = DEFAULT_DELAY;
	g_free(string);
    }

    if(term_conf.font == NULL) {
        term_conf.font = pango_font_description_from_string (DEFAULT_FONT);
    }

}

gint Check_configuration_file(void)
{
    struct stat my_stat;
    gchar *string = NULL;

    /* is configuration file present ? */
    if(stat(config_file, &my_stat) == 0)
    {
	/* If bad configuration file, fallback to _hardcoded_ defaults! */
	if(Load_configuration_from_file("default") == -1)
	{
	    Hard_default_configuration();
	    return -1;
	}
    }

    /* if not, create it, with the [default] section */
    else
    {
	string = g_strdup_printf(_("Configuration file (%s) with\n[default] configuration has been created.\n"), config_file);
	show_message(string, MSG_WRN);
	cfgAllocForNewSection(cfg, "default");
	Hard_default_configuration();
	Copy_configuration(0);
	cfgDump(config_file, cfg, CFG_INI, 1);
	g_free(string);
    }
    return 0;
}

void Hard_default_configuration(void)
{
    strcpy(config.port, DEFAULT_PORT);
    config.vitesse = DEFAULT_SPEED;
    config.parite = DEFAULT_PARITY;
    config.bits = DEFAULT_BITS;
    config.stops = DEFAULT_STOP;
    config.flux = DEFAULT_FLOW;
    config.delai = DEFAULT_DELAY;
    config.rs485_rts_time_before_transmit = DEFAULT_DELAY_RS485;
    config.rs485_rts_time_after_transmit = DEFAULT_DELAY_RS485;
    config.car = DEFAULT_CHAR;
    config.echo = DEFAULT_ECHO;
    config.crlfauto = FALSE;

    term_conf.font = pango_font_description_from_string (DEFAULT_FONT);

    term_conf.show_cursor = TRUE;
    term_conf.rows = 80;
    term_conf.columns = 25;
    term_conf.scrollback = DEFAULT_SCROLLBACK;
    term_conf.visual_bell = TRUE;

    Selec_couleur(&term_conf.foreground_color, 0.66, 0.66, 0.66);
    Selec_couleur(&term_conf.background_color, 0, 0, 0);
}

void Copy_configuration(int pos)
{
    gchar *string = NULL;
    macro_t *macros = NULL;
    gint size, i;

    string = g_strdup(config.port);
    cfgStoreValue(cfg, "port", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%d", config.vitesse);
    cfgStoreValue(cfg, "speed", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%d", config.bits);
    cfgStoreValue(cfg, "bits", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%d", config.stops);
    cfgStoreValue(cfg, "stopbits", string, CFG_INI, pos);
    g_free(string);

    switch(config.parite)
    {
	case 0:
	    string = g_strdup_printf("none");
	    break;
	case 1:
	    string = g_strdup_printf("odd");
	    break;
	case 2:
	    string = g_strdup_printf("even");
	    break;
	default:
	    string = g_strdup_printf("none");
    }
    cfgStoreValue(cfg, "parity", string, CFG_INI, pos);
    g_free(string);

    switch(config.flux)
    {
	case 0:
	    string = g_strdup_printf("none");
	    break;
	case 1:
	    string = g_strdup_printf("xon");
	    break;
	case 2:
	    string = g_strdup_printf("rts");
	    break;
	case 3:
	    string = g_strdup_printf("rs485");
	    break;
	default:
	    string = g_strdup_printf("none");
    }

    cfgStoreValue(cfg, "flow", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%d", config.delai);
    cfgStoreValue(cfg, "wait_delay", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%d", config.car);
    cfgStoreValue(cfg, "wait_char", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%d", config.rs485_rts_time_before_transmit);
    cfgStoreValue(cfg, "rs485_rts_time_before_tx", string, CFG_INI, pos);
    g_free(string);
    string = g_strdup_printf("%d", config.rs485_rts_time_after_transmit);
    cfgStoreValue(cfg, "rs485_rts_time_after_tx", string, CFG_INI, pos);
    g_free(string);

    if(config.echo == FALSE)
	string = g_strdup_printf("False");
    else
	string = g_strdup_printf("True");

    cfgStoreValue(cfg, "echo", string, CFG_INI, pos);
    g_free(string);

    if(config.crlfauto == FALSE)
	string = g_strdup_printf("False");
    else
	string = g_strdup_printf("True");

    cfgStoreValue(cfg, "crlfauto", string, CFG_INI, pos);
    g_free(string);

    string = pango_font_description_to_string (term_conf.font);
    cfgStoreValue(cfg, "font", string, CFG_INI, pos);
    g_free(string);

    macros = get_shortcuts(&size);
    for(i = 0; i < size; i++)
    {
	string = g_strdup_printf("%s::%s", macros[i].shortcut, macros[i].action);
	cfgStoreValue(cfg, "macros", string, CFG_INI, pos);
	g_free(string);
    }

    if(term_conf.show_cursor == FALSE)
	string = g_strdup_printf("False");
    else
	string = g_strdup_printf("True");
    cfgStoreValue(cfg, "term_show_cursor", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%d", term_conf.rows);
    cfgStoreValue(cfg, "term_rows", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%d", term_conf.columns);
    cfgStoreValue(cfg, "term_columns", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%d", term_conf.scrollback);
    cfgStoreValue(cfg, "term_scrollback", string, CFG_INI, pos);
    g_free(string);

    if(term_conf.visual_bell == FALSE)
	string = g_strdup_printf("False");
    else
	string = g_strdup_printf("True");
    cfgStoreValue(cfg, "term_visual_bell", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%u", (guint16) (term_conf.foreground_color.red * G_MAXUINT16));
    cfgStoreValue(cfg, "term_foreground_red", string, CFG_INI, pos);
    g_free(string);
    string = g_strdup_printf("%u", (guint16) (term_conf.foreground_color.green * G_MAXUINT16));
    cfgStoreValue(cfg, "term_foreground_green", string, CFG_INI, pos);
    g_free(string);
    string = g_strdup_printf("%u", (guint16) (term_conf.foreground_color.blue * G_MAXUINT16));
    cfgStoreValue(cfg, "term_foreground_blue", string, CFG_INI, pos);
    g_free(string);

    string = g_strdup_printf("%u", (guint16) (term_conf.background_color.red * G_MAXUINT16));
    cfgStoreValue(cfg, "term_background_red", string, CFG_INI, pos);
    g_free(string);
    string = g_strdup_printf("%u", (guint16) (term_conf.background_color.green * G_MAXUINT16));
    cfgStoreValue(cfg, "term_background_green", string, CFG_INI, pos);
    g_free(string);
    string = g_strdup_printf("%u", (guint16) (term_conf.background_color.blue * G_MAXUINT16));
    cfgStoreValue(cfg, "term_background_blue", string, CFG_INI, pos);
    g_free(string);
}


gint remove_section(gchar *cfg_file, gchar *section)
{
    FILE *f = NULL;
    char *buffer = NULL;
    char *buf;
    size_t size;
    gchar *to_search;
    size_t i, j, length, sect;

    f = fopen(cfg_file, "r");
    if(f == NULL)
    {
	perror(cfg_file);
	return -1;
    }

    fseek(f, 0L, SEEK_END);
    size = ftell(f);
    rewind(f);

    buffer = g_malloc(size);
    if(buffer == NULL)
    {
	perror("malloc");
	return -1;
    }

    if(fread(buffer, 1, size, f) != size)
    {
	perror(cfg_file);
	fclose(f);
	return -1;
    }

    to_search = g_strdup_printf("[%s]", section);
    length = strlen(to_search);

    /* Search section */
    for(i = 0; i < size - length; i++)
    {
	for(j = 0; j < length; j++)
	{
	    if(to_search[j] != buffer[i + j])
		break;
	}
	if(j == length)
	    break;
    }

    if(i == size - length)
    {
	i18n_printf(_("Cannot find section %s\n"), to_search);
	return -1;
    }

    sect = i;

    /* Search for next section */
    for(i = sect + length; i < size; i++)
    {
	if(buffer[i] == '[')
	    break;
    }

    f = fopen(cfg_file, "w");
    if(f == NULL)
    {
	perror(cfg_file);
	return -1;
    }

    fwrite(buffer, 1, sect, f);
    buf = buffer + i;
    fwrite(buf, 1, size - i, f);
    fclose(f);

    g_free(to_search);
    g_free(buffer);

    return 0;
}

void Config_Terminal(GtkAction *action, gpointer data)
{
    GtkWidget *dialog;
    GtkWidget *font_button;
    GtkWidget *check_box;
    GtkWidget *fg_color_button;
    GtkWidget *bg_color_button;
    GtkWidget *scrollback_spin;

    GtkBuilder *builder;

    builder = gtk_builder_new_from_resource ("/org/jensge/GtkTerm/settings-window.ui");

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog-settings-window"));

    font_button = GTK_WIDGET (gtk_builder_get_object (builder, "font-button"));
    gtk_font_chooser_set_font_desc (GTK_FONT_CHOOSER (font_button), term_conf.font);
    g_signal_connect(GTK_WIDGET(font_button), "font-set", G_CALLBACK(read_font_button), 0);

    check_box = GTK_WIDGET (gtk_builder_get_object (builder, "check-show-cursor"));
    g_signal_connect(GTK_WIDGET(check_box), "toggled", G_CALLBACK(Curseur_OnOff), 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_box), term_conf.show_cursor);

    fg_color_button = GTK_WIDGET (gtk_builder_get_object (builder, "color-button-fg"));
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (fg_color_button), &term_conf.foreground_color);
    g_signal_connect (GTK_WIDGET (fg_color_button), "color-set", G_CALLBACK (config_fg_color), NULL);

    bg_color_button = GTK_WIDGET (gtk_builder_get_object (builder, "color-button-bg"));
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (bg_color_button), &term_conf.background_color);
    g_signal_connect (GTK_WIDGET (bg_color_button), "color-set", G_CALLBACK (config_bg_color), NULL);

    scrollback_spin = GTK_WIDGET (gtk_builder_get_object (builder, "spin-scrollback"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (scrollback_spin), term_conf.scrollback);
    g_signal_connect (G_OBJECT(scrollback_spin), "value-changed", G_CALLBACK(scrollback_set), NULL);

    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_hide (dialog);
    gtk_widget_destroy (dialog);
    g_object_unref (builder);
}

void Curseur_OnOff(GtkWidget *Check_Bouton, gpointer data)
{
    term_conf.show_cursor = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Check_Bouton));
}

void Selec_couleur(GdkRGBA *color, gfloat R, gfloat G, gfloat B)
{
	color->red = R;
	color->green = G;
	color->blue = B;
    color->alpha = 1.0;
}

void config_fg_color(GtkWidget *button, gpointer data)
{
	gchar *string;

	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &term_conf.foreground_color);

	vte_terminal_set_color_foreground (VTE_TERMINAL(display), (const GdkRGBA *)&term_conf.foreground_color);
	gtk_widget_queue_draw (display);

	string = g_strdup_printf ("%u", (guint16) (term_conf.foreground_color.red * G_MAXUINT16));
	cfgStoreValue (cfg, "term_foreground_red", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", (guint16) (term_conf.foreground_color.green * G_MAXUINT16));
	cfgStoreValue (cfg, "term_foreground_green", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", (guint16) (term_conf.foreground_color.blue * G_MAXUINT16));
	cfgStoreValue (cfg, "term_foreground_blue", string, CFG_INI, 0);
	g_free (string);
}

void config_bg_color(GtkWidget *button, gpointer data)
{
	gchar *string;

	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &term_conf.background_color);

	vte_terminal_set_color_background (VTE_TERMINAL(display), (const GdkRGBA *)&term_conf.background_color);
	gtk_widget_queue_draw (display);

	string = g_strdup_printf ("%u", (guint16) (term_conf.background_color.red * G_MAXUINT16));
	cfgStoreValue (cfg, "term_background_red", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", (guint16) (term_conf.background_color.green * G_MAXUINT16));
	cfgStoreValue (cfg, "term_background_green", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", (guint16) (term_conf.background_color.blue * G_MAXUINT16));
	cfgStoreValue (cfg, "term_background_blue", string, CFG_INI, 0);
	g_free (string);
}


void scrollback_set(GtkSpinButton *spin_button, gpointer data)
{
    int scrollback_value = gtk_spin_button_get_value_as_int (spin_button);
    term_conf.scrollback = scrollback_value;
    vte_terminal_set_scrollback_lines (VTE_TERMINAL(display), term_conf.scrollback);
}

/**
 *  Filter user data entry on a GTK entry
 *
 *  user_data must be a function that takes an int and returns an int
 *  != 0 if the input is valid.  For instance, 'isdigit()'.
 */
void check_text_input(GtkEditable *editable,
                      gchar       *new_text,
                      gint         new_text_length,
                      gint        *position,
                      gpointer     user_data)
{
    int i;
    int (*check_func)(int) = NULL;

    if(user_data == NULL)
    {
        return;
    }

    g_signal_handlers_block_by_func(editable,
                                    (gpointer)check_text_input, user_data);
    check_func = (int (*)(int))user_data;

    for(i = 0; i < new_text_length; i++)
    {
        if(!check_func(new_text[i]))
            goto invalid_input;
    }

    gtk_editable_insert_text(editable, new_text, new_text_length, position);

invalid_input:
    g_signal_handlers_unblock_by_func(editable,
                                      (gpointer)check_text_input, user_data);
    g_signal_stop_emission_by_name(editable, "insert-text");
}

const char *gt_config_get_file_path (void)
{
    return config_file;
}

void gt_config_set_file_path (const char *path)
{
    g_free (config_file);
    config_file = g_strdup (path);
}
