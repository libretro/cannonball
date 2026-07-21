/***************************************************************************
    General C++ Helper Functions

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once

#include <stdint.h>
#include <string>

std::string Utils_to_string(int i);
std::string Utils_to_string(char c);
std::string Utils_to_hex_string(int i);
uint32_t Utils_from_hex_string(std::string s);

