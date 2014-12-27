/* Copyright 2009-2014, Stephen Fryatt
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
 * \file: process.c
 *
 * Text processing and manipulation.
 */

/* ANSI C header files */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Acorn C header files */

/* OSLib header files */

#include "oslib/types.h"

/* SF-Lib header files. */

#include "sflib/config.h"
#include "sflib/string.h"

/* Application header files */

#include "process.h"

#define ACTIONS_ALLOC_BLOCK  20
#define FILE_ALLOC_BLOCK     1024
#define PROCESS_LOG_LINE_LEN 1024

enum process_action_type {
	ACTION_NONE = 0,
	ACTION_SUBSTITUTE,
	ACTION_SUBSTITUTE_CASE_SENSITIVE,
	ACTION_REDUCE,
	ACTION_COMMAND
};

struct process_data {
	char				*mem;

	int				size;
	int				len;
};

/* Data Structure containing details of a conversion action.
 */

struct process_action {
	enum process_action_type	type;

	char				*from;
	char				*to;

	int				minimum;
	int				maximum;
};

struct process_script {
	char				*name;
};

struct process_file {
	char				*filename;				/**< The filename of the script file.			*/
	int				count;					/**< The number of scripts in the script file.		*/

	struct process_script		*scripts;				/**< An array of script details for the file.		*/
};

static int run_reduce(struct process_data *data, char *str, char *to, int minimum, int maximum, int verbosity, void (logger)(char *));
static int run_substitute(struct process_data *data, char *str, char *to, osbool cs, int verbosity, void (logger)(char *));
static int test_memory(char *mem, char *str, osbool cs);
static int substitute_text(struct process_data *data, char *position, int length, char *to);

static int load_script(char *file, char *script, struct process_action **actions, void (logger)(char *));
static void add_new_action(struct process_action **actions, int *items, int *size, int type, char *from, char *to, int min, int max);
char *str_char_cpy(char *str1, char *str2);

static int change_memory_allocation(struct process_data *data, int expand);

static char *write_ctrl_chars(char *buf, char *str, int len);


/* --------------------------------------------------------------------------------------------------------------------
 *
 */


/**
 * Initialise the file processing system.
 */

void process_initialise(void)
{
}


/**
 * Create a new, empty file data block and return its handle.
 *
 * \return		The new block handle, or NULL if failed.
 */

struct process_data *process_create(void)
{
	struct process_data *data = NULL;

	data = malloc(sizeof(struct process_data));

	if (data != NULL) {
		data->mem = NULL;
		data->size = 0;
		data->len = 0;
	}

	return data;
}


/**
 * Destroy a file data block.
 *
 * \param *data		The block to be destroyed.
 */

void process_destroy(struct process_data *data)
{
	if (data == NULL)
		return;

	if (data->mem != NULL)
		free(data->mem);

	free(data);
}


/**
 * Load the file to be processed into memory, allocating space with malloc()
 * and returning the pointer.
 *
 * \param data		pointer to the memory data structure to load the file into.
 * \param filename	the file name of the file to load.
 */

void process_load_file(struct process_data *data, char *filename)
{
	FILE	*f;
	char	*ptr = NULL;
	int	c;

	if (data == NULL || filename == NULL || *filename == '\0' || !change_memory_allocation(data, FILE_ALLOC_BLOCK))
		return;

	ptr = data->mem + data->len;

	f = fopen(filename, "r");
	if (f == NULL)
		return;

	/* Read the characters from the file and store them into the data block. */

	while((c = fgetc(f)) != EOF) {
		if (data->len >= data->size) {
			change_memory_allocation(data, FILE_ALLOC_BLOCK);
			ptr = (data->mem != NULL) ? (data->mem + data->len) : NULL;
		}

		if (ptr != NULL && data->len < data->size) {
			*ptr++ = c;
			(data->len)++;
		}
	}

	/* Close the file after loading the data in. */

	fclose(f);

	/* Add a NULL terminator to the end of the block. */

	if (data->len >= data->size) {
		change_memory_allocation(data, FILE_ALLOC_BLOCK);
		ptr = (data->mem != NULL) ? (data->mem + data->len) : NULL;
	}

	if (ptr != NULL && data->len < data->size) {
		*ptr++ = '\0';
		(data->len)++;
	}
}


/**
 * Save a data block out to a file.
 *
 * \param data		pointer to the memory data structure to save to file.
 * \param filename	the file to save the data to.
 */

void process_save_file(struct process_data *data, char *filename)
{
	FILE	*f;
	char	*ptr;

	if (data == NULL || filename == NULL || *filename == '\0')
		return;

	f = fopen(filename, "w");
	if (f == NULL)
		return;

	ptr = data->mem;

	while (ptr != NULL && *ptr != '\0')
		fputc(*ptr++, f);

	fclose(f);
}


/**
 * Run a script process on a data block.
 *
 * \param *data		The data block to be processed.
 * \param *file		The handle of the file containing the script to be used.
 * \param script	The number of the script within the file.
 * \param verbosity	The level of logging verbosity, larger == more.
 * \param *logger	A callback to handle logging output.
 */

void process_run_script(struct process_data *data, struct process_file *file, int script, int verbosity, void (logger)(char *))
{
	struct process_action	*actions = NULL;
	int			items, i;

	if (data == NULL || file == NULL || script < 0 || script >= file->count)
		return;

	items = load_script(file->filename, file->scripts[script].name, &actions, logger);

	for (i=0; i<items; i++) {
		switch (actions[i].type) {
		case ACTION_SUBSTITUTE:
		case ACTION_SUBSTITUTE_CASE_SENSITIVE:
			run_substitute(data, actions[i].from, actions[i].to, (actions[i].type == ACTION_SUBSTITUTE_CASE_SENSITIVE) ? TRUE : FALSE, verbosity, logger);
			break;

		case ACTION_REDUCE:
			run_reduce(data, actions[i].from, actions[i].to, actions[i].minimum, actions[i].maximum, verbosity, logger);
			break;
		}
	}
}


/**
 * Load a script file, registering the scripts and returning its handle.
 *
 * \param *filename		The filename of the script file to be loaded.
 * \return			The handle of the script file or NULL on failure.
 */

struct process_file *process_load_script_file(char *filename)
{
	FILE				*f;
	struct process_file		*file = NULL;
	enum config_read_status		result;
	int				i;
	char				token[64], value[1024], section[128];

	if (filename == NULL || *filename == '\0')
		return NULL;

	/* Allocate storage for the script file block. */

	file = malloc(sizeof(struct process_file));

	if (file == NULL)
		return NULL;

	file->count = 0;
	file->scripts = NULL;
	file->filename = NULL;

	/* Open the file and find the number of scripts that it contains. */

	f = fopen(filename, "r");
	if (f == NULL) {
		free(file);
		return NULL;
	}

	while ((result = config_read_token_pair(f, token, value, section)) != sf_CONFIG_READ_EOF)
		if (result == sf_CONFIG_READ_NEW_SECTION)
			file->count++;

	rewind(f);

	/* Allocate storage for the script data. */

	file->scripts = malloc(file->count * sizeof(struct process_script));

	if (file->scripts == NULL) {
		free(file);
		return NULL;
	}

	file->filename = strdup(filename);

	i = 0;

	while ((result = config_read_token_pair(f, token, value, section)) != sf_CONFIG_READ_EOF) {
		if ((result == sf_CONFIG_READ_NEW_SECTION) && (i < file->count)) {
			file->scripts[i].name = strdup(section);
			i++;
		}
	}

	fclose(f);

	return file;
}


/**
 * Destroy the data associated with a script file handle and free up any
 * memory that it is using.
 *
 * \param *file			The handle of the script file to be freed.
 */

void process_destroy_script_file(struct process_file *file)
{
	int	i;

	if (file == NULL)
		return;

	if (file->scripts != NULL) {
		for (i = 0; i < file->count; i++)
			if (file->scripts[i].name != NULL)
				free(file->scripts[i].name);

		free(file->scripts);
	}

	if (file->filename != NULL)
		free(file->filename);

	free(file);
}


/**
 * Return the number of scripts within a script file.
 *
 * \param *file			The handle of the script file.
 * \return			The number of scripts in the file.
 */

int process_script_file_entries(struct process_file *file)
{
	return (file == NULL) ? 0 : file->count;
}


/**
 * Return the name of a script from within a script file.
 *
 * \param *file			The handle of the script file.
 * \param script		The script to return the name for.
 * \return			A pointer to the name, or NULL.
 */

char *process_script_file_name(struct process_file *file, int script)
{
	if (file == NULL || script >= file->count)
		return NULL;

	return file->scripts[script].name;
}


/* --------------------------------------------------------------------------------------------------------------------
 */

static int run_reduce(struct process_data *data, char *str, char *to, int minimum, int maximum, int verbosity, void (logger)(char *))
{
	int	len, match_len, found, step;
	char	*mem, *start, b1[15], b2[15], log[PROCESS_LOG_LINE_LEN];

	found = 0;

	if (data != NULL && data->mem != NULL) {
		mem = data->mem;
		match_len = 0;
		len = strlen(str);

		while (*mem != '\0') {
			if (test_memory(mem, str, 1) == len) {
				if (match_len == 0)
					start = mem;
				match_len++;
				mem += (len - 1);
			} else {
				if (match_len >= minimum && (maximum == 0 || match_len <= maximum)) {
					step = match_len + substitute_text(data, start, match_len * len, to);
					mem += (step - 1);

					found++;
				}
				match_len = 0;
			}

			mem++;
		}
	}

	if (logger != NULL && (found > 0 || verbosity > 0)) {
		snprintf(log, PROCESS_LOG_LINE_LEN, "Reducing multiple '%s' to '%s': reduced %d\n",
				write_ctrl_chars(b1, str, 15), write_ctrl_chars(b2, to, 15), found);
		logger(log);
	}

	return (found);
}

/* --------------------------------------------------------------------------------------------------------------------
 */

static int run_substitute(struct process_data *data, char *from, char *to, osbool cs, int verbosity, void (logger)(char *))
{
	int	match_len, found, step;
	char	*mem, b1[15], b2[15], log[PROCESS_LOG_LINE_LEN];

	found = 0;

	if (data != NULL && data->mem != NULL && from != NULL) {
		mem = data->mem;

		while (*mem != '\0') {
			match_len = test_memory(mem, from, cs);

			if (match_len == strlen(from)) {
				step = match_len + substitute_text(data, mem, match_len, to);
				mem += (step - 1);

				found++;
			}

			mem++;
		}
	}

	if (logger != NULL && (found > 0 || verbosity > 0)) {
		snprintf(log, PROCESS_LOG_LINE_LEN, "Substituting '%s' with '%s': replaced %d\n",
				write_ctrl_chars(b1, from, 15), write_ctrl_chars(b2, to, 15), found);
		logger(log);
	}

	return (found);
}

/* --------------------------------------------------------------------------------------------------------------------
 * Test the block of memory against the string, and return the number of matched characters.
 *
 * Parameters in:
 *   mem:     the memory block to test
 *   str:     the string to match
 *   case:    non-zero to match case sensitively
 *
 * Returns:
 *   matched: the number of characters matched
 */

static int test_memory(char *mem, char *str, osbool cs)
{
	int	matched;

	matched = 0;

	if (mem != NULL && str != NULL)
		while (*mem != '\0' && *str != '\0' && ((cs) ? *mem++ : toupper(*mem++)) == ((cs) ? *str++ : toupper(*str++)))
			matched++;

	return(matched);
}

/* --------------------------------------------------------------------------------------------------------------------
 * Replace one piece of text with another.
 *
 * Parameters in:
 *   data:       pointer to the memory data structure to operate on.
 *   position:   pointer to the text to be replaced (must be in the block defined by data).
 *   length:     the number of characters to be replaced.
 *   to:         the text to replace the substituted block with.
 *
 * Returns:
 *   difference: the change in length of the target text.
 */

static int substitute_text(struct process_data *data, char *position, int length, char *to)
{
	int	difference = 0;

	if (data != NULL && data->mem != NULL && position != NULL && to != NULL &&
			position >= data->mem && position < (data->mem + data->len)) {
		difference = strlen(to) - length;

		if (difference > 0 && (data->len + difference) > data->size)
			change_memory_allocation(data, FILE_ALLOC_BLOCK);

		if (difference <= 0 || (data->len + difference) <= data->size) {
			if (difference != 0) {
				memmove(position + strlen(to), position + length, data->len - (position - data->mem));
				data->len += difference;
			}

			memcpy(position, to, strlen(to));
		}
	}

	return(difference);
}


/* --------------------------------------------------------------------------------------------------------------------
 * Load a script from a script file, returning the number of actions found.
 *
 * Parameters in:
 *   file:    the script file name
 *   script:  the script name
 *   actions: a pointer to a pointer to the script actions list; updated to point to the completed array.
 *
 * Returns:
 *   items:   the number of actions added to the list.
 */

static int load_script(char *file, char *script, struct process_action **actions, void (logger)(char *))
{
	FILE				*f;
	enum config_read_status		result;
	bool				found = FALSE;
	int				items = 0, size = 0;
	char				token[64], value[1024], section[128], *bkpt, log[PROCESS_LOG_LINE_LEN];

	size = ACTIONS_ALLOC_BLOCK;
	*actions = malloc(size * sizeof(struct process_action));

	if (file == NULL || *file == '\0')
		return items;

	f = fopen(file, "r");
	if (f == NULL)
		return items;

	while ((result = config_read_token_pair(f, token, value, section)) != sf_CONFIG_READ_EOF) {
		switch (result) {
		case sf_CONFIG_READ_NEW_SECTION:
			found = (string_nocase_strcmp(section, script) == 0);

		case sf_CONFIG_READ_VALUE_RETURNED:
			if (found) {
				if (string_nocase_strcmp(token, "sub") == 0 || string_nocase_strcmp(token, "cssub") == 0) {
					if ((bkpt = strchr(value, ':')) != NULL) {
						*bkpt = '\0';
						bkpt++;

						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE_CASE_SENSITIVE, value, bkpt, 0, 0);
					} else {
						snprintf(log, PROCESS_LOG_LINE_LEN, "Invalid substitution values '%s'.\n", value);
						if (logger != NULL)
							logger(log);
					}
				} else if (string_nocase_strcmp(token, "cisub") == 0) {
					if ((bkpt = strchr(value, ':')) != NULL) {
						*bkpt = '\0';
						bkpt++;

						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE, value, bkpt, 0, 0);
					} else {
						snprintf(log, PROCESS_LOG_LINE_LEN, "Invalid substitution values '%s'.\n", value);
						if (logger != NULL)
							logger(log);
					}
				} else if (string_nocase_strcmp(token, "remove") == 0) {
					if (string_nocase_strcmp(value, "smartquotes") == 0) {
						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE_CASE_SENSITIVE, "[144]", "'", 0, 0);
						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE_CASE_SENSITIVE, "[145]", "'", 0, 0);
						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE_CASE_SENSITIVE, "[148]", "\"", 0, 0);
						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE_CASE_SENSITIVE, "[149]", "\"", 0, 0);
					} else if (string_nocase_strcmp(value, "crlf") == 0) {
						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE_CASE_SENSITIVE, "[13][10]", "[10]", 0, 0);
					} else if (string_nocase_strcmp(value, "manyspaces") == 0) {
						add_new_action(actions, &items, &size, ACTION_REDUCE, " ", " ", 2, 0);
					} else if (string_nocase_strcmp(value, "manynewlines") == 0) {
						add_new_action(actions, &items, &size, ACTION_REDUCE, "[10]", "[10][10]", 2, 0);
					} else if (string_nocase_strcmp(value, "brokenlines") == 0) {
						add_new_action(actions, &items, &size, ACTION_REDUCE, "[10]", " ", 1, 1);
					} else if (string_nocase_strcmp(value, "manytabs") == 0) {
						add_new_action(actions, &items, &size, ACTION_REDUCE, "[9]", "[9]", 2, 0);
					} else {
						snprintf(log, PROCESS_LOG_LINE_LEN, "Invalid remove command '%s'.\n", value);
						if (logger != NULL)
							logger(log);
					}
				} else if (string_nocase_strcmp(token, "command") == 0) {
					add_new_action(actions, &items, &size, ACTION_COMMAND, value, "", 0, 0);
				}
			}
			break;
		}
	}

	fclose(f);

	return (items);
}

/* --------------------------------------------------------------------------------------------------------------------
 * Add a new script action to the actions list, allocating memory if required.
 */

void add_new_action(struct process_action **actions, int *items, int *size, int type, char *from, char *to, int min, int max)
{
	if (*actions != NULL && *items >= *size) {
		*size += ACTIONS_ALLOC_BLOCK;
		*actions = realloc(*actions, *size * sizeof(struct process_action));
	}

	if (*actions == NULL)
		return;

	(*actions)[*items].type = type;

	switch (type) {
	case ACTION_COMMAND:
		break;

	case ACTION_SUBSTITUTE:
	case ACTION_SUBSTITUTE_CASE_SENSITIVE:
		(*actions)[*items].from = malloc(strlen(from) + 1);
		str_char_cpy((*actions)[*items].from, from);

		(*actions)[*items].to = malloc(strlen(to) + 1);
		str_char_cpy((*actions)[*items].to, to);

		(*actions)[*items].minimum = 0;
		(*actions)[*items].maximum = 0;
		break;

	case ACTION_REDUCE:
		(*actions)[*items].from = malloc(strlen(from) + 1);
		str_char_cpy((*actions)[*items].from, from);

		(*actions)[*items].to = malloc(strlen(to) + 1);
		str_char_cpy((*actions)[*items].to, to);

		(*actions)[*items].minimum = min;
		(*actions)[*items].maximum = max;
		break;
	}

	(*items)++;
}

/* --------------------------------------------------------------------------------------------------------------------
 * Copy a string in memory, substituting [...] for a single byte of the correct value.
 */

char *str_char_cpy(char *str1, char *str2)
{
 	char	*base, num[128], *numptr;

 	base = str1;

	while (*str2 != '\0') {
		if (*str2 == '[') {
			str2++;
			numptr = num;

			while (*str2 != ']' && isdigit(*str2))
				*numptr++ = *str2++;

			if (*str2 == ']')
				str2++;

			*numptr = '\0';
			*str1++ = atoi(num);
		} else {
			*str1++ = *str2++;
		}
	}

	*str1 = '\0';

	return base;
}

/* --------------------------------------------------------------------------------------------------------------------
 * Change the memory allocation of a data block.
 */

static int change_memory_allocation(struct process_data *data, int expand)
{
	char	*new;
	int	result = 0;

	if (data->size + expand >= 0) {
		new = realloc(data->mem, data->size + expand);

		 if (new != NULL) {
			data->mem = new;
			data->size += expand;
			result = 1;
		}
	}

	return result;
}

/* --------------------------------------------------------------------------------------------------------------------
 */

static char *write_ctrl_chars(char *buf, char *str, int len)
{
	char		*write, ctrl[6];
	unsigned char	c;
	int		ctrllen;

	if (buf == NULL || str == NULL)
		return buf;

	write = buf;
	len--; /* Allow space for the terminator */

	while (len > 0 && *str != '\0') {
		if (*str > 32 && *str < 128) {
			*write++ = *str;
			len--;
		} else {
			c = (unsigned char) *str;
			ctrllen = snprintf(ctrl, 6, "[%d]", c);

			if (ctrllen < len) {
				memcpy(write, ctrl, ctrllen);
				len -= ctrllen;
				write += ctrllen;
			} else {
				len = 0;
			}
		}

		str++;
	}

	if (*str != '\0' && (write - 3) >= buf)
		strcpy(write-3, "...");

	*write = '\0';

	return buf;
}

