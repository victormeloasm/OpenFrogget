#include "encrypt.h"
#include "eccfrog512ck2.h"
#include "keygen.h"
#include <iostream>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <fstream>
#include <vector>
#include <algorithm>
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

void encrypt_file(const std::string& input_path,
                  const std::string& output_path,
                  const std::string& pubkey_path) {
    try {
        std::ifstream pub_file(pubkey_path);
        if (!pub_file) throw std::runtime_error("Failed to open public key file");

        std::string pgp_data((std::istreambuf_iterator<char>(pub_file)),
                             std::istreambuf_iterator<char>());
        std::string clean_hex = extract_pgp_payload(pgp_data);

        ECCFrog512CK2 curve;
        ECCFrog512CK2::Point pub_point;

        // Fixed key length checks for 512-bit ECC
        if (clean_hex.size() == 130 && (clean_hex.substr(0, 2) == "02" || clean_hex.substr(0, 2) == "03")) {
            pub_point = curve.point_from_compressed_hex(clean_hex);
        } else if (clean_hex.size() == 258 && clean_hex.substr(0, 2) == "04") {
            std::vector<unsigned char> bytes;
            for (size_t i = 0; i < clean_hex.length(); i += 2) {
                std::string byte_str = clean_hex.substr(i, 2);
                bytes.push_back(static_cast<unsigned char>(std::stoul(byte_str, nullptr, 16)));
            }
            pub_point = curve.point_from_uncompressed(bytes);
        } else {
            throw std::runtime_error("Unrecognized PGP key format. Expected 130/258 hex chars for compressed/uncompressed keys");
        }

        // Ephemeral key generation
        mpz_class eph_priv = generate_secure_private_key(curve.get_n());
        ECCFrog512CK2::Point eph_pub = curve.scalar_mul(curve.get_G(), eph_priv);
        ECCFrog512CK2::Point shared_point = curve.scalar_mul(pub_point, eph_priv);

        // Derive AES key from shared secret
        std::string shared_secret = shared_point.x.get_str(16);
        if (shared_secret.length() > 64) {
            shared_secret = shared_secret.substr(0, 64); // Truncate if too long
        } else if (shared_secret.length() < 64) {
            shared_secret = std::string(64 - shared_secret.length(), '0') + shared_secret; // Pad with leading zeros
        }

        std::vector<unsigned char> aes_key(32);
        for (size_t i = 0; i < 32; ++i) {
            aes_key[i] = static_cast<unsigned char>(
                std::stoul(shared_secret.substr(i * 2, 2), nullptr, 16));
        }

        // Read plaintext
        std::ifstream infile(input_path, std::ios::binary);
        if (!infile) throw std::runtime_error("Failed to open input file");
        std::vector<unsigned char> plaintext((std::istreambuf_iterator<char>(infile)),
                                            std::istreambuf_iterator<char>());
        infile.close();
        if (plaintext.empty()) throw std::runtime_error("Input file is empty");

        // Generate IV
        std::vector<unsigned char> iv(12);
        if (RAND_bytes(iv.data(), iv.size()) != 1) {
            throw std::runtime_error("Failed to generate IV");
        }

        // AES-GCM Encryption
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) throw std::runtime_error("Failed to create cipher context");

        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1 ||
            EVP_EncryptInit_ex(ctx, nullptr, nullptr, aes_key.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Encryption initialization failed");
        }

        std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
        int len = 0, ct_len = 0;

        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), plaintext.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Encryption failed during update");
        }
        ct_len = len;

        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + ct_len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Final encryption step failed");
        }
        ct_len += len;

        // Retrieve authentication tag
        std::vector<unsigned char> tag(16);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to retrieve GCM tag");
        }
        EVP_CIPHER_CTX_free(ctx);

        // Write output
        std::ofstream outfile(output_path, std::ios::binary);
        if (!outfile) throw std::runtime_error("Failed to create output file");

        std::vector<unsigned char> eph_pub_bytes = eph_pub.to_uncompressed_bytes();
        uint16_t eph_pub_size = static_cast<uint16_t>(eph_pub_bytes.size());
        outfile.write(reinterpret_cast<const char*>(&eph_pub_size), sizeof(eph_pub_size));
        outfile.write(reinterpret_cast<const char*>(eph_pub_bytes.data()), eph_pub_bytes.size());
        outfile.write(reinterpret_cast<const char*>(iv.data()), iv.size());
        outfile.write(reinterpret_cast<const char*>(tag.data()), tag.size());
        outfile.write(reinterpret_cast<const char*>(ciphertext.data()), ct_len);

        std::cout << "[+] File encrypted successfully to: " << output_path << "\n";

    } catch (const std::exception& e) {
        std::cerr << "[-] Encryption error: " << e.what() << "\n";
        throw;
    }
}