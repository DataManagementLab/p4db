#pragma once

#include <iostream>

std::ostream& render_printable_chars(std::ostream& os, const char* buffer,
                                     size_t bufsize);

std::ostream& hex_dump(std::ostream& os, const uint8_t* buffer, size_t bufsize,
                       bool showPrintableChars = true);