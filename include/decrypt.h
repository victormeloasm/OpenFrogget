#ifndef DECRYPT_H
#define DECRYPT_H

#include <string>

void decrypt_file(const std::string& input_path, const std::string& output_path, const std::string& privkey_path);

#endif