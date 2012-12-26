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
 * \file: process.h
 *
 * Text processing and manipulation.
 */

#ifndef PROCTEXT_PROCESS
#define PROCTEXT_PROCESS

/* ==================================================================================================================
 * Static constants
 */

struct process_data;
struct process_file;


/**
 * Initialise the file processing system.
 */

void process_initialise(void);


/**
 * Create a new, empty file data block and return its handle.
 *
 * \return		The new block handle, or NULL if failed.
 */

struct process_data *process_create(void);


/**
 * Destroy a file data block.
 *
 * \param *data		The block to be destroyed.
 */

void process_destroy(struct process_data *data);


/**
 * Load the file to be processed into memory, allocating space with malloc()
 * and returning the pointer.
 *
 * \param data		pointer to the memory data structure to load the file into.
 * \param filename	the file name of the file to load.
 */

void process_load_file(struct process_data *data, char *filename);


/**
 * Save a data block out to a file.
 *
 * \param data		pointer to the memory data structure to save to file.
 * \param filename	the file to save the data to.
 */

void process_save_file(struct process_data *data, char *filename);


/**
 * Run a script process on a data block.
 *
 * \param *data		The data block to be processed.
 * \param *file		The handle of the file containing the script to be used.
 * \param script	The number of the script within the file.
 * \param verbosity	The level of logging verbosity, larger == more.
 * \param *logger	A callback to handle logging output.
 */

void process_run_script(struct process_data *data, struct process_file *file, int script, int verbosity, void (logger)(char *));


/**
 * Load a script file, registering the scripts and returning its handle.
 *
 * \param *filename		The filename of the script file to be loaded.
 * \return			The handle of the script file or NULL on failure.
 */

struct process_file *process_load_script_file(char *filename);


/**
 * Destroy the data associated with a script file handle and free up any
 * memory that it is using.
 *
 * \param *file			The handle of the script file to be freed.
 */

void process_destroy_script_file(struct process_file *file);


/**
 * Return the number of scripts within a script file.
 *
 * \param *file			The handle of the script file.
 * \return			The number of scripts in the file.
 */

int process_script_file_entries(struct process_file *file);


/**
 * Return the name of a script from within a script file.
 *
 * \param *file			The handle of the script file.
 * \param script		The script to return the name for.
 * \return			A pointer to the name, or NULL.
 */

char *process_script_file_name(struct process_file *file, int script);


#endif

