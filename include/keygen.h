#ifndef KEYGEN_H
#define KEYGEN_H

#include <gmpxx.h>
#include "eccfrog512ck2.h"

mpz_class generate_secure_private_key(const mpz_class& n);
void generate_keys();
void save_key(const std::string& filename, const mpz_class& key);
void save_key(const std::string& filename, const ECCFrog512CK2::Point& key);

#endif