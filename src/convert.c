/* Copyright 2009-2016, Stephen Fryatt
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
 * \file: convert.c
 *
 * File conversion dialogue implementation.
 */

/* ANSI C header files */

#include <string.h>

/* Acorn C header files */

/* OSLib header files */

#include "oslib/os.h"
#include "oslib/osfile.h"
#include "oslib/wimp.h"

/* SF-Lib header files. */

#include "sflib/config.h"
#include "sflib/dataxfer.h"
#include "sflib/debug.h"
#include "sflib/errors.h"
#include "sflib/event.h"
#include "sflib/icons.h"
#include "sflib/ihelp.h"
#include "sflib/menus.h"
#include "sflib/msgs.h"
#include "sflib/string.h"
#include "sflib/templates.h"
#include "sflib/windows.h"

/* Application header files */

#include "convert.h"

#include "process.h"

/* Program Info Window */

#define ICON_CONVERT_SAVE 0
#define ICON_CONVERT_CANCEL 1
#define ICON_CONVERT_FILENAME 2
#define ICON_CONVERT_FILE 3
#define ICON_CONVERT_SCRIPTMENU 4
#define ICON_CONVERT_SCRIPT 5

#define SCRAP_FILENAME_LEN 64
#define SCRIPT_MENU_TITLE_LEN 64


static osbool		convert_load_file(wimp_w w, wimp_i i, unsigned filetype, char *filename, void *data);
static void		convert_click_handler(wimp_pointer *pointer);
static void		convert_menu_prepare_handler(wimp_w window, wimp_menu *menu, wimp_pointer *pointer);
static void		convert_drag_end_handler(wimp_pointer *pointer, void *data);
static osbool		convert_drag_end_save_handler(char *filename, void *data);
static osbool		convert_immediate_window_save(void);
static osbool		convert_process_and_save(char *filename);
static void		convert_close_window();
static wimp_menu	*convert_script_menu_build(void);
static void		convert_script_menu_destroy(void);
static void		convert_logger(char *text);


static wimp_w			convert_window = NULL;				/**< The handle of the conversion window.			*/
static struct process_file	*convert_file = NULL;				/**< The handle of the current script file, or NULL if none.	*/
static struct process_data	*convert_data = NULL;				/**< The handle of the current file data, or NULL if none.	*/

static int			convert_script = 0;				/**< The script to use in the conversion.			*/
static wimp_menu		*convert_script_menu = NULL;			/**< The menu block for the convert script menu.		*/
static char			*convert_script_menu_title = NULL;		/**< The title data for the convert script menu.		*/

static osbool			convert_logger_available = FALSE;		/**< True if we have a logger available to take output.		*/

/**
 * Initialise the file conversion dialogue.
 */

void convert_initialise(void)
{
	convert_window = templates_create_window("Process");
	ihelp_add_window(convert_window, "Process", NULL);
	event_add_window_mouse_event(convert_window, convert_click_handler);
	event_add_window_menu_prepare(convert_window, convert_menu_prepare_handler);

	dataxfer_set_drop_target(osfile_TYPE_TEXT, wimp_ICON_BAR, -1, NULL, convert_load_file, NULL);

	event_add_window_icon_popup(convert_window, ICON_CONVERT_SCRIPTMENU, convert_script_menu, ICON_CONVERT_SCRIPT, NULL);
}



/**
 * Load a file for conversion, and open the conversion dialogue.
 *
 * \param *filename	The file to load.
 */

static osbool convert_load_file(wimp_w w, wimp_i i, unsigned filetype, char *filename, void *data)
{
	wimp_pointer	pointer;
	wimp_menu	*popup;
	os_error	*error;
	char		*window_filename, scrap_filename[SCRAP_FILENAME_LEN];

	/* Abandon any existing conversion process. */

	if (convert_data != NULL) {
		process_destroy(convert_data);
		convert_data = NULL;
	}

	if (convert_file != NULL) {
		process_destroy_script_file(convert_file);
		convert_file = NULL;
	}

	/* Get details of the script file to use. */

	convert_file = process_load_script_file(config_str_read("ScriptFile"));

	if (convert_file == NULL)
		return TRUE;

	/* Create a new conversion job for the new file. */

	convert_data = process_create();

	if (convert_data == NULL) {
		process_destroy_script_file(convert_file);
		convert_file = NULL;
		return TRUE;
	}

	process_load_file(convert_data, filename);

	/* Set up and open the conversion dialogue. */

	popup = convert_script_menu_build();
	event_set_window_icon_popup_menu(convert_window, ICON_CONVERT_SCRIPTMENU, popup);

	convert_script = 0;

	if (strcmp(filename, "<Wimp$Scrap>") == 0) {
		msgs_lookup("ScrapFilename", scrap_filename, SCRAP_FILENAME_LEN);
		window_filename = scrap_filename;
	} else {
		window_filename = filename;
	}

	icons_printf(convert_window, ICON_CONVERT_FILENAME, "%s2", window_filename);
	event_set_window_icon_popup_selection(convert_window, ICON_CONVERT_SCRIPTMENU, convert_script);

	error = xwimp_get_pointer_info(&pointer);
	if (error == NULL)
		windows_open_centred_at_pointer(convert_window, &pointer);

	return TRUE;
}


/**
 * Process the termination of icon drags from the Convert dialogue.
 *
 * \param *pointer		The pointer location at the end of the drag.
 * \param *data			The saveas_savebox data for the drag.
 */

static void convert_drag_end_handler(wimp_pointer *pointer, void *data)
{
	char			*leafname;

	leafname = string_find_leafname(icons_get_indirected_text_addr(convert_window, ICON_CONVERT_FILENAME));

	dataxfer_start_save(pointer, leafname, 0, osfile_TYPE_TEXT, 0, convert_drag_end_save_handler, NULL);
}


/**
 * Callback handler for DataSave completion on file save drags: start the
 * conversion using the filename returned.
 *
 * \param *filename		The filename returned by the DataSave protocol.
 * \param *data			Data pointer (unused).
 * \return			TRUE if the save was started OK; else FALSE.
 */

static osbool convert_drag_end_save_handler(char *filename, void *data)
{
	if (!convert_process_and_save(filename))
		return FALSE;

	convert_close_window();

	return TRUE;
}


/**
 * Try to save in response to a click on 'OK' in the Save dialogue.
 *
 * \return 		TRUE if the process completed OK; else FALSE.
 */

static osbool convert_immediate_window_save(void)
{
	char			*filename;

	filename = icons_get_indirected_text_addr(convert_window, ICON_CONVERT_FILENAME);

	/* Test if the filename is valid. */

	if (strchr (filename, '.') == NULL) {
		error_msgs_report_info("DragSave");
		return FALSE;
	}

	if (!convert_process_and_save(filename))
		return FALSE;

	convert_close_window();

	return TRUE;
}


/**
 * Run the selected conversion on the current file, and save the result out.
 *
 * \param *filename		The filename of the file to take the result.
 * \return			TRUE if the process completed successfully; FALSE on failure.
 */

static osbool convert_process_and_save(char *filename)
{
	int	swi;
	char	log[128];

	convert_logger_available = (xos_swi_number_from_string("Report_Text0", &swi) == NULL) ? TRUE : FALSE;

	msgs_lookup("Convert", log, 128);
	convert_logger(log);

	convert_script = event_get_window_icon_popup_selection(convert_window, ICON_CONVERT_SCRIPTMENU);

	if (!process_run_script(convert_data, convert_file, convert_script, 1, convert_logger))
		return FALSE;

	if (!process_save_file(convert_data, filename))
		return FALSE;

	return TRUE;
}


/**
 * Close the conversion window, freeing any job data handle in the process.
 */

static void convert_close_window()
{
		wimp_close_window(convert_window);

		if (convert_data != NULL)
			process_destroy(convert_data);

		if (convert_file != NULL)
			process_destroy_script_file(convert_file);

		convert_data = NULL;
		convert_file = NULL;

		convert_script_menu_destroy();
}


/**
 * Handle mouse clicks in the conversion window.
 *
 * \param *pointer		The Wimp mouse click event data.
 */

static void convert_click_handler(wimp_pointer *pointer)
{
	if (pointer == NULL)
		return;

	switch (pointer->i) {
	case ICON_CONVERT_SAVE:
		if (pointer->buttons == wimp_CLICK_SELECT)
			convert_immediate_window_save();
		break;

	case ICON_CONVERT_CANCEL:
		if (pointer->buttons == wimp_CLICK_SELECT)
			convert_close_window();
		break;

	case ICON_CONVERT_FILE:
		if (pointer->buttons == wimp_DRAG_SELECT)
			dataxfer_save_window_drag(pointer->w, ICON_CONVERT_FILE, convert_drag_end_handler, NULL);
		break;
	}
}


/**
 * Handle menu prepare events in the conversion window.
 *
 * \param window		The window containing the menu.
 * \param *menu			The menu to be opened.
 * \param *pointer		The Wimp mouse click event data, or NULL on re-open.
 */

static void convert_menu_prepare_handler(wimp_w window, wimp_menu *menu, wimp_pointer *pointer)
{
	if (pointer != NULL) {
		event_set_menu_block(convert_script_menu);
	}
}


/**
 * Build a menu of scripts associated with the current conversion.
 */

static wimp_menu *convert_script_menu_build(void)
{
	int	line, scripts, width, len;
	char	*name;

	convert_script_menu_destroy();

	if (convert_file == NULL)
		return NULL;

	/* Find out how many scripts there are. */

	scripts = process_script_file_entries(convert_file);

	if (scripts == 0)
		return NULL;

	/* Claim enough memory to build the menu in. */

	convert_script_menu = malloc(28 + (24 * scripts));
	convert_script_menu_title = malloc(SCRIPT_MENU_TITLE_LEN);

	if (convert_script_menu == NULL || convert_script_menu_title == NULL) {
		convert_script_menu_destroy();
		return NULL;
	}

	ihelp_add_menu(convert_script_menu, "ScriptMenu");

	/* Populate the menu. */

	line = 0;
	width = 0;

	while (line < scripts) {
		/* Set up the link data.  A copy of the name is taken, because the original is in a flex block and could
		 * well move while the menu is open.  The account number is also stored, to allow the account to be found.
		 */

		name = strdup(process_script_file_name(convert_file, line));

		len = (name == NULL) ? 1 : strlen(name);

		if (len > width)
			width = len;

		/* Set the menu and icon flags up. */

		convert_script_menu->entries[line].menu_flags = 0;

		convert_script_menu->entries[line].sub_menu = (wimp_menu *) -1;
		convert_script_menu->entries[line].icon_flags = wimp_ICON_TEXT | wimp_ICON_FILLED |
				wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT |
				wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT;

		/* Set the menu icon contents up. */

		if (name != NULL) {
			convert_script_menu->entries[line].data.indirected_text.text = name;
			convert_script_menu->entries[line].data.indirected_text.validation = NULL;
			convert_script_menu->entries[line].data.indirected_text.size = strlen(name);
			convert_script_menu->entries[line].icon_flags |= wimp_ICON_INDIRECTED;
		} else {
			strncpy(convert_script_menu->entries[line].data.text, "?", 12);
		}

		line++;
	}

	convert_script_menu->entries[line - 1].menu_flags |= wimp_MENU_LAST;

	msgs_lookup("ScriptMenuTitle", convert_script_menu_title, SCRIPT_MENU_TITLE_LEN);
	convert_script_menu->title_data.indirected_text.text = convert_script_menu_title;
	convert_script_menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	convert_script_menu->title_fg = wimp_COLOUR_BLACK;
	convert_script_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	convert_script_menu->work_fg = wimp_COLOUR_BLACK;
	convert_script_menu->work_bg = wimp_COLOUR_WHITE;

	convert_script_menu->width = (width + 1) * 16;
	convert_script_menu->height = 44;
	convert_script_menu->gap = 0;

	return convert_script_menu;
}


/**
 * Destroy any Script List menu which is currently open.
 */

static void convert_script_menu_destroy(void)
{
	int	line = 0;

	ihelp_remove_menu(convert_script_menu);

	if (convert_script_menu != NULL) {
		do {
			if ((convert_script_menu->entries[line].icon_flags & wimp_ICON_INDIRECTED) != 0 &&
					convert_script_menu->entries[line].data.indirected_text.text != NULL)
				free(convert_script_menu->entries[line].data.indirected_text.text);
		} while ((convert_script_menu->entries[line++].menu_flags & wimp_MENU_LAST) == 0);

		free(convert_script_menu);
	}

	if (convert_script_menu_title != NULL)
		free(convert_script_menu_title);

	convert_script_menu = NULL;
	convert_script_menu_title = NULL;
}


/**
 * Provide a route back from the process module for conversion progress
 * details.
 *
 * \param *text			The text to be logged.
 */

static void convert_logger(char *text)
{
	if (convert_logger_available)
		debug_printf(text);
}

