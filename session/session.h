/*
 * This file is part of graphene-desktop, the desktop environment of VeltOS.
 * Copyright (C) 2016 Velt Technologies, Aidan Shafran <zelbrium@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GRAPHENE_SESSION_H__
#define __GRAPHENE_SESSION_H__

#include <glib.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef void (*CSMStartupCompleteCallback)(gpointer userdata);
typedef void (*CSMQuitCallback)(gboolean failed, gpointer userdata);

void graphene_session_init(CSMStartupCompleteCallback startupCb, CSMQuitCallback quitCb, void *userdata);

/*
 * Starts the logout phase, which asks all clients to close and eventually
 * ends the session. If any clients reject the logout, the phase is cancelled.
 * A successful logout will call CSMQuitCallback with failed set to FALSE.
 */
void graphene_session_logout();

/*
 * Immediately exits the session, closing all programs without saving.
 * This will call the CSMQuitCallback with failed set to TRUE.
 */
void graphene_session_exit();

G_END_DECLS

#endif /* __GRAPHENE_SESSION_H__ */
