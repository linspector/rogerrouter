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

#pragma once

#include <gtk/gtk.h>

#include "preferences.h"

G_BEGIN_DECLS

/* Those typedefs needs to be moved to librm */
typedef enum {
  RM_CONTROLLER_ISDN1,
  RM_CONTROLLER_ISDN2,
  RM_CONTROLLER_SYSTEM_ISDN,
  RM_CONTROLLER_SYSTEM_ANALOG,
  RM_CONTROLLER_INTERNET1,
  RM_CONTROLLER_INTERNET2
} RmController;

typedef enum {
  RM_RESOLUTION_LOW,
  RM_RESOLUTION_HIGH
} RmResolution;

typedef enum {
  RM_SERVICE_ANALOG,
  RM_SERVICE_ISDN
} RmService;
/* END - Those typedefs needs to be moved to librm */

void roger_preferences_setup_telephony (RogerPreferencesWindow *self);

G_END_DECLS
