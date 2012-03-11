/* ProcText - convert.c
 *
 * (c) Stephen Fryatt, 2009-2011
 */

/* ANSI C header files */

#include <string.h>

/* Acorn C header files */

/* OSLib header files */

#include "oslib/os.h"
#include "oslib/wimp.h"

/* SF-Lib header files. */

#include "sflib/config.h"
#include "sflib/debug.h"
#include "sflib/errors.h"
#include "sflib/event.h"
#include "sflib/icons.h"
#include "sflib/menus.h"
#include "sflib/msgs.h"
#include "sflib/string.h"
#include "sflib/windows.h"

/* Application header files */

#include "convert.h"

#include "dataxfer.h"
#include "ihelp.h"
#include "process.h"
#include "templates.h"

/* Program Info Window */

#define ICON_CONVERT_SAVE 0
#define ICON_CONVERT_CANCEL 1
#define ICON_CONVERT_FILENAME 2
#define ICON_CONVERT_FILE 3
#define ICON_CONVERT_SCRIPTMENU 4
#define ICON_CONVERT_SCRIPT 5

#define SCRIPT_MENU_TITLE_LEN 64


static void		convert_click_handler(wimp_pointer *pointer);
static void		convert_menu_prepare_handler(wimp_w window, wimp_menu *menu, wimp_pointer *pointer);
static osbool		convert_immediate_window_save(void);
static void		convert_process_and_save(char *filename);
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

	event_add_window_icon_popup(convert_window, ICON_CONVERT_SCRIPTMENU, convert_script_menu, ICON_CONVERT_SCRIPT, NULL);
}



/**
 * Load a file for conversion, and open the conversion dialogue.
 *
 * \param *filename	The file to load.
 */

void convert_load_file(char *filename)
{
	wimp_pointer	pointer;
	wimp_menu	*popup;
	os_error	*error;

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
		return;

	/* Create a new conversion job for the new file. */

	convert_data = process_create();

	if (convert_data == NULL) {
		process_destroy_script_file(convert_file);
		convert_file = NULL;
		return;
	}

	process_load_file(convert_data, filename);

	/* Set up and open the conversion dialogue. */

	popup = convert_script_menu_build();
	event_set_window_icon_popup_menu(convert_window, ICON_CONVERT_SCRIPTMENU, popup);

	convert_script = 0;

	icons_printf(convert_window, ICON_CONVERT_FILENAME, "%s2", filename);
	event_set_window_icon_popup_selection(convert_window, ICON_CONVERT_SCRIPTMENU, convert_script);

	error = xwimp_get_pointer_info(&pointer);
	if (error == NULL)
		windows_open_centred_at_pointer(convert_window, &pointer);
}


/**
 * Process and save the currently loaded file.
 *
 * \param *filename	The file to save as.
 * \return		0 if the save was started OK.
 */

int convert_save_file(char *filename)
{
	convert_process_and_save(filename);
	convert_close_window();

	return 0;
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

	convert_process_and_save(filename);
	convert_close_window();

	return TRUE;
}


/**
 * Run the selected conversion on the current file, and save the result out.
 *
 * \param *filename		The filename of the file to take the result.
 */

static void convert_process_and_save(char *filename)
{
	int	swi;

	convert_logger_available = (xos_swi_number_from_string("Report_Text0", &swi) == NULL) ? TRUE : FALSE;

	convert_logger("\\GConverting file");

	convert_script = event_get_window_icon_popup_selection(convert_window, ICON_CONVERT_SCRIPTMENU);

	process_run_script(convert_data, convert_file, convert_script, 1, convert_logger);
	process_save_file(convert_data, filename);
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
			start_save_window_drag(DRAGTYPE_CONVERT, convert_window, ICON_CONVERT_FILE,
					string_find_leafname(icons_get_indirected_text_addr(convert_window, ICON_CONVERT_FILENAME)));
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
		templates_set_menu(TEMPLATES_MENU_SCRIPT_POPUP, convert_script_menu);
	}
}


/**
 * Build a menu of scripts associated with the current conversion.
 */

static wimp_menu *convert_script_menu_build(void)
{
	int	line, scripts, width;
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

	/* Populate the menu. */

	line = 0;
	width = 0;

	while (line < scripts) {
		/* Set up the link data.  A copy of the name is taken, because the original is in a flex block and could
		 * well move while the menu is open.  The account number is also stored, to allow the account to be found.
		 */

		name = strdup(process_script_file_name(convert_file, line));

		if (name == NULL)
			name = "?";

		if (strlen(name) > width)
			width = strlen(name);

		/* Set the menu and icon flags up. */

		convert_script_menu->entries[line].menu_flags = 0;

		convert_script_menu->entries[line].sub_menu = (wimp_menu *) -1;
		convert_script_menu->entries[line].icon_flags = wimp_ICON_TEXT | wimp_ICON_FILLED | wimp_ICON_INDIRECTED |
				wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT |
				wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT;

		/* Set the menu icon contents up. */

		convert_script_menu->entries[line].data.indirected_text.text = name;
		convert_script_menu->entries[line].data.indirected_text.validation = NULL;
		convert_script_menu->entries[line].data.indirected_text.size = strlen(name);

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

	return;
	if (convert_script_menu != NULL) {
		do {
			if (convert_script_menu->entries[line].data.indirected_text.text != NULL)
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

