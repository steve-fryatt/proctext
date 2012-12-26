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
 * \file: convert.h
 *
 * File conversion dialogue implementation.
 */

#ifndef PROCTEXT_CONVERT
#define PROCTEXT_CONVERT

/**
 * Initialise the file conversion dialogue.
 */

void convert_initialise(void);


/**
 * Load a file for conversion, and open the conversion dialogue.
 *
 * \param *filename	The file to load.
 */

void convert_load_file(char *filename);


/**
 * Process and save the currently loaded file.
 *
 * \param *filename	The file to save as.
 * \return		0 if the save was started OK.
 */

int convert_save_file(char *filename);



#endif

