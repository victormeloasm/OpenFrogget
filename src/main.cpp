#include <iostream>
#include "eccfrog512ck2.hpp"

int main() {
    ECCFrog512CK2 ecc;

    std::cout << "Checking if twist is secure...\n";
    std::cout << (ecc.is_twist_secure() ? "Twist secure.\n" : "Twist NOT secure.\n");

    auto [priv, pub] = ecc.generate_keypair();

    std::cout << "\nPrivate Key:\n" << priv.get_str() << "\n";
    std::cout << "\nPublic Key (compressed):\n" << ecc.compress(pub) << "\n";

    std::cout << "\nBenchmarking scalar multiplication methods...\n";
    ecc.benchmark();

    return 0;
}
std::cout << "\nTesting GLV scalar multiplication...\n";
auto glv = ecc.scalar_mult_GLV(123456789, ecc.Gx, ecc.Gy);
std::cout << "GLV X: " << glv.first.get_str().substr(0, 64) << "..." << std::endl;
