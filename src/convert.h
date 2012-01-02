/* ProcText - convert.h
 *
 * (c) Stephen Fryatt, 2009-2011
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

