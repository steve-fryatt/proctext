/* ProcText - main.h
 *
 * (c) Stephen Fryatt, 2009-2011
 */

#ifndef PROCTEXT_MAIN
#define PROCTEXT_MAIN


/**
 * Application-wide global variables.
 */

extern wimp_t			main_task_handle;
extern int			main_quit_flag;
extern osspriteop_area		*main_wimp_sprites;

/**
 * Main code entry point.
 */

int main(int argc, char *argv[]);

#endif

