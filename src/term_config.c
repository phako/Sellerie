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
#define DEFAULT_FONT "Monospace, 12"
#define DEFAULT_SCROLLBACK 200

#define DEFAULT_PORT "/dev/ttyS0"
#define DEFAULT_SPEED 9600
#define DEFAULT_PARITY 0
#define DEFAULT_BITS 8
#define DEFAULT_STOP 1
#define DEFAULT_FLOW 0
#define DEFAULT_DELAY 0
#define DEFAULT_CHAR -1
#define DEFAULT_DELAY_RS485 30
#define DEFAULT_ECHO FALSE

#define GT_SETTINGS_DEFAULT_ID "149e7525-24af-448f-a3a7-1b32d0930486"
#define GT_SETTINGS_PROFILE_SCHEMA "org.jensge.GtkTerm.Profile"
#define GT_SETTINGS_SCHEMA "org.jensge.GtkTerm"
#define GT_SETTINGS_PROFILE_TEMPLATE "/org/jensge/GtkTerm/profiles/%s/"

void check_text_input(GtkEditable *editable,
		       gchar       *new_text,
		       gint         new_text_length,
		       gint        *position,
		       gpointer     user_data);


extern GtSerialPort *serial_port;

static const gchar *devices_to_check[] = {
    "/dev/ttyS%d",
    "/dev/tts/%d",
    "/dev/ttyUSB%d",
    "/dev/ttyACM%d",
    "/dev/usb/tts/%d",
    NULL
};

static gchar *config_file = NULL;

typedef struct {
  gint rows;
  gint columns;
  gint scrollback;
  gboolean visual_bell;
  GdkRGBA foreground_color;
  GdkRGBA background_color;
  PangoFontDescription *font;
} display_config_t;

static display_config_t term_conf;

static void read_font_button(GtkFontButton *fontButton);
static void Select_config(gchar *, void *);
static void Save_config_file(void);
static void load_config(GtkDialog *, gint, GtkTreeSelection *);
static void delete_config(GtkDialog *, gint, GtkTreeSelection *);
static void save_config(GtkDialog *, gint, GtkWidget *);
static void config_fg_color(GtkWidget *button, gpointer data);
static void config_bg_color(GtkWidget *button, gpointer data);
static void scrollback_set(GtkSpinButton *spin_button, gpointer data);
void gt_config_edit_profile (GtkWindow *parent, const char *profile);

/* GSettings binding mapping functions */
static gboolean int_to_str (GValue *value,
                            GVariant *data,
                            gpointer user_data);
static GVariant *str_to_int (const GValue *value,
                             const GVariantType *type,
                             gpointer user_data);

static gboolean speed_to_str (GValue *value,
                              GVariant *data,
                              gpointer user_data);
static GVariant *str_to_speed (const GValue *value,
                               const GVariantType *type,
                               gpointer user_data);

extern GtkWidget *display;

void Config_Port_Fenetre(GtkWindow *parent)
{
    gt_config_edit_profile (parent, GT_SETTINGS_DEFAULT_ID);
}

static gboolean int_to_str (GValue *value, GVariant *data, gpointer user_data)
{
    int val = -1;
    char *str = NULL;

    val = g_variant_get_int32 (data);
    if (val == -1)
    {
        str = g_strdup ("");
    }
    else
    {
        str = g_strdup_printf ("%c", (char) g_variant_get_int32 (data));
    }

    g_value_take_string (value, str);

    return TRUE;
}

static GVariant *str_to_int (const GValue *value,
                             const GVariantType *type,
                             gpointer user_data)
{
    const char *str = g_value_get_string (value);

    return g_variant_new_int32 (str == NULL ? 0 : (int) str[0]);
}

static gboolean speed_to_str (GValue *value, GVariant *data, gpointer user_data)
{
    g_value_take_string (value, g_strdup_printf ("%d", g_variant_get_int32 (data)));

    return TRUE;
}

static GVariant *str_to_speed (const GValue *value,
                               const GVariantType *type,
                               gpointer user_data)
{
    return g_variant_new_int32 (atoi (g_value_get_string (value)));
}

void gt_config_edit_profile (GtkWindow *parent, const char *profile)
{
    GSettings *settings;
    GtkBuilder *builder;
    GtkDialog *dialog;
    GtkWidget *combo;
    GtkWidget *entry;
    GList *device_list = NULL;
    int device_list_length = 0;
    GList *it = NULL;
    struct stat my_stat;
    int result = GTK_RESPONSE_CANCEL;

    const gchar **dev = NULL;
    guint i;

    settings = gt_config_get_settings_for_profile (profile);
    g_settings_delay (settings);


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

    entry = gtk_bin_get_child (GTK_BIN (combo));
    g_settings_bind (settings, "port", entry, "text", G_SETTINGS_BIND_DEFAULT);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-baud-rate"));
    entry = gtk_bin_get_child (GTK_BIN (combo));
    g_signal_connect (G_OBJECT (entry),
                      "insert-text",
                      G_CALLBACK (check_text_input),
                      isdigit);

    g_settings_bind_with_mapping (settings,
                                  "speed",
                                  entry,
                                  "text",
                                  G_SETTINGS_BIND_DEFAULT,
                                  speed_to_str,
                                  str_to_speed,
                                  NULL, NULL);


    if (g_settings_get_int (settings, "speed") == 0) {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "115200");
    }

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-parity"));
    g_settings_bind (settings, "parity", combo, "active-id", G_SETTINGS_BIND_DEFAULT);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-bits"));
    g_settings_bind (settings, "bits", combo, "value", G_SETTINGS_BIND_DEFAULT);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-stopbits"));
    g_settings_bind (settings, "stopbits", combo, "value", G_SETTINGS_BIND_DEFAULT);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-flow"));
    g_settings_bind (settings, "flow", combo, "active-id", G_SETTINGS_BIND_DEFAULT);

    /* Set values on second page */
    {
        combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-eol-delay"));
        g_settings_bind (settings, "wait-delay", combo, "value", G_SETTINGS_BIND_DEFAULT);
        combo = GTK_WIDGET (gtk_builder_get_object (builder, "check-use-wait-char"));
        entry = GTK_WIDGET (gtk_builder_get_object (builder, "entry-wait-char"));

        g_object_bind_property (G_OBJECT (combo), "active",
                                entry,
                                "sensitive", 0);
        g_object_bind_property (G_OBJECT (combo), "active",
                                entry,
                                "sensitive", G_BINDING_INVERT_BOOLEAN);

        g_settings_bind_with_mapping (settings, "wait-character",
                                      entry,
                                      "text",
                                      G_SETTINGS_BIND_DEFAULT,
                                      int_to_str,
                                      str_to_int,
                                      NULL, NULL);

        if (g_settings_get_int (settings, "wait-character") != -1) {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo), TRUE);
        }
    }

    /* Set values on third page */
    {
        combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-rs485-on"));
        g_settings_bind (settings, "rs485-pre-tx-rts-delay", combo, "value", G_SETTINGS_BIND_DEFAULT);

        combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-rs485-off"));
        g_settings_bind (settings, "rs485-post-tx-rts-delay", combo, "value", G_SETTINGS_BIND_DEFAULT);
    }

    result = gtk_dialog_run (dialog);
    gtk_widget_hide (GTK_WIDGET (dialog));
    if (result == GTK_RESPONSE_OK) {
        g_settings_apply (settings);
        gt_serial_port_config (serial_port, profile);
    }

    g_object_unref (builder);
    g_object_unref (settings);
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
    gint max;

    GtkWidget *scrolled, *tree_view, *label;
    char *label_text;

    GtkListStore *list_model;
    GtkTreeIter tree_iter;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *tree_selection;
    GSettings *settings;
    GVariant *value;
    GVariantIter *iter;
    char *profile;

    /* Parse the config file */
    settings = g_settings_new ("org.jensge.GtkTerm");
    value = g_settings_get_value (settings, "profiles");
    iter = g_variant_iter_new (value);
    max = g_variant_iter_n_children (iter);

    if(max == -1)
    {
        show_message(_("Cannot read configuration file!\n"), MSG_ERR);
        return;
    }

    dialog = gtk_dialog_new_with_buttons (_("Configurations"),
                                          NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          _("_Cancel"),
                                          GTK_RESPONSE_NONE,
                                          _("_OK"),
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG(dialog));

    list_model = gtk_list_store_new (1, G_TYPE_STRING);

    tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_model));
    gtk_tree_view_set_search_column (GTK_TREE_VIEW (tree_view), 0);

    tree_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
    gtk_tree_selection_set_mode (tree_selection, GTK_SELECTION_SINGLE);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scrolled), tree_view);

    renderer = gtk_cell_renderer_text_new ();

    column = gtk_tree_view_column_new_with_attributes(_("Configurations"),
                                                      renderer,
                                                      "text",
                                                      0,
                                                      NULL);
    gtk_tree_view_column_set_sort_column_id (column, 0);

    label = gtk_label_new ("");
    label_text = g_strdup_printf ("<span weight=\"bold\" style=\"italic\">%s</span>",
                                  _("Configurations"));
    gtk_label_set_markup (GTK_LABEL (label), label_text);
    g_free (label_text);
    gtk_tree_view_column_set_widget (GTK_TREE_VIEW_COLUMN (column), label);
    gtk_widget_show(label);

    gtk_tree_view_column_set_alignment (GTK_TREE_VIEW_COLUMN (column), 0.5f);
    gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN (column), FALSE);
    gtk_tree_view_append_column(GTK_TREE_VIEW (tree_view), column);


    while (g_variant_iter_loop (iter, "s", &profile))
    {
        gtk_list_store_append (list_model, &tree_iter);
        gtk_list_store_set (list_model, &tree_iter, 0, profile, -1);
    }

    gtk_widget_set_size_request (GTK_WIDGET(dialog), 200, 200);

    g_signal_connect (GTK_WIDGET (dialog),
                      "response",
                      G_CALLBACK (callback),
                      GTK_TREE_SELECTION (tree_selection));
    g_signal_connect_swapped (GTK_WIDGET (dialog),
                              "response",
                              G_CALLBACK (gtk_widget_destroy),
                              GTK_WIDGET (dialog));

    gtk_box_pack_start (GTK_BOX (content_area), scrolled, TRUE, TRUE, 0);

    gtk_widget_show_all (dialog);
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

#if 0
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
#endif

void save_config(GtkDialog *dialog, gint response_id, GtkWidget *edit)
{
#if 0
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
#endif
}

void load_config(GtkDialog *dialog, gint response_id, GtkTreeSelection *Selection_Liste)
{
#if 0
    GtkTreeIter iter;
    GtkTreeModel *Modele;
    gchar *txt;

    if(response_id == GTK_RESPONSE_ACCEPT)
    {
        if(gtk_tree_selection_get_selected(Selection_Liste, &Modele, &iter))
        {
            gtk_tree_model_get(GTK_TREE_MODEL(Modele), &iter, 0, (gint *)&txt, -1);
            gt_config_load_profile (txt);
            gt_config_validate ();
            gt_serial_port_config (serial_port, gt_config_get ());
            add_shortcuts();
        }
    }
#endif
}

void delete_config(GtkDialog *dialog, gint response_id, GtkTreeSelection *Selection_Liste)
{
#if 0
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
#endif
}

#if 0
void gt_config_default (void)
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

    term_conf.rows = 80;
    term_conf.columns = 25;
    term_conf.scrollback = DEFAULT_SCROLLBACK;
    term_conf.visual_bell = TRUE;

    Selec_couleur(&term_conf.foreground_color, 0.66, 0.66, 0.66);
    Selec_couleur(&term_conf.background_color, 0, 0, 0);
}
#endif


#if 0
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
#endif


#if 0
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
#endif

void Config_Terminal(GtkAction *action, gpointer data)
{
    GtkWidget *dialog;
    GtkWidget *font_button;
    GtkWidget *fg_color_button;
    GtkWidget *bg_color_button;
    GtkWidget *scrollback_spin;

    GtkBuilder *builder;

    builder = gtk_builder_new_from_resource ("/org/jensge/GtkTerm/settings-window.ui");

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog-settings-window"));

    font_button = GTK_WIDGET (gtk_builder_get_object (builder, "font-button"));
    gtk_font_chooser_set_font_desc (GTK_FONT_CHOOSER (font_button), term_conf.font);
    g_signal_connect(GTK_WIDGET(font_button), "font-set", G_CALLBACK(read_font_button), 0);

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

#if 0
void Selec_couleur(GdkRGBA *color, gfloat R, gfloat G, gfloat B)
{
	color->red = R;
	color->green = G;
	color->blue = B;
    color->alpha = 1.0;
}
#endif

void config_fg_color(GtkWidget *button, gpointer data)
{
#if 0
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
#endif
}

void config_bg_color(GtkWidget *button, gpointer data)
{
#if 0
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
#endif
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

const char *gt_config_get_profile (void)
{
    if (config_file == NULL)
    {
        config_file = g_strdup (GT_SETTINGS_DEFAULT_ID);
    }

    return config_file;
}

void gt_config_load_profile (const char *path)
{
    g_free (config_file);
    config_file = g_strdup (path);
    gt_serial_port_config (serial_port, path);
}

GSettings *gt_config_get_settings_for_profile (const char *id)
{
    GSettings *settings = NULL;
    char *path = NULL;

    path = g_strdup_printf (GT_SETTINGS_PROFILE_TEMPLATE, id);
    g_debug ("Getting settings from path %s", path);
    settings = g_settings_new_with_path (GT_SETTINGS_PROFILE_SCHEMA, path);
    g_free (path);

    return settings;
}

GSettings *gt_config_get_profile_settings (void)
{
    const char *profile = NULL;

    profile = gt_config_get_profile ();

    return G_SETTINGS (gt_config_get_settings_for_profile (profile));
}
