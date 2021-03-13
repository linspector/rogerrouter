/*
 * Roger Router
 * Copyright (c) 2012-2021 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "journal.h"

#include "application.h"
#include "contacts.h"
#include "roger-phone.h"
#include "roger-print.h"
#include "roger-voice-mail.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <handy.h>
#include <rm/rm.h>
#include <string.h>
#include <stdlib.h>

struct _RogerJournal {
  HdyWindow parent_instance;

  GtkWidget *header_bars_stack;
  GtkWidget *headerbar;
  GtkWidget *content_stack;
  GtkWidget *journal_listbox;
  GtkWidget *menu_button;
  GtkWidget *filter_combobox;
  GtkWidget *view;
  GtkWidget *spinner;
  GtkListStore *list_store;
  RmFilter *filter;
  RmFilter *search_filter;
  GList *list;
  GtkWidget *search_bar;
  GtkWidget *search_entry;

  GtkWidget *col0;
  GtkWidget *col1;
  GtkWidget *col2;
  GtkWidget *col3;
  GtkWidget *col4;
  GtkWidget *col5;
  GtkWidget *col6;
  GtkWidget *col7;
  GtkWidget *col8;
  GtkCellRenderer *col2_renderer;

  GMutex mutex;
  GCancellable *cancellable;

  gboolean hide_on_quit;
  gboolean hide_on_start;
  gboolean mobile;
  gboolean active;
  guint update_id;
};

G_DEFINE_TYPE (RogerJournal, roger_journal, HDY_TYPE_WINDOW)

static GdkPixbuf *icon_call_in = NULL;
static GdkPixbuf *icon_call_missed = NULL;
static GdkPixbuf *icon_call_out = NULL;
static GdkPixbuf *icon_fax = NULL;
static GdkPixbuf *icon_fax_report = NULL;
static GdkPixbuf *icon_voice = NULL;
static GdkPixbuf *icon_record = NULL;
static GdkPixbuf *icon_blocked = NULL;

static GSettings *journal_window_state = NULL;

static void
clear_listbox (GtkWidget *widget,
               gpointer   data)
{
  gtk_widget_destroy (widget);
}

void
journal_clear (RogerJournal *journal)
{
  if (journal->mobile)
    gtk_container_foreach (GTK_CONTAINER (journal->journal_listbox), clear_listbox, NULL);
  else
    gtk_list_store_clear (journal->list_store);
}

static void
init_call_icons (void)
{
  gint width = 18;

  icon_call_in = gdk_pixbuf_new_from_resource_at_scale ("/org/tabos/roger/images/roger-call-in.svg", width, width, TRUE, NULL);
  icon_call_missed = gdk_pixbuf_new_from_resource_at_scale ("/org/tabos/roger/images/roger-call-missed.svg", width, width, TRUE, NULL);
  icon_call_out = gdk_pixbuf_new_from_resource_at_scale ("/org/tabos/roger/images/roger-call-out.svg", width, width, TRUE, NULL);
  icon_fax = gdk_pixbuf_new_from_resource_at_scale ("/org/tabos/roger/images/roger-fax.svg", width, width, TRUE, NULL);
  icon_fax_report = gdk_pixbuf_new_from_resource_at_scale ("/org/tabos/roger/images/roger-fax-report.svg", width, width, TRUE, NULL);
  icon_voice = gdk_pixbuf_new_from_resource_at_scale ("/org/tabos/roger/images/roger-call-voice.svg", width, width, TRUE, NULL);
  icon_record = gdk_pixbuf_new_from_resource_at_scale ("/org/tabos/roger/images/roger-record.svg", width, width, TRUE, NULL);
  icon_blocked = gdk_pixbuf_new_from_resource_at_scale ("/org/tabos/roger/images/roger-call-blocked.svg", width, width, TRUE, NULL);
}

GdkPixbuf *
roger_journal_get_call_icon (RmCallEntryTypes type)
{
  switch (type) {
    case RM_CALL_ENTRY_TYPE_INCOMING:
      return icon_call_in;
    case RM_CALL_ENTRY_TYPE_MISSED:
      return icon_call_missed;
    case RM_CALL_ENTRY_TYPE_OUTGOING:
      return icon_call_out;
    case RM_CALL_ENTRY_TYPE_FAX:
      return icon_fax;
    case RM_CALL_ENTRY_TYPE_FAX_REPORT:
      return icon_fax_report;
    case RM_CALL_ENTRY_TYPE_VOICE:
      return icon_voice;
    case RM_CALL_ENTRY_TYPE_RECORD:
      return icon_record;
    case RM_CALL_ENTRY_TYPE_BLOCKED:
      return icon_blocked;
    default:
      g_debug ("%s(): Unknown icon type: %d", __FUNCTION__, type);
      break;
  }

  return NULL;
}

static void
box_header_func (GtkListBoxRow *row,
                 GtkListBoxRow *before,
                 gpointer       user_data)
{
  GtkWidget *current;

  if (!before) {
    gtk_list_box_row_set_header (row, NULL);
    return;
  }

  current = gtk_list_box_row_get_header (row);
  if (!current) {
    current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_show (current);
    gtk_list_box_row_set_header (row, current);
  }
}

void
journal_redraw (RogerJournal *self)
{
  GList *list;
  gint duration = 0;
  char *text = NULL;
  gint count = 0;
  RmProfile *profile;

  /* Update liststore */
  for (list = self->list; list != NULL; list = list->next) {
    RmCallEntry *call = list->data;

    g_assert (call != NULL);

    if (rm_filter_rule_match (self->filter, call) == FALSE) {
      continue;
    }

    if (rm_filter_rule_match (self->search_filter, call) == FALSE) {
      continue;
    }

    if (!self->mobile) {
      GtkTreeIter iter;

      gtk_list_store_insert_with_values (self->list_store, &iter, -1,
                                         JOURNAL_COL_TYPE, roger_journal_get_call_icon (call->type),
                                         JOURNAL_COL_DATETIME, call->date_time,
                                         JOURNAL_COL_NAME, call->remote->name,
                                         JOURNAL_COL_COMPANY, call->remote->company,
                                         JOURNAL_COL_NUMBER, call->remote->number,
                                         JOURNAL_COL_CITY, call->remote->city,
                                         JOURNAL_COL_EXTENSION, call->local->name,
                                         JOURNAL_COL_LINE, call->local->number,
                                         JOURNAL_COL_DURATION, call->duration,
                                         JOURNAL_COL_CALL_PTR, call,
                                         -1);
    } else {
      GtkWidget *row = gtk_list_box_row_new ();
      GtkWidget *grid = gtk_grid_new ();
      GtkWidget *icon;
      GtkWidget *name;
      GtkWidget *date;
      GtkWidget *phone;
      g_autofree char *tmp = NULL;

      gtk_container_set_border_width (GTK_CONTAINER (grid), 6);

      icon = gtk_image_new_from_pixbuf (roger_journal_get_call_icon (call->type));
      gtk_grid_attach (GTK_GRID (grid), icon, 0, 0, 1, 2);
      gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

      if (!RM_EMPTY_STRING (call->remote->name)) {
        name = gtk_label_new (call->remote->name);
      } else {
        name = gtk_label_new (_("Unknown"));
        gtk_widget_set_sensitive (name, FALSE);
      }
      gtk_label_set_line_wrap (GTK_LABEL (name), TRUE);
      gtk_widget_set_hexpand (name, TRUE);
      gtk_label_set_ellipsize (GTK_LABEL (name), PANGO_ELLIPSIZE_END);
      PangoAttrList *attrlist = pango_attr_list_new ();
      PangoAttribute *attr = pango_attr_weight_new (PANGO_WEIGHT_SEMIBOLD);
      pango_attr_list_insert (attrlist, attr);
      gtk_label_set_attributes (GTK_LABEL (name), attrlist);
      pango_attr_list_unref (attrlist);

      gtk_label_set_xalign (GTK_LABEL (name), 0.0f);
      gtk_grid_attach (GTK_GRID (grid), name, 1, 0, 2, 1);

      phone = gtk_label_new (call->remote->number);
      gtk_label_set_line_wrap (GTK_LABEL (phone), TRUE);
      gtk_widget_set_sensitive (phone, FALSE);
      gtk_label_set_xalign (GTK_LABEL (phone), 0.0f);
      gtk_grid_attach (GTK_GRID (grid), phone, 1, 1, 1, 1);
      g_object_set_data (G_OBJECT (row), "number", call->remote->number);

      tmp = g_strdup (call->date_time);
      date = gtk_label_new (tmp);
      gtk_label_set_line_wrap (GTK_LABEL (date), TRUE);
      gtk_label_set_xalign (GTK_LABEL (date), 1.0f);
      gtk_widget_set_sensitive (date, FALSE);
      gtk_grid_attach (GTK_GRID (grid), date, 2, 1, 1, 1);

      /*duration = gtk_label_new(call->duration);
       *  gtk_label_set_xalign (GTK_LABEL(duration), 1.0f);
       *  gtk_grid_attach (GTK_GRID (grid), duration, 2, 0, 1, 3);*/

      gtk_widget_show_all (grid);

      gtk_container_add (GTK_CONTAINER (row), grid);
      gtk_widget_show_all (row);
      gtk_list_box_insert (GTK_LIST_BOX (self->journal_listbox), row, -1);
    }
    if (call->duration && strchr (call->duration, 's') != NULL) {
      /* Ignore voicebox duration */
    } else {
      if (call->duration != NULL && strlen (call->duration) > 0) {
        duration += (call->duration[0] - '0') * 60;
        duration += (call->duration[2] - '0') * 10;
        duration += call->duration[3] - '0';
      }
    }
    count++;
  }

  profile = rm_profile_get_active ();

  hdy_header_bar_set_title (HDY_HEADER_BAR (self->headerbar), profile ? profile->name : _("<No profile>"));
  g_autofree char *markup = g_strdup_printf (_("%d calls, %d:%2.2dh"), count, duration / 60, duration % 60);
  hdy_header_bar_set_subtitle (HDY_HEADER_BAR (self->headerbar), markup);

  g_free (text);
}

static void
lookup_journal_thread_cb (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  RogerJournal *self = ROGER_JOURNAL (task_data);
  GList *list;
  gboolean retval = FALSE;

  if (g_task_return_error_if_cancelled (task))
    return;

  for (list = self->list; list; list = list->next) {
    RmCallEntry *call = list->data;

    if (!RM_EMPTY_STRING (call->remote->name))
      continue;

    if (rm_lookup_search (call->remote->number, call->remote)) {
      call->remote->lookup = TRUE;
      retval = TRUE;
    }
  }

  g_task_return_boolean (task, retval);
}

void journal_filter_box_changed (GtkComboBox *box,
                                 gpointer     user_data);
static void journal_update_filter_box (RogerJournal *self);

void
journal_update_content (RogerJournal *self)
{
  RmProfile *profile = rm_profile_get_active ();

  if (!profile) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->content_stack), "welcome");
    gtk_stack_set_visible_child_name (GTK_STACK (self->header_bars_stack), "empty");
    return;
  }

  gtk_stack_set_visible_child_name (GTK_STACK (self->header_bars_stack), "full");

  if (self->mobile) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->content_stack), "mobile");
    gtk_widget_set_visible (self->filter_combobox, FALSE);
  } else {
    gtk_stack_set_visible_child_name (GTK_STACK (self->content_stack), "treeview");
    gtk_widget_set_visible (self->filter_combobox, TRUE);
  }
}

static void
journal_reverse_lookup_async (GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  RogerJournal *self = ROGER_JOURNAL (user_data);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, journal_reverse_lookup_async);

  g_task_set_return_on_cancel (task, FALSE);

  g_task_set_task_data (task, g_object_ref (self), g_object_unref);
  g_task_run_in_thread (task, lookup_journal_thread_cb);
}

static gboolean
journal_reverse_lookup_finished (GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, lookup_journal_thread_cb), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
journal_reverse_lookup_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  gboolean lookup_found = journal_reverse_lookup_finished (res, &error);
  RogerJournal *self = ROGER_JOURNAL (user_data);
  GtkTreeIter iter;
  gboolean valid;
  RmCallEntry *call;

  if (lookup_found) {
    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->list_store), &iter);
    while (valid) {
      gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &iter, JOURNAL_COL_CALL_PTR, &call, -1);

      if (call->remote->lookup) {
        gtk_list_store_set (self->list_store, &iter, JOURNAL_COL_NAME, call->remote->name, -1);
        gtk_list_store_set (self->list_store, &iter, JOURNAL_COL_CITY, call->remote->city, -1);
      }

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->list_store), &iter);
    }
  }

  gtk_spinner_stop (GTK_SPINNER (self->spinner));
  gtk_widget_hide (self->spinner);
}

static void
roger_journal_loaded_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
  g_autoptr (GError) error = NULL;
  GList *list = rm_router_load_journal_finish (source_object, res, &error);
  GList *old;

  if (!self->active) {
    self->active = TRUE;
    journal_update_content (self);
  }

  if (!self->list && list)
    journal_update_filter_box (self);

  journal_filter_box_changed (GTK_COMBO_BOX (self->filter_combobox), self);

  /* Clear existing liststore */
  journal_clear (self);

  /* Set new internal list */
  old = self->list;
  self->list = list;

  if (old) {
    rm_journal_free (old);
  }

  journal_redraw (self);
  journal_reverse_lookup_async (self->cancellable, journal_reverse_lookup_cb, self);
}

static void
reload_journal (RogerJournal *self)
{
  RmProfile *profile = rm_profile_get_active ();

  if (!profile)
    return;

  gtk_spinner_start (GTK_SPINNER (self->spinner));
  gtk_widget_show (self->spinner);

  rm_router_load_journal_async (profile, self->cancellable, roger_journal_loaded_cb, self);
}

static void
on_connection_changed (RmObject     *obj,
                       gint          type,
                       RmConnection *connection,
                       gpointer      user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);

  if (type == RM_CONNECTION_TYPE_DISCONNECT)
    reload_journal (self);
}

static void
clear_journal (RogerJournal *self)
{
  GtkWidget *dialog;
  gint flags = GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR;

  dialog = gtk_message_dialog_new (GTK_WINDOW (self), flags, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, _("Do you want to delete the router journal?"));

  gtk_dialog_add_button (GTK_DIALOG (dialog), _("Cancel"), GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("Delete"), GTK_RESPONSE_OK);

  gint result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (result != GTK_RESPONSE_OK) {
    return;
  }

  rm_router_clear_journal (rm_profile_get_active ());
}

void
delete_foreach (GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
  RmCallEntry *call = NULL;
  GValue ptr = { 0 };

  gtk_tree_model_get_value (model, iter, JOURNAL_COL_CALL_PTR, &ptr);

  call = g_value_get_pointer (&ptr);

  switch (call->type) {
    case RM_CALL_ENTRY_TYPE_RECORD:
      g_unlink (call->priv);
      break;
    case RM_CALL_ENTRY_TYPE_VOICE:
      rm_router_delete_voice (rm_profile_get_active (), call->priv);
      break;
    case RM_CALL_ENTRY_TYPE_FAX:
      rm_router_delete_fax (rm_profile_get_active (), call->priv);
      break;
    case RM_CALL_ENTRY_TYPE_FAX_REPORT:
      g_unlink (call->priv);
      break;
    default:
      self->list = g_list_remove (self->list, call);
      g_debug ("Deleting: '%s'", call->date_time);
      rm_journal_save (self->list);
      break;
  }
}

void
journal_button_delete_clicked_cb (GtkWidget *button,
                                  gpointer   user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
  GtkWidget *dialog;
  gint flags = GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR;

  dialog = gtk_message_dialog_new (GTK_WINDOW (self), flags, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, _("Do you really want to delete the selected entry?"));

  gtk_dialog_add_button (GTK_DIALOG (dialog), _("Cancel"), GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("Delete"), GTK_RESPONSE_OK);

  gint result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (result != GTK_RESPONSE_OK) {
    return;
  }

  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->view));

  gtk_tree_selection_selected_foreach (selection, delete_foreach, NULL);
}

void
journal_add_contact (RmCallEntry *call)
{
  app_contacts (call->remote);
}

void
add_foreach (GtkTreeModel *model,
             GtkTreePath  *path,
             GtkTreeIter  *iter,
             gpointer      data)
{
  RmCallEntry *call = NULL;
  GValue ptr = { 0 };

  gtk_tree_model_get_value (model, iter, JOURNAL_COL_CALL_PTR, &ptr);

  call = g_value_get_pointer (&ptr);

  journal_add_contact (call);
}

void
journal_button_add_clicked_cb (GtkWidget *button,
                               GtkWidget *view)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  gtk_tree_selection_selected_foreach (selection, add_foreach, NULL);
}

static void
on_search_entry_changed (GtkEditable *entry,
                         gpointer     user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
  RmProfile *profile = rm_profile_get_active ();
  const char *text = gtk_entry_get_text (GTK_ENTRY (entry));

  if (self->search_filter != NULL) {
    rm_filter_remove (profile, self->search_filter);
    self->search_filter = NULL;
  }

  if (strlen (text) > 0) {
    self->search_filter = rm_filter_new (profile, "internal_search");

    if (g_ascii_isdigit (text[0])) {
      rm_filter_rule_add (self->search_filter, RM_FILTER_REMOTE_NUMBER, RM_FILTER_CONTAINS, (char *)text);
    } else {
      rm_filter_rule_add (self->search_filter, RM_FILTER_REMOTE_NAME, RM_FILTER_CONTAINS, (char *)text);
    }
  }

  journal_clear (self);
  journal_redraw (self);
}

void
journal_filter_box_changed (GtkComboBox *box,
                            gpointer     user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
  RmProfile *profile = rm_profile_get_active ();
  GList *filter_list;
  const char *text = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (box));

  self->filter = NULL;
  journal_clear (self);

  if (text == NULL || profile == NULL) {
    return;
  }

  for (filter_list = rm_filter_get_list (profile); filter_list != NULL; filter_list = filter_list->next) {
    RmFilter *filter = filter_list->data;

    if (!strcmp (filter->name, text)) {
      self->filter = filter;
      break;
    }
  }

  journal_redraw (self);
}

static void
journal_update_filter_box (RogerJournal *self)
{
  GList *filter_list;

  filter_list = rm_filter_get_list (rm_profile_get_active ());

  gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (self->filter_combobox));

  while (filter_list != NULL) {
    RmFilter *filter = filter_list->data;

    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (self->filter_combobox), NULL, filter->name);
    filter_list = filter_list->next;
  }

  gtk_combo_box_set_active (GTK_COMBO_BOX (self->filter_combobox), 0);
}

static void
roger_journal_voice_loaded_cb (GObject      *source_object,
                               GAsyncResult *res,
                               gpointer      user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
  g_autoptr (GError) error = NULL;
  g_autofree GBytes *bytes = rm_router_load_voice_mail_finish (source_object, res, &error);
  GtkWidget *voice_mail = NULL;

  if (!bytes) {
    g_warning ("Could not load voice file: %s", error ? error->message : "?");
    return;
  }

  voice_mail = roger_voice_mail_new ();
  gtk_window_set_transient_for (GTK_WINDOW (voice_mail), GTK_WINDOW (self));
  gtk_widget_show (voice_mail);
  roger_voice_mail_play (ROGER_VOICE_MAIL (voice_mail), g_steal_pointer (&bytes));
}

static void
on_view_row_activated (GtkTreeView       *view,
                       GtkTreePath       *path,
                       GtkTreeViewColumn *column,
                       gpointer           user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  RmCallEntry *call;
  GError *error = NULL;
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter, JOURNAL_COL_CALL_PTR, &call, -1);

  switch (call->type) {
    case RM_CALL_ENTRY_TYPE_FAX_REPORT: {
      g_autofree char *uri = g_strdup_printf ("file:///%s", call->priv);

      if (!gtk_show_uri_on_window (GTK_WINDOW (self), uri, GDK_CURRENT_TIME, &error)) {
        g_debug ("%s(): Could not open uri '%s'", __FUNCTION__, uri);
        g_debug ("%s(): '%s'", __FUNCTION__, error->message);
      } else {
        g_debug ("%s(): Opened '%s'", __FUNCTION__, uri);
      }
      break;
    }
    case RM_CALL_ENTRY_TYPE_FAX: {
      gsize len = 0;
      g_autofree char *data = NULL;

      data = rm_router_load_fax (rm_profile_get_active (), call->priv, &len);

      if (data && len) {
        g_autofree char *path = g_build_filename (rm_get_user_cache_dir (), G_DIR_SEPARATOR_S, "fax.pdf", NULL);
        g_autofree char *uri = g_strdup_printf ("file:///%s", path);

        rm_file_save (path, data, len);

        if (!gtk_show_uri_on_window (GTK_WINDOW (self), uri, GDK_CURRENT_TIME, &error)) {
          g_debug ("%s(): Could not open uri '%s'", __FUNCTION__, uri);
          g_debug ("%s(): '%s'", __FUNCTION__, error->message);
        } else {
          g_debug ("%s(): Opened '%s'", __FUNCTION__, uri);
        }
      }
      break;
    }
    case RM_CALL_ENTRY_TYPE_RECORD: {
      char *tmp = call->priv;

      if (!gtk_show_uri_on_window (GTK_WINDOW (self), tmp, GDK_CURRENT_TIME, &error)) {
        g_debug ("%s(): Could not open uri '%s'", __FUNCTION__, tmp);
        g_debug ("%s(): '%s'", __FUNCTION__, error->message);
      } else {
        g_debug ("%s(): Opened '%s'", __FUNCTION__, tmp);
      }
      break;
    }
    case RM_CALL_ENTRY_TYPE_VOICE:
      rm_router_load_voice_mail_async (rm_profile_get_active (), call->priv, NULL, roger_journal_voice_loaded_cb, self);
      break;
    default: {
      GtkWidget *phone = roger_phone_new ();

      roger_phone_set_dial_number (ROGER_PHONE (phone), call->remote->number);

      gtk_window_set_transient_for (GTK_WINDOW (phone), GTK_WINDOW (self));
      gtk_widget_show_all (phone);
      break;
    }
  }
}

void
journal_set_hide_on_quit (RogerJournal *self,
                          gboolean      hide)
{
  self->hide_on_quit = hide;

  if (!hide) {
    gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
  }
}

void
journal_set_hide_on_start (RogerJournal *self,
                           gboolean      hide)
{
  self->hide_on_start = hide;

  if (hide) {
    gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
  }
}

static gboolean
on_delete_event (GtkWidget *widget,
                 GdkEvent  *event,
                 gpointer   user_data)
{
  RogerJournal *self = ROGER_JOURNAL (widget);

  if (self->hide_on_quit) {
    gtk_widget_hide (widget);

    return TRUE;
  }

  return FALSE;
}

static gint
journal_sort_by_date (GtkTreeModel *model,
                      GtkTreeIter  *a,
                      GtkTreeIter  *b,
                      gpointer      data)
{
  RmCallEntry *call_a;
  RmCallEntry *call_b;

  gtk_tree_model_get (model, a, JOURNAL_COL_CALL_PTR, &call_a, -1);
  gtk_tree_model_get (model, b, JOURNAL_COL_CALL_PTR, &call_b, -1);

  return rm_journal_sort_by_date (call_a, call_b);
}

static gint
journal_sort_by_type (GtkTreeModel *model,
                      GtkTreeIter  *a,
                      GtkTreeIter  *b,
                      gpointer      data)
{
  RmCallEntry *call_a;
  RmCallEntry *call_b;

  gtk_tree_model_get (model, a, JOURNAL_COL_CALL_PTR, &call_a, -1);
  gtk_tree_model_get (model, b, JOURNAL_COL_CALL_PTR, &call_b, -1);

  return call_a->type > call_b->type ? -1 : call_a->type < call_b->type ? 1 : 0;
}

static void
name_column_cell_data_func (GtkTreeViewColumn *column,
                            GtkCellRenderer   *renderer,
                            GtkTreeModel      *model,
                            GtkTreeIter       *iter,
                            gpointer           user_data)
{
  RmCallEntry *call;

  gtk_tree_model_get (model, iter, JOURNAL_COL_CALL_PTR, &call, -1);

  if (call != NULL && call->remote->lookup) {
    g_object_set (renderer, "foreground", "darkgrey", "foreground-set", TRUE, NULL);
  } else {
    g_object_set (renderer, "foreground-set", FALSE, NULL);
  }
}

static gboolean
journal_column_header_button_pressed_cb (GtkTreeViewColumn *column,
                                         GdkEventButton    *event,
                                         gpointer           user_data)
{
  GtkMenu *menu = GTK_MENU (user_data);

  if (event->button == GDK_BUTTON_SECONDARY) {
#if GTK_CHECK_VERSION (3, 21, 0)
    gtk_menu_popup_at_pointer (menu, (GdkEvent *)event);
#else
    gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);
#endif
    return TRUE;
  }

  return FALSE;
}

void
journal_popup_copy_number (GtkWidget   *widget,
                           RmCallEntry *call)
{
  gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE), call->remote->number, -1);
}

void
journal_popup_call_number (GtkWidget   *widget,
                           RmCallEntry *call)
{
  GtkWidget *phone = roger_phone_new ();

  roger_phone_set_dial_number (ROGER_PHONE (phone), call->remote->number);

  gtk_widget_show_all (phone);
}

void
journal_popup_add_contact (GtkWidget   *widget,
                           RmCallEntry *call)
{
  journal_add_contact (call);
}

void
journal_popup_delete_entry (GtkWidget   *widget,
                            RmCallEntry *call)
{
  /*journal_button_delete_clicked_cb(NULL, journal_view); */
}

void
journal_popup_menu (GtkWidget      *treeview,
                    GdkEventButton *event,
                    gpointer        user_data)
{
  GtkWidget *menu, *menuitem;
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  GtkTreeModel *model;
  GList *list;
  GtkTreeIter iter;
  RmCallEntry *call = NULL;

  if (gtk_tree_selection_count_selected_rows (selection) != 1) {
    return;
  }
  list = gtk_tree_selection_get_selected_rows (selection, &model);
  gtk_tree_model_get_iter (model, &iter, (GtkTreePath *)list->data);
  gtk_tree_model_get (model, &iter, JOURNAL_COL_CALL_PTR, &call, -1);

  menu = gtk_menu_new ();

  /* Copy phone number */
  menuitem = gtk_menu_item_new_with_label (_("Copy number"));
  g_signal_connect (menuitem, "activate", (GCallback)journal_popup_copy_number, call);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  /* Call phone number */
  menuitem = gtk_menu_item_new_with_label (_("Call number"));
  g_signal_connect (menuitem, "activate", (GCallback)journal_popup_call_number, call);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  /* Separator */
  menuitem = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  /* Add contact */
  menuitem = gtk_menu_item_new_with_label (_("Add contact"));
  g_signal_connect (menuitem, "activate", (GCallback)journal_popup_add_contact, call);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  /* Separator */
  /* menuitem = gtk_separator_menu_item_new (); */
  /* gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem); */

  /* Delete entry */
  /* menuitem = gtk_menu_item_new_with_label (_("Delete entry")); */
  /* g_signal_connect (menuitem, "activate", (GCallback)journal_popup_delete_entry, call); */
  /* gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem); */

  gtk_widget_show_all (menu);

  gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *)event);
}

gboolean
on_view_button_press_event (GtkWidget      *treeview,
                            GdkEventButton *event,
                            gpointer        user_data)
{
  if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY) {
    GtkTreeSelection *selection;
    GtkTreePath *path;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL)) {
      gtk_tree_selection_unselect_all (selection);
      gtk_tree_selection_select_path (selection, path);
      gtk_tree_path_free (path);
    }

    journal_popup_menu (treeview, event, user_data);

    return TRUE;
  }

  return FALSE;
}

static void
window_cmd_refresh (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);

  reload_journal (self);
}

static void
window_cmd_print (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);

  roger_print_journal (self->list);
}

static void
window_cmd_clear (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);

  clear_journal (self);
}

static void
window_cmd_export (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
  g_autoptr (GtkFileChooserNative) native = NULL;
  GtkFileChooser *chooser;
  gint res;

  native = gtk_file_chooser_native_new (_("Export journal"), GTK_WINDOW (self), GTK_FILE_CHOOSER_ACTION_SAVE, _("Save"), _("Cancel"));
  chooser = GTK_FILE_CHOOSER (native);
  gtk_file_chooser_set_current_name (chooser, "journal.csv");

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));
  if (res == GTK_RESPONSE_ACCEPT) {
    g_autofree char *file = gtk_file_chooser_get_filename (chooser);

    rm_journal_save_as (self->list, file);
  }
}

static void
on_contacts_changed (RmObject *object,
                     gpointer  user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);

  reload_journal (self);
}

void
journal_set_visible (RogerJournal *self,
                     gboolean      state)
{
  gtk_widget_set_visible (GTK_WIDGET (self), state);
}

#if 0
void
journal_column_restore_default (GtkMenuItem *item,
                                gpointer     user_data)
{
  gint index;

  for (index = JOURNAL_COL_DATETIME; index <= JOURNAL_COL_DURATION; index++) {
    GtkTreeViewColumn *column;
    char *key;

    key = g_strdup_printf ("col-%d-width", index);
    g_settings_set_uint (app_settings, key, 0);
    g_free (key);

    key = g_strdup_printf ("col-%d-visible", index);
    g_settings_set_boolean (app_settings, key, TRUE);
    g_free (key);

    column = gtk_tree_view_get_column (GTK_TREE_VIEW (user_data), index);
    gtk_tree_view_column_set_visible (column, TRUE);
    gtk_tree_view_column_set_fixed_width (column, -1);
  }
}
#endif

static gboolean
on_key_press_event (GtkWidget *widget,
                    GdkEvent  *event,
                    gpointer   user_data)
{
  RogerJournal *self = ROGER_JOURNAL (widget);

  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->header_bars_stack)), "empty") == 0) {
    return GDK_EVENT_PROPAGATE;
  }

  return hdy_search_bar_handle_event (HDY_SEARCH_BAR (self->search_bar), event);
}

static const GActionEntry window_entries [] = {
  { "refresh", window_cmd_refresh },
  { "print", window_cmd_print },
  { "clear", window_cmd_clear },
  { "export", window_cmd_export },
  /*
   *  { "contacts-edit-phone-home", contacts_add_detail_activated },
   *  { "contacts-edit-phone-work", contacts_add_detail_activated },
   *  { "contacts-edit-phone-mobile", contacts_add_detail_activated },
   *  { "contacts-edit-phone-home-fax", contacts_add_detail_activated },
   *  { "contacts-edit-phone-work-fax", contacts_add_detail_activated },
   *  { "contacts-edit-phone-pager", contacts_add_detail_activated },
   *  { "contacts-edit-address-home", contacts_add_detail_activated },
   *  { "contacts-edit-address-work", contacts_add_detail_activated },*/
};

static void
roger_journal_constructed (GObject *object)
{
  RogerJournal *journal = ROGER_JOURNAL (object);
  g_autoptr (GVariant) default_size = NULL;
  gboolean maximized;
  gboolean fullscreen;
  int default_width;
  int default_height;

  maximized = g_settings_get_boolean (journal_window_state, "maximized");
  fullscreen = g_settings_get_boolean (journal_window_state, "fullscreen");
  default_size = g_settings_get_value (journal_window_state,
                                       "initial-size");

  g_variant_get (default_size, "(ii)", &default_width, &default_height);

  gtk_window_set_default_size (GTK_WINDOW (journal), default_width, default_height);

  if (maximized)
    gtk_window_maximize (GTK_WINDOW (journal));

  if (fullscreen)
    gtk_window_fullscreen (GTK_WINDOW (journal));

  G_OBJECT_CLASS (roger_journal_parent_class)->constructed (object);

  if (!journal->hide_on_start)
    gtk_widget_show (GTK_WIDGET (journal));
}

static void
roger_journal_dispose (GObject *self)
{
  RogerJournal *journal = ROGER_JOURNAL (self);
  gboolean is_maximized;

  if (gtk_widget_get_window (GTK_WIDGET (journal))) {
    GVariant *initial_size;
    int width;
    int height;

    gtk_window_get_size (GTK_WINDOW (journal), &width, &height);

    initial_size = g_variant_new_parsed ("(%i, %i)", width, height);
    is_maximized = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (journal))) & GDK_WINDOW_STATE_MAXIMIZED;

    if (!is_maximized)
      g_settings_set_value (journal_window_state, "initial-size", initial_size);

    g_settings_set_boolean (journal_window_state, "maximized", is_maximized);
  }

  g_clear_handle_id (&journal->update_id, g_source_remove);

  g_cancellable_cancel (journal->cancellable);

  if (journal->list) {
    g_list_free_full (g_steal_pointer (&journal->list), rm_call_entry_free);
  }

  G_OBJECT_CLASS (roger_journal_parent_class)->dispose (self);
}

static void
roger_journal_size_allocate (GtkWidget     *self,
                             GtkAllocation *allocation)
{
  RogerJournal *journal = ROGER_JOURNAL (self);
  GtkAllocation alloc;
  gboolean mobile;

  GTK_WIDGET_CLASS (roger_journal_parent_class)->size_allocate (self, allocation);

  gtk_widget_get_allocated_size (self, &alloc, NULL);
  mobile = alloc.width < 500;

  if (!journal->active) {
    journal->mobile = mobile;
    return;
  }


  if (journal->mobile != mobile) {
    journal_clear (journal);
    journal->mobile = mobile;
    journal_update_content (journal);
    journal_redraw (journal);
  }
}

static void
roger_journal_listbox_row_activated_cb (GtkListBox    *box,
                                        GtkListBoxRow *row,
                                        gpointer       user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
  GtkWidget *phone = roger_phone_new ();
  char *number = g_object_get_data (G_OBJECT (row), "number");

  roger_phone_set_dial_number (ROGER_PHONE (phone), number);

  gtk_window_set_transient_for (GTK_WINDOW (phone), GTK_WINDOW (self));
  gtk_widget_show_all (phone);
}

static void
roger_journal_class_init (RogerJournalClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  widget_class->size_allocate = roger_journal_size_allocate;
  object_class->constructed = roger_journal_constructed;
  object_class->dispose = roger_journal_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tabos/roger/ui/journal.ui");

  gtk_widget_class_bind_template_child (widget_class, RogerJournal, header_bars_stack);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, headerbar);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, content_stack);

  gtk_widget_class_bind_template_child (widget_class, RogerJournal, journal_listbox);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, menu_button);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, filter_combobox);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, view);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, list_store);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, spinner);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, search_bar);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, search_entry);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, col0);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, col1);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, col2);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, col3);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, col4);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, col5);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, col6);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, col7);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, col8);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, col2_renderer);
  gtk_widget_class_bind_template_child (widget_class, RogerJournal, content_stack);

  gtk_widget_class_bind_template_callback (widget_class, on_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_delete_event);
  gtk_widget_class_bind_template_callback (widget_class, on_view_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_view_button_press_event);
  gtk_widget_class_bind_template_callback (widget_class, journal_filter_box_changed);
  gtk_widget_class_bind_template_callback (widget_class, journal_column_header_button_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_journal_listbox_row_activated_cb);
}

static void
roger_journal_init (RogerJournal *self)
{
  GtkTreeSortable *sortable;
  GSimpleActionGroup *simple_action_group;

  journal_window_state = g_settings_new ("org.tabos.roger.window-state");

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Add actions */
  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   window_entries,
                                   G_N_ELEMENTS (window_entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "win",
                                  G_ACTION_GROUP (simple_action_group));


  init_call_icons ();
  self->list = NULL;

  g_type_ensure (G_TYPE_THEMED_ICON);
  GtkBuilder *builder = gtk_builder_new_from_resource ("/org/tabos/roger/ui/journal-popover.ui");
  GtkWidget *journal_popover = GTK_WIDGET (gtk_builder_get_object (builder, "RogerJournalPopover"));
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->menu_button), journal_popover);
  g_object_unref (builder);

  hdy_search_bar_connect_entry (HDY_SEARCH_BAR (self->search_bar), GTK_ENTRY (self->search_entry));
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->journal_listbox), box_header_func, NULL, NULL);

  journal_update_filter_box (self);

  gtk_widget_hide_on_delete (GTK_WIDGET (self));

  journal_filter_box_changed (GTK_COMBO_BOX (self->filter_combobox), self);

  sortable = GTK_TREE_SORTABLE (self->list_store);

  g_settings_bind (app_settings, "col-0-width", self->col0, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-0-visible", self->col0, "visible", G_SETTINGS_BIND_DEFAULT);
  gtk_tree_sortable_set_sort_func (sortable, JOURNAL_COL_TYPE, journal_sort_by_type, 0, NULL);

  g_settings_bind (app_settings, "col-1-width", self->col1, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-1-visible", self->col1, "visible", G_SETTINGS_BIND_DEFAULT);
  gtk_tree_sortable_set_sort_func (sortable, JOURNAL_COL_DATETIME, journal_sort_by_date, 0, NULL);

  g_settings_bind (app_settings, "col-2-width", self->col2, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-2-visible", self->col2, "visible", G_SETTINGS_BIND_DEFAULT);
  gtk_tree_view_column_set_cell_data_func (GTK_TREE_VIEW_COLUMN (self->col2), self->col2_renderer, name_column_cell_data_func, NULL, NULL);

  g_settings_bind (app_settings, "col-3-width", self->col3, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-3-visible", self->col3, "visible", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-4-width", self->col4, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-4-visible", self->col4, "visible", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-5-width", self->col5, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-5-visible", self->col5, "visible", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-6-width", self->col6, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-6-visible", self->col6, "visible", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-7-width", self->col7, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-7-visible", self->col7, "visible", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-8-width", self->col8, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-8-visible", self->col8, "visible", G_SETTINGS_BIND_DEFAULT);

  self->cancellable = g_cancellable_new ();

  self->active = FALSE;
  gtk_stack_set_visible_child_name (GTK_STACK (self->header_bars_stack), "empty");
  gtk_stack_set_visible_child_name (GTK_STACK (self->content_stack), "loading");

  g_signal_connect_object (rm_object, "connection-changed", G_CALLBACK (on_connection_changed), self, 0);
  g_signal_connect_object (rm_object, "contacts-changed", G_CALLBACK (on_contacts_changed), self, 0);
}

GtkWidget *
roger_journal_new (void)
{
  return g_object_new (ROGER_TYPE_JOURNAL, NULL);
}
