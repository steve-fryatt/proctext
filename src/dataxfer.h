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
 * \file: dataxfer.h
 *
 * Save dialogue and data transfer implementation.
 */

#ifndef PROCTEXT_DATAXFER
#define PROCTEXT_DATAXFER

/* ==================================================================================================================
 * Static constants
 */


#define TEXT_FILE_TYPE 0xfff

#define DRAG_SAVE_PDF    1
#define DRAG_SAVE_SAVEAS 2

enum dataxfer_dragtype {
	DRAGTYPE_NONE = 0,							/**< No drag is in action.			*/
	DRAGTYPE_CONVERT = 1							/**< Dragging a file to start a conversion.	*/
};

/**
 * Initialise the data transfer system.
 */

void dataxfer_initialise(void);


/**
 * Start dragging the icon from the save dialogue.  Called in response to an attempt to drag the icon.
 *
 * \param type		The drag type to start.
 * \param w		The window where the drag is starting.
 * \param i		The icon to be dragged.
 * \param *filename	The filename to be used as a starting point.
 */

void start_save_window_drag(enum dataxfer_dragtype type, wimp_w w, wimp_i i, char *filename);

#endif

