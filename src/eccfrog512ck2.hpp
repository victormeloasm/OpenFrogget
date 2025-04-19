#ifndef ECCFROG512CK2_HPP
#define ECCFROG512CK2_HPP

#include <gmpxx.h>
#include <sodium.h>
#include <string>
#include <utility>
#include <vector>

class ECCFrog512CK2 {
public:
    const mpz_class p, a, b, n, Gx, Gy;

    ECCFrog512CK2();
    std::pair<mpz_class, std::pair<mpz_class, mpz_class>> generate_keypair();
    std::pair<mpz_class, mpz_class> scalar_mult_montgomery(mpz_class k, mpz_class x, mpz_class y);
    std::pair<mpz_class, mpz_class> scalar_mult_NAF(mpz_class k, mpz_class x, mpz_class y);
    std::vector<int> naf(mpz_class k);
    std::string compress(std::pair<mpz_class, mpz_class> point);
    bool is_on_curve(mpz_class x, mpz_class y);
    bool is_twist_secure();
    void benchmark();
    std::pair<mpz_class, mpz_class> endomorphism(mpz_class x, mpz_class y);    std::pair<mpz_class, mpz_class> scalar_mult_GLV(mpz_class k, mpz_class x, mpz_class y);};

#endif