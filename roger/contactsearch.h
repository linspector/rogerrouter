#ifndef CONTACT_SEARCH_H
#define CONTACT_SEARCH_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CONTACT_TYPE_SEARCH (contact_search_get_type())

G_DECLARE_FINAL_TYPE(ContactSearch, contact_search, CONTACT, SEARCH, GtkBox)

GtkWidget *contact_search_new(void);
gchar *contact_search_get_number(ContactSearch *widget);

G_END_DECLS

#endif
