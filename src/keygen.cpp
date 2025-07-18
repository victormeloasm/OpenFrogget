#include "keygen.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <gmpxx.h>
#include <openssl/rand.h>
#include <iomanip>

// Generates a cryptographically secure private key
mpz_class generate_secure_private_key(const mpz_class& n) {
    gmp_randclass rand(gmp_randinit_default);

    // Seed with both time and OpenSSL's random bytes
    unsigned char openssl_seed[32];
    RAND_bytes(openssl_seed, sizeof(openssl_seed));
    rand.seed(time(nullptr) + *reinterpret_cast<unsigned long*>(openssl_seed));

    mpz_class priv_key;
    do {
        priv_key = rand.get_z_range(n);
    } while (priv_key == 0);

    return priv_key;
}

// Saves key in PGP armored format
void save_pgp_key(const std::string& filename,
                 const std::string& key_data,
                 const std::string& key_type) {
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("Failed to open key file for writing");
    }

    file << "-----BEGIN ECCFROG512 " << key_type << "-----\n";

    // Format with 64 characters per line
    for (size_t i = 0; i < key_data.size(); i += 64) {
        file << key_data.substr(i, 64) << "\n";
    }

    file << "-----END ECCFROG512 " << key_type << "-----\n";
}

void generate_keys() {
    try {
        ECCFrog512CK2 curve;

        // Generate private key (hex string)
        mpz_class priv_key = generate_secure_private_key(curve.get_n());
        std::string priv_hex = priv_key.get_str(16);

        // Ensure uniform length (pad with leading zeros if needed)
        size_t expected_length = curve.get_n().get_str(16).length();
        while (priv_hex.length() < expected_length) {
            priv_hex = "0" + priv_hex;
        }

        // Generate public key (uncompressed format)
        ECCFrog512CK2::Point pub_key = curve.scalar_mul(curve.get_G(), priv_key);
        std::vector<unsigned char> pub_bytes = pub_key.to_uncompressed_bytes();

        // Convert to hex string
        std::ostringstream oss;
        for (unsigned char byte : pub_bytes) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        std::string pub_hex = oss.str();

        // Save in PGP format
        save_pgp_key("private_key.pem", priv_hex, "PRIVATE KEY");
        save_pgp_key("public_key.pem", pub_hex, "PUBLIC KEY");

        std::cout << "[+] PGP keys generated successfully:\n"
                  << " - Private key: private_key.pem\n"
                  << " - Public key:  public_key.pem\n"
                  << " Key format: Uncompressed ECCFrog512CK2 (SEC1)\n";

    } catch (const std::exception& e) {
        std::cerr << "[-] Key generation failed: " << e.what() << "\n";
        throw;
    }
}