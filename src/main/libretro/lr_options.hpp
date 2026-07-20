/***************************************************************************
   Interface for notifying Libretro frontend of configuration
   value changes.

   See license.txt for more details.
***************************************************************************/

#pragma once

#include <libretro.h>

/*----------------------------------------------------------------------------- */
/* Function prototypes  */
/*----------------------------------------------------------------------------- */
extern void lr_options_init();
extern void lr_options_close();

/* Use function overloading to simplify usage
 * (i.e. reduce verbosity by avoiding the need
 * to cast pointers) */
extern void lr_options_set_frontend_variable(const bool *config_var);
extern void lr_options_set_frontend_variable(const int *config_var);


