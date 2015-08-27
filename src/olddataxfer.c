/* Copyright 2009-2015, Stephen Fryatt
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

#include "olddataxfer.h"

#include "convert.h"
#include "main.h"
#include "templates.h"

/* ==================================================================================================================
 * Global variables.
 */


/**
 * Function prototypes.
 */

static osbool		message_data_save_reply(wimp_message *message);
static osbool		message_data_save_ack_reply(wimp_message *message);
static osbool		message_data_load_reply(wimp_message *message);


/**
 * Initialise the data transfer system.
 */

void olddataxfer_initialise(void)
{
	event_add_message_handler(message_DATA_SAVE, EVENT_MESSAGE_INCOMING, message_data_save_reply);
	event_add_message_handler(message_DATA_LOAD, EVENT_MESSAGE_INCOMING, message_data_load_reply);
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

