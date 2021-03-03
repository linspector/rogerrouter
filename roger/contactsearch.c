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

#include "contactsearch.h"

#include "contacts.h"
#include "gd-two-lines-renderer.h"

#include <ctype.h>
#include <glib/gi18n.h>
#include <handy.h>
#include <rm/rm.h>

#define ROW_PADDING_VERT        4
#define ICON_PADDING_LEFT       5
#define ICON_CONTENT_WIDTH      32
#define ICON_PADDING_RIGHT      9
#define ICON_CONTENT_HEIGHT     32
#define TEXT_PADDING_LEFT       0
#define BKMK_PADDING_RIGHT      6

char *
phone_number_type_to_string (RmPhoneNumber *number)
{
  char *tmp;

  switch (number->type) {
    case RM_PHONE_NUMBER_TYPE_HOME:
      tmp = g_strdup (_("Home"));
      break;
    case RM_PHONE_NUMBER_TYPE_WORK:
      tmp = g_strdup (_("Work"));
      break;
    case RM_PHONE_NUMBER_TYPE_MOBILE:
      tmp = g_strdup (_("Mobile"));
      break;
    case RM_PHONE_NUMBER_TYPE_FAX_HOME:
      tmp = g_strdup (_("Fax Home"));
      break;
    case RM_PHONE_NUMBER_TYPE_FAX_WORK:
      tmp = g_strdup (_("Fax Work"));
      break;
    case RM_PHONE_NUMBER_TYPE_OTHER:
      if (number->name && strlen (number->name) > 0) {
        tmp = g_strdup (number->name);
        break;
      }
      /* Fall through */
    default:
      tmp = g_strdup (_("Unknown"));
      break;
  }

  return tmp;
}

static gboolean
contact_search_completion_match_func (GtkEntryCompletion *completion,
                                      const char         *key,
                                      GtkTreeIter        *iter,
                                      gpointer            user_data)
{
  GtkTreeModel *model;
  g_autofree char *item = NULL;

  model = gtk_entry_completion_get_model (completion);
  gtk_tree_model_get (model, iter, 1, &item, -1);

  if (item && rm_strcasestr (item, key))
    return TRUE;

  return FALSE;
}

static gboolean
contact_search_completion_match_selected_cb (GtkEntryCompletion *completion,
                                             GtkTreeModel       *model,
                                             GtkTreeIter        *iter,
                                             gpointer            user_data)
{
  RmPhoneNumber *item;

  gtk_tree_model_get (model, iter, 3, &item, -1);

  gtk_entry_set_text (GTK_ENTRY (gtk_entry_completion_get_entry (completion)), item->number);

  return TRUE;
}

void
contact_search_completion_add (GtkWidget *entry)
{
  GtkListStore *store;
  GSList *list;
  RmAddressBook *book;
  GtkCellRenderer *cell;
  GtkEntryCompletion *completion;

  store = gtk_list_store_new (4, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

  book = rm_profile_get_addressbook (rm_profile_get_active ());
  if (!book) {
    GSList *book_plugins = rm_addressbook_get_plugins ();

    if (book_plugins) {
      book = book_plugins->data;
    }
  }

  list = rm_addressbook_get_contacts (book);

  while (list != NULL) {
    RmContact *contact = list->data;
    GtkTreeIter iter;

    if (contact != NULL) {
      GdkPixbuf *pixbuf;
      GSList *numbers;

      if (contact->image) {
        pixbuf = rm_image_scale (contact->image, 32);
      } else {
        GtkWidget *avatar = hdy_avatar_new (32, contact->name, TRUE);
        pixbuf = hdy_avatar_draw_to_pixbuf (HDY_AVATAR (avatar), 32, 1);
      }

      for (numbers = contact->numbers; numbers != NULL; numbers = numbers->next) {
        RmPhoneNumber *phone_number = numbers->data;
        char *num_str = g_strdup_printf ("%s: %s", phone_number_type_to_string (phone_number), phone_number->number);

        gtk_list_store_insert_with_values (store, &iter, -1, 0, pixbuf, 1, contact->name, 2, num_str, 3, phone_number, -1);
      }
    }

    list = list->next;
  }

  completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_model (GTK_ENTRY_COMPLETION (completion), GTK_TREE_MODEL (store));
  g_signal_connect (completion, "match-selected", G_CALLBACK (contact_search_completion_match_selected_cb), NULL);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (completion), cell, "pixbuf", 0, NULL);

  gtk_cell_renderer_set_padding (cell, ICON_PADDING_LEFT, ROW_PADDING_VERT);
  gtk_cell_renderer_set_fixed_size (cell, (ICON_PADDING_LEFT + ICON_CONTENT_WIDTH + ICON_PADDING_RIGHT), ICON_CONTENT_HEIGHT);
  gtk_cell_renderer_set_alignment (cell, 0.0, 0.5);

  cell = gd_two_lines_renderer_new ();
  g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, "text-lines", 2, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), cell, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion), cell, "text", 1);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion), cell, "line-two", 2);
  gtk_entry_completion_set_match_func (GTK_ENTRY_COMPLETION (completion), contact_search_completion_match_func, NULL, NULL);

  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
}

