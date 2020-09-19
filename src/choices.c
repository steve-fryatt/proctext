/* Copyright 2016, Stephen Fryatt (info@stevefryatt.org.uk)
 *
 * This file is part of ProcText:
 *
 *   http://www.stevefryatt.org.uk/software/
 *
 * Licensed under the EUPL, Version 1.2 only (the "Licence");
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
 * \file: choices.c
 *
 * Choices dialogue implementation.
 */

/* ANSI C Header files. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Acorn C Header files. */


/* OSLib Header files. */

#include "oslib/osbyte.h"
#include "oslib/osfile.h"
#include "oslib/wimp.h"

/* SF-Lib Header files. */

#include "sflib/config.h"
#include "sflib/errors.h"
#include "sflib/event.h"
#include "sflib/heap.h"
#include "sflib/icons.h"
#include "sflib/ihelp.h"
#include "sflib/windows.h"
#include "sflib/debug.h"
#include "sflib/string.h"
#include "sflib/templates.h"

/* Application header files. */

#include "choices.h"

#include "search.h"


/* Choices window icons. */

#define CHOICE_ICON_APPLY 0
#define CHOICE_ICON_SAVE 1
#define CHOICE_ICON_CANCEL 2
#define CHOICE_ICON_SCRIPT_FILE 5
#define CHOICE_ICON_DEFAULT_SCRIPT_POPUP 7
#define CHOICE_ICON_DEFAULT_SCRIPT 8


/* Global variables */

static wimp_w			choices_window = NULL;

static void	choices_close_window(void);
static void	choices_set_window(void);
static osbool	choices_read_window(void);
static void	choices_redraw_window(void);

static void	choices_click_handler(wimp_pointer *pointer);
static osbool	choices_keypress_handler(wimp_key *key);

static osbool handle_choices_icon_drop(wimp_message *message);


/**
 * Initialise the Choices module.
 */

void choices_initialise(void)
{
	wimp_window	*def = templates_load_window("Choices");

	if (def == NULL)
		error_msgs_report_fatal("BadTemplate");

	choices_window = wimp_create_window(def);
	free(def);
	ihelp_add_window(choices_window, "Choices", NULL);

	event_add_window_mouse_event(choices_window, choices_click_handler);
	event_add_window_key_event(choices_window, choices_keypress_handler);

	event_add_message_handler(message_DATA_LOAD, EVENT_MESSAGE_INCOMING, handle_choices_icon_drop);
}


/**
 * Open the Choices window at the mouse pointer.
 *
 * \param *pointer		The details of the pointer to open the window at.
 */

void choices_open_window(wimp_pointer *pointer)
{
	if (windows_get_open(choices_window))
		return;

	choices_set_window();

	windows_open_centred_at_pointer(choices_window, pointer);

	icons_put_caret_at_end(choices_window, CHOICE_ICON_SCRIPT_FILE);
}


/**
 * Close the choices window.
 */

static void choices_close_window(void)
{
	wimp_close_window(choices_window);
}


/**
 * Set the contents of the Choices window to reflect the current settings.
 */

static void choices_set_window(void)
{
	icons_printf(choices_window, CHOICE_ICON_SCRIPT_FILE, "%s", config_str_read("ScriptFile"));
}


/**
 * Update the configuration settings from the values in the Choices window.
 *
 * \return			TRUE if successful; FALSE if validation errors occurred.
 */

static osbool choices_read_window(void)
{
	config_str_set("ScriptFile", icons_get_indirected_text_addr(choices_window, CHOICE_ICON_SCRIPT_FILE));

	return TRUE;
}


/**
 * Refresh the Choices dialogue, to reflech changed icon states.
 */

static void choices_redraw_window(void)
{
	wimp_set_icon_state(choices_window, CHOICE_ICON_SCRIPT_FILE, 0, 0);

	icons_replace_caret_in_window(choices_window);
}


/**
 * Process mouse clicks in the Choices dialogue.
 *
 * \param *pointer		The mouse event block to handle.
 */

static void choices_click_handler(wimp_pointer *pointer)
{
	if (pointer == NULL)
		return;

	switch ((int) pointer->i) {
	case CHOICE_ICON_APPLY:
		if (pointer->buttons == wimp_CLICK_SELECT || pointer->buttons == wimp_CLICK_ADJUST) {
			if (!choices_read_window())
				break;

			if (pointer->buttons == wimp_CLICK_SELECT)
				choices_close_window();
		}
		break;

	case CHOICE_ICON_SAVE:
		if (pointer->buttons == wimp_CLICK_SELECT || pointer->buttons == wimp_CLICK_ADJUST) {
			if (!choices_read_window())
				break;
			config_save();

			if (pointer->buttons == wimp_CLICK_SELECT)
				choices_close_window();
		}
		break;

	case CHOICE_ICON_CANCEL:
		if (pointer->buttons == wimp_CLICK_SELECT) {
			choices_close_window();
		} else if (pointer->buttons == wimp_CLICK_ADJUST) {
			choices_set_window();
			choices_redraw_window();
		}
		break;
	}
}


/**
 * Process keypresses in the Choices window.
 *
 * \param *key		The keypress event block to handle.
 * \return		TRUE if the event was handled; else FALSE.
 */

static osbool choices_keypress_handler(wimp_key *key)
{
	if (key == NULL)
		return FALSE;

	switch (key->c) {
	case wimp_KEY_RETURN:
		choices_read_window();
		config_save();
		choices_close_window();
		break;

	case wimp_KEY_ESCAPE:
		choices_close_window();
		break;

	default:
		return FALSE;
		break;
	}

	return TRUE;
}


/**
 * Check incoming Message_DataSave to see if it's a file being dropped into the
 * the search path icon.
 *
 * \param *message		The incoming message block.
 * \return			TRUE if we claim the message as intended for us; else FALSE.
 */

static osbool handle_choices_icon_drop(wimp_message *message)
{
	wimp_full_message_data_xfer	*datasave = (wimp_full_message_data_xfer *) message;


	/* If it isn't our window, don't claim the message as someone else
	 * might want it.
	 */

	if (datasave == NULL || datasave->w != choices_window)
		return FALSE;

	/* If it is our window, but not the icon or filetype that  we
	 * care about, claim the message.
	 */

	if (datasave->i != CHOICE_ICON_SCRIPT_FILE || datasave->file_type != osfile_TYPE_TEXT)
		return TRUE;

	/* It's our window and the correct icon, so copy the filename and refresh the icon. */

	icons_printf(datasave->w, datasave->i, "%s", datasave->file_name);

	icons_replace_caret_in_window(datasave->w);
	wimp_set_icon_state(datasave->w, datasave->i, 0, 0);

	return TRUE;
}
