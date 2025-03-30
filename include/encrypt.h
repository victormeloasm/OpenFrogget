#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <string>

// Corrected parameter order: input_path, output_path, pubkey_path
void encrypt_file(const std::string& input_path, 
                  const std::string& output_path, 
                  const std::string& pubkey_path);

#endif