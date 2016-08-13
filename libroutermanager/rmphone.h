/**
 * The libroutermanager project
 * Copyright (c) 2012-2016 Jan-Michael Brummer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef LIBROUTERMANAGER_PHONE_H
#define LIBROUTERMANAGER_PHONE_H

G_BEGIN_DECLS

struct device_phone {
	gchar *name;
	struct connection *(*dial)(const gchar *target, gboolean anonymous);
	gint (*pickup)(struct connection *connection);
	void (*hangup)(struct connection *connection);
	void (*hold)(struct connection *connection, gboolean hold);
	void (*send_dtmf_code)(struct connection *connection, guchar code);

	gboolean (*number_is_handled)(gchar *number);

	void (*mute)(struct connection *connection, gboolean mute);
	/*void (*record)(gpointer connection, guchar hold, const gchar *dir);*/
};

void rm_phone_register(struct device_phone *phone);
GSList *rm_phone_get_plugins(void);
void rm_phone_mute(struct connection *connection, gboolean mute);
void rm_phone_hold(struct connection *connection, gboolean hold);
void rm_phone_dtmf(struct connection *connection, guchar code);
void rm_phone_hangup(struct connection *connection);
gint rm_phone_pickup(struct connection *connection);
struct connection *rm_phone_dial(const gchar *target, gboolean anonymous);

G_END_DECLS

#endif