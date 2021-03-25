/*
 * Roger Router
 * Copyright (c) 2012-2022 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "roger-contactsearch.h"

#include "suggestion-entry.h"

#include <ctype.h>
#include <glib/gi18n.h>
#include <adwaita.h>
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

static void
contact_search_completion_match_func (MatchObject *object,
                                      const char  *search,
                                      gpointer     user_data)
{
  char *tmp1, *tmp2, *tmp3, *tmp4;

  tmp1 = g_utf8_normalize (match_object_get_string (object), -1, G_NORMALIZE_ALL);
  tmp2 = g_utf8_casefold (tmp1, -1);

  tmp3 = g_utf8_normalize (search, -1, G_NORMALIZE_ALL);
  tmp4 = g_utf8_casefold (tmp3, -1);

  if (g_str_has_prefix (tmp2, tmp4))
    match_object_set_match (object, 0, g_utf8_strlen (search, -1), 1);
  else
    match_object_set_match (object, 0, 0, 0);

  g_free (tmp1);
  g_free (tmp2);
  g_free (tmp3);
  g_free (tmp4);
}

/* static gboolean */
/* contact_search_completion_match_selected_cb (GtkEntryCompletion *completion, */
/*                                              GtkTreeModel       *model, */
/*                                              GtkTreeIter        *iter, */
/*                                              gpointer            user_data) */
/* { */
/*   RmPhoneNumber *item; */

/*   gtk_tree_model_get (model, iter, 3, &item, -1); */

/*   gtk_editable_set_text (GTK_EDITABLE (gtk_entry_completion_get_entry (completion)), item->number); */

/*   return TRUE; */
/* } */

#define STRING_TYPE_HOLDER (string_holder_get_type ())
G_DECLARE_FINAL_TYPE (StringHolder, string_holder, STRING, HOLDER, GObject)

struct _StringHolder {
  GObject parent_instance;
  char *name;
  char *number;
  char *number_str;
  GdkPixbuf *icon;
};

G_DEFINE_TYPE (StringHolder, string_holder, G_TYPE_OBJECT);

static void
string_holder_init (StringHolder *holder)
{
}

static void
string_holder_finalize (GObject *object)
{
  StringHolder *holder = STRING_HOLDER (object);

  g_free (holder->name);
  g_free (holder->number);
  g_free (holder->number_str);
  g_clear_object (&holder->icon);

  G_OBJECT_CLASS (string_holder_parent_class)->finalize (object);
}

static void
string_holder_class_init (StringHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = string_holder_finalize;
}

static StringHolder *
string_holder_new (const char *name,
                   GdkPixbuf  *icon,
                   const char *number,
                   const char *number_str)
{
  StringHolder *holder = g_object_new (STRING_TYPE_HOLDER, NULL);
  holder->name = g_strdup (name);
  holder->icon = icon ? g_object_ref (icon) : NULL;
  holder->number = g_strdup (number);
  holder->number_str = g_strdup (number_str);
  return holder;
}

static void
strings_setup_item_full (GtkSignalListItemFactory *factory,
                         GtkListItem              *item)
{
  GtkWidget *box, *box2, *image, *name, *number;

  image = gtk_image_new ();
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  name = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (name), 0.0);
  number = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (number), 0.0);
  gtk_widget_add_css_class (number, "dim-label");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  gtk_box_append (GTK_BOX (box), image);
  gtk_box_append (GTK_BOX (box), box2);
  gtk_box_append (GTK_BOX (box2), name);
  gtk_box_append (GTK_BOX (box2), number);

  g_object_set_data (G_OBJECT (item), "name", name);
  g_object_set_data (G_OBJECT (item), "image", image);
  g_object_set_data (G_OBJECT (item), "number", number);

  gtk_list_item_set_child (item, box);
}

static void
strings_bind_item (GtkSignalListItemFactory *factory,
                   GtkListItem              *item,
                   gpointer                  data)
{
  GtkWidget *image, *name, *number;
  StringHolder *holder;
  MatchObject *match;

  match = MATCH_OBJECT (gtk_list_item_get_item (item));
  holder = STRING_HOLDER (match_object_get_item (match));

  name = g_object_get_data (G_OBJECT (item), "name");
  image = g_object_get_data (G_OBJECT (item), "image");
  number = g_object_get_data (G_OBJECT (item), "number");

  gtk_label_set_label (GTK_LABEL (name), holder->name);
  if (image) {
    if (holder->icon)
      gtk_image_set_from_pixbuf (GTK_IMAGE (image), holder->icon);
    else {
      gtk_image_set_from_icon_name (GTK_IMAGE (image), "avatar-default-symbolic");
    }
  }
  if (number) {
    gtk_label_set_label (GTK_LABEL (number), holder->number_str);
  }
}

static char *
get_name (gpointer item)
{
  StringHolder *holder = STRING_HOLDER (item);

  return g_strdup (holder->name);
}

void
roger_contact_search_completion_add (GtkWidget *entry)
{
  GtkListItemFactory *factory;
  GListStore *store;
  StringHolder *holder;
  GtkExpression *expression;
  GdkPixbuf *pixbuf;
  /* GtkWidget *avatar = gtk_image_new_from_icon_name ("avatar-default-symbolic"); */
  RmAddressBook *book;
  GList *list;

  book = rm_profile_get_addressbook (rm_profile_get_active ());
  if (!book) {
    GList *book_plugins = rm_addressbook_get_plugins ();

    if (book_plugins)
      book = book_plugins->data;
  }

  list = rm_addressbook_get_contacts (book);

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                            0, NULL,
                                            (GCallback)get_name,
                                            NULL, NULL);
  suggestion_entry_set_expression (SUGGESTION_ENTRY (entry), expression);

  store = g_list_store_new (STRING_TYPE_HOLDER);

  while (list && list->data) {
    RmContact *contact = list->data;
    GList *numbers;

    if (contact->image) {
      pixbuf = rm_image_scale (contact->image, 32);
    } else {
      /*GtkWidget *avatar = adw_avatar_new (32, contact->name, TRUE); */
      /*pixbuf = adw_avatar_draw_to_pixbuf (ADW_AVATAR (avatar), 32, 1); */
      pixbuf = NULL;
    }

    for (numbers = contact->numbers; numbers != NULL; numbers = numbers->next) {
      RmPhoneNumber *phone_number = numbers->data;
      char *num_str = g_strdup_printf ("%s: %s", phone_number_type_to_string (phone_number), phone_number->number);

      holder = string_holder_new (contact->name, pixbuf, phone_number->number, num_str);
      g_list_store_append (store, holder);
      g_object_unref (holder);
    }

    list = list->next;
  }

  suggestion_entry_set_model (SUGGESTION_ENTRY (entry), G_LIST_MODEL (store));
  g_object_unref (store);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (strings_setup_item_full), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (strings_bind_item), NULL);
  suggestion_entry_set_factory (SUGGESTION_ENTRY (entry), factory);
  suggestion_entry_set_use_filter (SUGGESTION_ENTRY (entry), TRUE);
  suggestion_entry_set_match_func (SUGGESTION_ENTRY (entry), contact_search_completion_match_func, NULL, NULL);
}
