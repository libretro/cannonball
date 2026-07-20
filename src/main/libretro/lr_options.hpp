/***************************************************************************
   Interface for notifying Libretro frontend of configuration
   value changes.

   See license.txt for more details.
***************************************************************************/

#pragma once

#include <libretro.h>

//-----------------------------------------------------------------------------
// Function prototypes 
//-----------------------------------------------------------------------------
namespace lr_options
{
   extern void init();
   extern void close();

   /* Use function overloading to simplify usage
    * (i.e. reduce verbosity by avoiding the need
    * to cast pointers) */
   extern void set_frontend_variable(const bool *config_var);
   extern void set_frontend_variable(const int *config_var);
}

