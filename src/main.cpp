#include "encrypt.h"
#include "decrypt.h"
#include "keygen.h"
#include "eccfrog512ck2.h"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

void print_usage() {
    std::cout << "Usage:\n"
              << "  openfrogget --generate-keys\n"
              << "  openfrogget --decrypt <input_file>.enc <output_file>.txt <Private_key>.pem\n"
              << "  openfrogget --encrypt <input_file> <output_file> <public_key_file>.pem\n"
              << "  openfrogget --verify-keys\n";
}

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
                if (isxdigit(c)) hex += static_cast<char>(tolower(static_cast<unsigned char>(c)));
            }
        }
    }

    return hex;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    try {
        if (command == "--generate-keys") {
            generate_keys();
        } else if (command == "--encrypt" && argc == 3) {
            encrypt_file(argv[2], "encrypted.enc", "public_key.pem");
        } else if (command == "--decrypt" && argc == 3) {
            decrypt_file(argv[2], "decrypted.out", "private_key.pem");
        } else if (command == "--verify-keys") {
            ECCFrog512CK2 curve;

            std::ifstream priv_file("private_key.pem");
            std::ifstream pub_file("public_key.pem");
            if (!priv_file || !pub_file) {
                throw std::runtime_error("Failed to open key files for validation.");
            }

            std::string priv_data((std::istreambuf_iterator<char>(priv_file)), std::istreambuf_iterator<char>());
            std::string pub_data((std::istreambuf_iterator<char>(pub_file)), std::istreambuf_iterator<char>());

            std::string priv_clean = extract_pgp_payload(priv_data);
            mpz_class priv_key(priv_clean, 16);

            std::string pub_clean = extract_pgp_payload(pub_data);
            ECCFrog512CK2::Point expected_pub = curve.scalar_mul(curve.get_G(), priv_key);

            ECCFrog512CK2::Point actual_pub;
            if (pub_clean.size() == 66 && (pub_clean.substr(0, 2) == "02" || pub_clean.substr(0, 2) == "03")) {
                actual_pub = curve.point_from_compressed_hex(pub_clean);
            } else if (pub_clean.size() == 258 && pub_clean.substr(0, 2) == "04") {
                std::vector<unsigned char> bytes;
                for (size_t i = 0; i < pub_clean.length(); i += 2) {
                    bytes.push_back(static_cast<unsigned char>(
                        std::stoul(pub_clean.substr(i, 2), nullptr, 16)));
                }
                actual_pub = curve.point_from_uncompressed(bytes);
            } else {
                throw std::runtime_error("Unrecognized PGP key format: not compressed or uncompressed valid hex");
            }

            if (expected_pub.x == actual_pub.x && expected_pub.y == actual_pub.y) {
                std::cout << "[+] Key pair is valid and matches ECCFrog512CK2.\n";
            } else {
                std::cerr << "[-] Key pair does NOT match.\n";
                return 1;
            }
        } else {
            print_usage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[-] Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
