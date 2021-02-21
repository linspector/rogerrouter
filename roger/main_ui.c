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

#include "application.h"

#include <gtk/gtk.h>
#include <rm/rm.h>
#include <stdlib.h>
#include <string.h>

GtkApplication *application_new (void);
char *argv0 = NULL;

int
main (int    argc,
      char **argv)
{
  int status;

  argv0 = g_strdup (argv[0]);

  roger_app = application_new ();

  gdk_notify_startup_complete ();
  status = g_application_run (G_APPLICATION (roger_app), argc, argv);

  g_object_unref (roger_app);

  return status;
}
