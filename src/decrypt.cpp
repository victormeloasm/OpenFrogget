#include "decrypt.h"
#include "eccfrog512ck2.h"
#include "keygen.h"
#include <iostream>
#include <openssl/evp.h>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <sstream>

static std::string extract_pgp_payload(const std::string& data) {
    std::string hex;
    std::istringstream iss(data);
    std::string line;
    bool inside = false;

    while (std::getline(iss, line)) {
        if (line.find("-----BEGIN") != std::string::npos) {
            inside = true;
            continue;
        }
        if (line.find("-----END") != std::string::npos) {
            break;
        }
        if (inside) {
            for (char c : line) {
                if (isxdigit(c)) {
                    hex += static_cast<char>(tolower(static_cast<unsigned char>(c)));
                }
            }
        }
    }

    if (hex.empty()) {
        throw std::runtime_error("No PGP payload found");
    }
    return hex;
}

void decrypt_file(const std::string& input_path,
                  const std::string& output_path,
                  const std::string& privkey_path) {
    try {
        std::ifstream priv_file(privkey_path);
        if (!priv_file) throw std::runtime_error("Failed to open private key file");

        std::string pgp_data((std::istreambuf_iterator<char>(priv_file)),
                             std::istreambuf_iterator<char>());
        std::string clean_hex = extract_pgp_payload(pgp_data);
        mpz_class priv_key(clean_hex, 16);

        std::ifstream infile(input_path, std::ios::binary);
        if (!infile) throw std::runtime_error("Failed to open input file");

        uint16_t eph_pub_size = 0;
        infile.read(reinterpret_cast<char*>(&eph_pub_size), sizeof(eph_pub_size));
        if (static_cast<size_t>(infile.gcount()) != sizeof(eph_pub_size)) {
            throw std::runtime_error("Failed to read ephemeral public key size");
        }

        std::vector<unsigned char> eph_pub_bytes(eph_pub_size), iv(12), tag(16);
        infile.read(reinterpret_cast<char*>(eph_pub_bytes.data()), eph_pub_bytes.size());
        if (static_cast<size_t>(infile.gcount()) != eph_pub_bytes.size()) {
            throw std::runtime_error("Failed to read ephemeral public key bytes");
        }

        infile.read(reinterpret_cast<char*>(iv.data()), iv.size());
        if (static_cast<size_t>(infile.gcount()) != iv.size()) {
            throw std::runtime_error("Failed to read IV");
        }

        infile.read(reinterpret_cast<char*>(tag.data()), tag.size());
        if (static_cast<size_t>(infile.gcount()) != tag.size()) {
            throw std::runtime_error("Failed to read authentication tag");
        }

        std::vector<unsigned char> ciphertext((std::istreambuf_iterator<char>(infile)), {});
        infile.close();

        ECCFrog512CK2 curve;
        ECCFrog512CK2::Point eph_pub = curve.point_from_uncompressed(eph_pub_bytes);
        ECCFrog512CK2::Point shared_point = curve.scalar_mul(eph_pub, priv_key);

        std::string shared_secret = shared_point.x.get_str(16);
        // Ensure exactly 64 characters by padding with leading zeros or truncating
        shared_secret = std::string(64 - std::min<size_t>(64, shared_secret.length()), '0') + shared_secret;
        shared_secret = shared_secret.substr(0, 64); // Truncate if longer than 64 chars

        std::vector<unsigned char> aes_key(32);
        for (size_t i = 0; i < 32; ++i) {
            aes_key[i] = static_cast<unsigned char>(
                std::stoul(shared_secret.substr(i * 2, 2), nullptr, 16));
        }

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) throw std::runtime_error("Failed to create cipher context");

        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1 ||
            EVP_DecryptInit_ex(ctx, nullptr, nullptr, aes_key.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Decryption initialization failed");
        }

        std::vector<unsigned char> plaintext(ciphertext.size() + 16); // Buffer with extra space
        int len = 0, pt_len = 0;

        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Decryption failed during update");
        }
        pt_len = len;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to set GCM authentication tag");
        }

        int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + pt_len, &len);
        pt_len += len;
        EVP_CIPHER_CTX_free(ctx);

        if (ret <= 0) {
            throw std::runtime_error("Decryption failed: integrity check failed");
        }

        std::ofstream outfile(output_path, std::ios::binary);
        if (!outfile) throw std::runtime_error("Failed to create output file");

        outfile.write(reinterpret_cast<const char*>(plaintext.data()), pt_len);
        std::cout << "[+] File decrypted successfully to: " << output_path << "\n";

    } catch (const std::exception& e) {
        std::cerr << "[-] Decryption error: " << e.what() << "\n";
        throw;
    }
}