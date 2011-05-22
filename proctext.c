/** Process Text
 **
 ** Process text files, performing multiple search and replace.
 **
 ** (c) Stephen Fryatt, 2009
 **
 ** Version 0.01 (27 Dec 2009)
 **/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ACTIONS_ALLOC_BLOCK 20
#define FILE_ALLOC_BLOCK    1024

#define ACTION_NONE         0
#define ACTION_SUBSTITUTE   1
#define ACTION_REDUCE       2
#define ACTION_COMMAND      3

#define READ_EOF            0
#define READ_NEW_SECTION    1
#define READ_VALUE_RETURNED 2

#define TRUE  1
#define FALSE 0



/* Data Structure containing details of a conversion action.
 */

typedef struct {
	int	type;

	char	*from;
	char	*to;

	int	minimum;
	int	maximum;
} action;

typedef struct {
	char	*mem;

	int	size;
	int	len;
} memory;


static int run_reduce(memory *data, char *str, char *to, int minimum, int maximum, int verbosity);
static int run_substitute(memory *data, char *str, char *to, int cs, int verbosity);
static int test_memory(char *mem, char *str, int cs);
static int substitute_text(memory *data, char *position, int length, char *to);

static void load_file(memory *data, char *filename);
static void save_file(memory *data, char *filename);

static int load_script(char *file, char *script, action **actions);
int add_new_action(action **actions, int *items, int *size, int type, char *from, char *to, int min, int max);
char *str_char_cpy(char *str1, char *str2);

static int change_memory_allocation(memory *data, int expand);

int read_token_pair (FILE *file, char *token, char *value, char *section);
char *strip_surrounding_whitespace (char *string);
int wildcard_strcmp (char *s1, char *s2, int any_case);
int strcmp_no_case (char *s1, char *s2);

static char *write_ctrl_chars(char *buf, char *str, int len);


/* --------------------------------------------------------------------------------------------------------------------
 *
 */

void main(int argc, char *argv[])
{
	memory		data;
	action		*actions = NULL;
	int		items, i, verbosity;

	verbosity = 0;

	data.mem = NULL;
	data.size = 0;
	data.len = 0;

	items = load_script(argv[1], argv[2], &actions);

	load_file(&data, argv[3]);

	for (i=0; i<items; i++) {
		switch (actions[i].type) {
		case ACTION_SUBSTITUTE:
			run_substitute(&data, actions[i].from, actions[i].to, 1, verbosity);
			break;

		case ACTION_REDUCE:
			run_reduce(&data, actions[i].from, actions[i].to, actions[i].minimum, actions[i].maximum, verbosity);
			break;
		}
	}

	save_file(&data, argv[4]);
}

/* --------------------------------------------------------------------------------------------------------------------
 */

static int run_reduce(memory *data, char *str, char *to, int minimum, int maximum, int verbosity)
{
	int	len, match_len, found, step;
	char	*mem, *start, b1[15], b2[15];

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

	if (found > 0 || verbosity > 0)
		printf("Reducing multiple '%s' to '%s': reduced %d\n",
				write_ctrl_chars(b1, str, 15), write_ctrl_chars(b2, to, 15), found);

	return (found);
}

/* --------------------------------------------------------------------------------------------------------------------
 */

static int run_substitute(memory *data, char *from, char *to, int cs, int verbosity)
{
	int	match_len, found, step;
	char	*mem, b1[15], b2[15];

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

	if (found > 0 || verbosity > 0)
		printf("Substituting '%s' with '%s': replaced %d\n",
				write_ctrl_chars(b1, from, 15), write_ctrl_chars(b2, to, 15), found);

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

static int test_memory(char *mem, char *str, int cs)
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

static int substitute_text(memory *data, char *position, int length, char *to)
{
	int	difference;

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
 * Load the file to be processed into memory, allocating space with malloc() and returning the pointer.
 *
 * Parameters in:
 *   data:     pointer to the memory data structure to load the file into.
 *   filename: the file name
 */

static void load_file(memory *data, char *filename)
{
	FILE	*f;
	char	*ptr = NULL, c;

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

/* --------------------------------------------------------------------------------------------------------------------
 * Save a data block out to a file.
 *
 * Parameters in:
 *   data:     pointer to the memory data structure to save to file.
 *   filename: the file to save the data to.
 */

static void save_file(memory *data, char *filename)
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

static int load_script(char *file, char *script, action **actions)
{
	FILE	*f;
	int	items = 0, size = 0, result, found = 0;
	char	token[64], value[1024], section[128], *bkpt;

	size = ACTIONS_ALLOC_BLOCK;
	*actions = malloc(size * sizeof(action));

	if (file == NULL || *file == '\0')
		return items;

	f = fopen(file, "r");
	if (f == NULL)
		return items;

	while ((result = read_token_pair(f, token, value, section)) != READ_EOF) {
		switch (result) {
		case READ_NEW_SECTION:
			found = (strcmp_no_case(section, script) == 0);

		case READ_VALUE_RETURNED:
			if (found) {
				if (strcmp_no_case(token, "sub") == 0) {
					if ((bkpt = strchr(value, ':')) != NULL) {
						*bkpt = '\0';
						bkpt++;

						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE, value, bkpt, 0, 0);
					} else {
						printf("Invalid substitution values '%s'.\n", value);
					}
				} else if (strcmp_no_case(token, "remove") == 0) {
					if (strcmp_no_case(value, "smartquotes") == 0) {
						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE, "[144]", "'", 0, 0);
						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE, "[145]", "'", 0, 0);
						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE, "[148]", "\"", 0, 0);
						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE, "[149]", "\"", 0, 0);
					} else if (strcmp_no_case(value, "crlf") == 0) {
						add_new_action(actions, &items, &size, ACTION_SUBSTITUTE, "[13][10]", "[10]", 0, 0);
					} else if (strcmp_no_case(value, "manyspaces") == 0) {
						add_new_action(actions, &items, &size, ACTION_REDUCE, " ", " ", 2, 0);
					} else if (strcmp_no_case(value, "manynewlines") == 0) {
						add_new_action(actions, &items, &size, ACTION_REDUCE, "[10]", "[10][10]", 2, 0);
					} else if (strcmp_no_case(value, "brokenlines") == 0) {
						add_new_action(actions, &items, &size, ACTION_REDUCE, "[10]", " ", 1, 1);
					} else if (strcmp_no_case(value, "manytabs") == 0) {
						add_new_action(actions, &items, &size, ACTION_REDUCE, "[9]", "[9]", 2, 0);
					} else {
						printf("Invalid remove command '%s'.\n", value);
					}
				} else if (strcmp_no_case(token, "command") == 0) {
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

int add_new_action(action **actions, int *items, int *size, int type, char *from, char *to, int min, int max)
{
	if (*actions != NULL && *items >= *size) {
		*size += ACTIONS_ALLOC_BLOCK;
		*actions = realloc(*actions, *size * sizeof(action));
	}

	if (*actions == NULL)
		return;

	(*actions)[*items].type = type;

	switch (type) {
	case ACTION_COMMAND:
		break;

	case ACTION_SUBSTITUTE:
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

static int change_memory_allocation(memory *data, int expand)
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











int read_token_pair(FILE *file, char *token, char *value, char *section)
{
	char	line[1024], *stripped_line, *a, *b;
	int	result = READ_EOF, read = 0;


	while (!read && (fgets (line, sizeof(line), file) != NULL)) {
		if (*line != '#') {
			stripped_line = strip_surrounding_whitespace(line);

			if (wildcard_strcmp("[*]", stripped_line, 1)) {
				*strrchr(stripped_line, ']') = '\0';
				if (section != NULL)
					strcpy(section, stripped_line + 1);

				result = READ_NEW_SECTION;
			} else {
				a = NULL;
				b = strchr(stripped_line, ':');

				if (b != NULL) {
					a = stripped_line;

					*b = '\0';
					b += 1;

					if (token != NULL)
					strcpy(token, a);
				}

				if (value != NULL) {
					/* Remove external whitespace and enclosing quotes if presnt. */

					b = strip_surrounding_whitespace (b);

					if (*b == '"' && *(strchr(b, '\0')-1) == '"') {
						b++;
						*(strchr(b, '\0')-1) = '\0';
					}

					strcpy(value, b);
				}

				if (result != READ_NEW_SECTION) {
					result = READ_VALUE_RETURNED;

					read = 1;
				} else {
					if (token != NULL)
						*token = '\0';

					if (value != NULL)
						*value = '\0';
				}
			}
		}
	}

	return (result);
}





char *strip_surrounding_whitespace(char *string)
{
	char	*start, *end;

	if (*string == '\0')
		return (string);

	start = string;
	while (isspace (*start))
		start++;

	end = strrchr (string, '\0') - 1;
	while (isspace (*end))
		*end-- = '\0';

	return (start);
}



int wildcard_strcmp(char *s1, char *s2, int any_case)
{
	char	c1 = *s1, c2 = *s2;

	if (any_case) {
		c1 = tolower (c1);
		c2 = tolower (c2);
	}

	if (c2 == 0) {
		while (*s1 == '*')
		s1++;
		return (*s1 == 0);
	}

	if (c1 == c2 || c1 == '#')
		return (wildcard_strcmp (&s1[1], &s2[1], any_case));

	if (c1 == '*') {
		int ok = FALSE;

		if (s1[1] == 0)
			return (TRUE);

		while (!ok && *s2 != 0) {
		ok = ok || wildcard_strcmp (&s1[1], s2, any_case);
		s2++;
		}

		return (ok);
	}

	return (FALSE);
}



int strcmp_no_case(char *s1, char *s2)
{
	while (*s1 != '\0' && *s2 != '\0' && (toupper(*s1) - toupper(*s2)) == 0) {
		s1++;
		s2++;
	}

	return (toupper(*s1) - toupper(*s2));
}
