/*
 * Roger Router Copyright (c) 2012-2021 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <rm/rm.h>

G_BEGIN_DECLS

typedef enum {
  RM_NOTIFICATION_TYPE_NONE,
  RM_NOTIFICATION_TYPE_INCOMING,
  RM_NOTIFICATION_TYPE_OUTGOING,
  RM_NOTIFICATION_TYPE_BOTH
} RmNotificationType;


struct _RogerPreferencesWindow {
  AdwPreferencesWindow parent_instance;

  RmProfile *profile;

  GtkWidget *host;
  GtkWidget *login_user;
  GtkWidget *login_password;

  GtkWidget *external_code;
  GtkWidget *international_code;
  GtkWidget *country_code;
  GtkWidget *national_code;
  GtkWidget *area_code;

  GtkWidget *softphone;
  GtkWidget *softfax;

  GtkWidget *softphone_group;
  GtkWidget *softphone_number;
  GtkWidget *softphone_controller;
  GtkWidget *softfax_group;
  GtkWidget *softfax_number;
  GtkWidget *softfax_controller;
  GtkWidget *softfax_header;
  GtkWidget *softfax_ident;
  GtkWidget *softfax_resolution;
  GtkWidget *softfax_service;
  GtkWidget *softfax_report;
  GtkWidget *softfax_directory;
  GtkWidget *softfax_ecm;

  GtkWidget *notification_incoming;
  GtkWidget *notification_outgoing;
  GtkWidget *filter;

  GtkWidget *ringtone;
  GtkWidget *microphone;
  GtkWidget *speaker;
  GtkWidget *ringer;

  GtkWidget *plugins_listbox;
};

#define ROGER_TYPE_PREFERENCES_WINDOW (roger_preferences_window_get_type ())

G_DECLARE_FINAL_TYPE (RogerPreferencesWindow, roger_preferences_window, ROGER, PREFERENCES_WINDOW, AdwPreferencesWindow)

RogerPreferencesWindow *roger_preferences_window_new (void);

G_END_DECLS
