/* Copyright 2009-2012, Stephen Fryatt
 *
 * This file is part of ProcText:
 *
 *   http://www.stevefryatt.org.uk/software/
 *
 * Licensed under the EUPL, Version 1.1 only (the "Licence");
 * You may not use this work except in compliance with the
 * Licence.
 *
 * You may obtain a copy of the Licence at:
 *
 *   http://joinup.ec.europa.eu/software/page/eupl
 *
 * Unless required by applicable law or agreed to in
 * writing, software distributed under the Licence is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the Licence for the specific language governing
 * permissions and limitations under the Licence.
 */

/**
 * \file: dataxfer.c
 *
 * Save dialogue and data transfer implementation.
 */

/* ANSI C header files */

#include <string.h>
#include <stdio.h>

/* Acorn C header files */

/* OSLib header files */

#include "oslib/wimp.h"
#include "oslib/dragasprite.h"
#include "oslib/osbyte.h"
#include "oslib/wimpspriteop.h"

/* SF-Lib header files. */

#include "sflib/icons.h"
#include "sflib/string.h"
#include "sflib/windows.h"
#include "sflib/transfer.h"
#include "sflib/debug.h"
#include "sflib/errors.h"
#include "sflib/event.h"
#include "sflib/general.h"
#include "sflib/config.h"

/* Application header files */

#include "dataxfer.h"

#include "convert.h"
#include "main.h"
#include "templates.h"

/* ==================================================================================================================
 * Global variables.
 */

/**
 * Boolean to indicate whether DragASprite is in use or not.
 */

static int			dragging_sprite = 0;

/**
 * The type of save drag: to differentiate Save PDF from Save As.
 */

static enum dataxfer_dragtype	drag_type = 0;

static char			*drag_save_leafname = NULL;

/**
 * Function prototypes.
 */

static void		terminate_user_drag(wimp_dragged *drag, void *data);
static osbool		message_data_save_reply(wimp_message *message);
static osbool		message_data_save_ack_reply(wimp_message *message);
static osbool		message_data_load_reply(wimp_message *message);


/**
 * Initialise the data transfer system.
 */

void dataxfer_initialise(void)
{
	event_add_message_handler(message_DATA_SAVE, EVENT_MESSAGE_INCOMING, message_data_save_reply);
	event_add_message_handler(message_DATA_SAVE_ACK, EVENT_MESSAGE_INCOMING, message_data_save_ack_reply);
	event_add_message_handler(message_DATA_LOAD, EVENT_MESSAGE_INCOMING, message_data_load_reply);
}


/**
 * Start dragging the icon from the save dialogue.  Called in response to an attempt to drag the icon.
 *
 * \param type		The drag type to start.
 * \param w		The window where the drag is starting.
 * \param i		The icon to be dragged.
 * \param *filename	The filename to be used as a starting point.
 */

void start_save_window_drag(enum dataxfer_dragtype type, wimp_w w, wimp_i i, char *filename)
{
	wimp_window_state	window;
	wimp_icon_state		icon;
	wimp_drag		drag;
	int			ox, oy;

	/* Get the basic information about the window and icon. */

	window.w = w;
	wimp_get_window_state (&window);

	ox = window.visible.x0 - window.xscroll;
	oy = window.visible.y1 - window.yscroll;

	icon.w = window.w;
	icon.i = i;
	wimp_get_icon_state (&icon);

	/* Set up the drag parameters. */

	drag.w = window.w;
	drag.type = wimp_DRAG_USER_FIXED;

	drag.initial.x0 = ox + icon.icon.extent.x0;
	drag.initial.y0 = oy + icon.icon.extent.y0;
	drag.initial.x1 = ox + icon.icon.extent.x1;
	drag.initial.y1 = oy + icon.icon.extent.y1;

	drag.bbox.x0 = 0x80000000;
	drag.bbox.y0 = 0x80000000;
	drag.bbox.x1 = 0x7fffffff;
	drag.bbox.y1 = 0x7fffffff;


	/* Read CMOS RAM to see if solid drags are required. */

	dragging_sprite = ((osbyte2 (osbyte_READ_CMOS, osbyte_CONFIGURE_DRAG_ASPRITE, 0) &
			osbyte_CONFIGURE_DRAG_ASPRITE_MASK) != 0);

	if (dragging_sprite)
		dragasprite_start (dragasprite_HPOS_CENTRE | dragasprite_VPOS_CENTRE |
			dragasprite_NO_BOUND | dragasprite_BOUND_POINTER | dragasprite_DROP_SHADOW,
			wimpspriteop_AREA, icon.icon.data.indirected_text.text, &(drag.initial), &(drag.bbox));
	else
		wimp_drag_box (&drag);

	drag_save_leafname = string_find_leafname(filename);

	drag_type = type;
	event_set_drag_handler(terminate_user_drag, NULL, NULL);
}


/**
 * Callback handler for queue window drag termination.
 *
 * Start a data-save dialogue with the application at the other end.
 *
 * \param  *drag		The Wimp poll block from termination.
 * \param  *data		NULL (unused).
 */

static void terminate_user_drag(wimp_dragged *drag, void *data)
{
	wimp_pointer		pointer;

	if (dragging_sprite)
		dragasprite_stop ();

	wimp_get_pointer_info (&pointer);

	switch (drag_type) {
	case DRAGTYPE_CONVERT:
		transfer_save_start_callback(pointer.w, pointer.i, pointer.pos, 0, convert_save_file, 0, TEXT_FILE_TYPE, drag_save_leafname);
		break;
	default:
		break;
	}
}


/**
 * Handle the receipt of a Message_DataSaveAck, usually in response to another
 * application trying to save a file to our icon.  Supply the location of the
 * queue head file, which allows postscript printer drivers to be set up by
 * dragging their save icon to our iconbar.
 *
 * \param *message		The associated Wimp message block.
 * \return			TRUE to show that the message was handled.
 */

static osbool message_data_save_reply(wimp_message *message)
{
	wimp_full_message_data_xfer	*datasave = (wimp_full_message_data_xfer *) message;
	os_error			*error;

	if (message->sender == main_task_handle) /* We don't want to respond to our own save requests. */
		return TRUE;


	if (datasave->w == wimp_ICON_BAR && datasave->file_type == TEXT_FILE_TYPE) {
		datasave->your_ref = datasave->my_ref;
		datasave->action = message_DATA_SAVE_ACK;

		switch (datasave->file_type) {
		case TEXT_FILE_TYPE:
			strcpy(datasave->file_name, "<Wimp$Scrap>");
			break;
		}

		datasave->size = WORDALIGN(45 + strlen(datasave->file_name));

		error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message *) datasave, datasave->sender);
		if (error != NULL)
			error_report_os_error(error, wimp_ERROR_BOX_CANCEL_ICON);
	}

	return TRUE;
}


/**
 * Handle the receipt of a Message_DataSaveAck.
 *
 * \param *message		The associated Wimp message block.
 * \return			TRUE to show that the message was handled.
 */

static osbool message_data_save_ack_reply(wimp_message *message)
{
	transfer_save_reply_datasaveack(message);

	return TRUE;
}


/**
 * Handle the receipt of a Message_DataLoad, generally as a result of a file
 * being dragged from the Filer to one of our windows or icons.
 *
 * \param *message		The associated Wimp message block.
 * \return			TRUE to show the message was handled.
 */

static osbool message_data_load_reply(wimp_message *message)
{
	wimp_full_message_data_xfer	*dataload = (wimp_full_message_data_xfer *) message;
	os_error			*error;
	osbool				handled = FALSE;

	if (dataload->w == wimp_ICON_BAR && dataload->file_type == TEXT_FILE_TYPE) {
		switch (dataload->file_type) {
		case TEXT_FILE_TYPE:
			convert_load_file(dataload->file_name);
			break;
		}

		/* Reply with a Message_DataLoadAck. */

		dataload->your_ref = dataload->my_ref;
		dataload->action = message_DATA_LOAD_ACK;

		error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message *) dataload, dataload->sender);
		if (error != NULL)
			error_report_os_error(error, wimp_ERROR_BOX_CANCEL_ICON);

		handled = TRUE;
	}

	return handled;
}

