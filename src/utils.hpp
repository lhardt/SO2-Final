#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

/** Get the first word of the command, and where it ends.  */
std::string get_first_word(const std::string &s, int & last_index_ret);

/** Returns a copy of the string, from the first printable character to the last. */
std::string trim_string(const std::string &s);

#endif /* UTILS_HPP */