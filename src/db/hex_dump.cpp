#include "hex_dump.hpp"

#include <iomanip>


std::ostream& render_printable_chars(std::ostream& os, const char* buffer, size_t bufsize) {
    os << " | ";
    for (size_t i = 0; i < bufsize; ++i) {
        if (std::isprint(buffer[i])) {
            os << buffer[i];
        } else {
            os << ".";
        }
    }
    return os;
}

std::ostream& hex_dump(std::ostream& os, const uint8_t* buffer, size_t bufsize, bool showPrintableChars) {
    auto oldFormat = os.flags();
    auto oldFillChar = os.fill();

    os << std::hex;
    os.fill('0');
    bool printBlank = false;
    size_t i = 0;
    for (; i < bufsize; ++i) {
        if (i % 8 == 0) {
            if (i != 0 && showPrintableChars) {
                render_printable_chars(os, reinterpret_cast<const char*>(&buffer[i] - 8), 8);
            }
            os << std::endl;
            printBlank = false;
        }
        if (printBlank) {
            os << ' ';
        }
        os << std::setw(2) << std::right << unsigned(buffer[i]);
        if (!printBlank) {
            printBlank = true;
        }
    }
    if (i % 8 != 0 && showPrintableChars) {
        for (size_t j = 0; j < 8 - (i % 8); ++j) {
            os << "   ";
        }
        render_printable_chars(os, reinterpret_cast<const char*>(&buffer[i] - (i % 8)), (i % 8));
    } else if (i > 0 && showPrintableChars) {
        render_printable_chars(os, reinterpret_cast<const char*>(&buffer[i] - 8), 8);
    }

    os << std::endl;

    os.fill(oldFillChar);
    os.flags(oldFormat);

    return os;
}