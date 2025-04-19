#include "eccfrog512ck2.hpp"
#include <chrono>
#include <iostream>
#include <blake3.h>

ECCFrog512CK2::ECCFrog512CK2()
    : p("13407807929942597099574024998205846127479365820592393377723561443721764030073546976801874298166903427690031858186486050853753882811946569946433649006084171"),
      a("-7"),
      Gx("8738379852674060893335160669390950305573653783231030656544726305066230810328881024010886931317627508773245362380938677755754448451420325817972750886722371"),
      Gy("10104906652111556042420230041540564410479008895503871920234628893113479746324902980450282449515434382420807421477792784494545955741858630434485376637406642"),
      n("13407807929942597099574024998205846127479365820592393377723561443720917119732220222937674194194655218914977121741651673281619388251992476346242122033284439"),
      b([this]() {
          const std::string seed = "ECCFrog512CK2 forever";
          uint8_t hash[32];
          blake3_hasher hasher;
          blake3_hasher_init(&hasher);
          blake3_hasher_update(&hasher, seed.data(), seed.size());
          blake3_hasher_finalize(&hasher, hash, 32);
          mpz_class result;
          mpz_import(result.get_mpz_t(), 32, 1, 1, 1, 0, hash);
          result %= p;
          return result;
      }())
{
    sodium_init();
}

std::pair<mpz_class, std::pair<mpz_class, mpz_class>> ECCFrog512CK2::generate_keypair() {
    unsigned char seed[64];
    randombytes_buf(seed, sizeof(seed));
    mpz_class priv;
    mpz_import(priv.get_mpz_t(), sizeof(seed), 1, 1, 1, 0, seed);
    priv %= n;
    auto pub = scalar_mult_montgomery(priv, Gx, Gy);
    return {priv, pub};
}

std::vector<int> ECCFrog512CK2::naf(mpz_class k) {
    std::vector<int> naf_repr;
    mpz_class zero = 0;
    while (k > 0) {
        if (k % 2 != 0) {
            int z = 2 - (k % 4).get_si();
            naf_repr.push_back(z);
            k -= z;
        } else {
            naf_repr.push_back(0);
        }
        k /= 2;
    }
    return naf_repr;
}

std::pair<mpz_class, mpz_class> ECCFrog512CK2::scalar_mult_NAF(mpz_class k, mpz_class x, mpz_class y) {
    std::vector<int> naf_k = naf(k);
    mpz_class rx = 0, ry = 0;
    bool started = false;

    for (ssize_t i = naf_k.size() - 1; i >= 0; --i) {
        if (started) {
            mpz_class lambda, den;
            mpz_invert(den.get_mpz_t(), (2 * ry).get_mpz_t(), p.get_mpz_t());
            lambda = ((3 * rx * rx + a) * den) % p;
            mpz_class x2 = (lambda * lambda - 2 * rx) % p;
            mpz_class y2 = (lambda * (rx - x2) - ry) % p;
            rx = (x2 + p) % p;
            ry = (y2 + p) % p;
        }

        if (naf_k[i] == 0) continue;

        mpz_class qx = x, qy = (naf_k[i] > 0 ? y : (-y + p) % p);
        if (!started) {
            rx = qx;
            ry = qy;
            started = true;
            continue;
        }

        mpz_class lambda, den;
        if (rx == qx) {
            mpz_invert(den.get_mpz_t(), (2 * ry).get_mpz_t(), p.get_mpz_t());
            lambda = ((3 * rx * rx + a) * den) % p;
        } else {
            mpz_invert(den.get_mpz_t(), (qx - rx + p).get_mpz_t(), p.get_mpz_t());
            lambda = ((qy - ry + p) * den) % p;
        }

        mpz_class x3 = (lambda * lambda - rx - qx) % p;
        mpz_class y3 = (lambda * (rx - x3) - ry) % p;
        rx = (x3 + p) % p;
        ry = (y3 + p) % p;
    }

    return {rx, ry};
}

std::pair<mpz_class, mpz_class> ECCFrog512CK2::scalar_mult_montgomery(mpz_class k, mpz_class x, mpz_class y) {
    mpz_class x1 = x, x2 = 0, z2 = 1, x3 = x, z3 = 0;
    mpz_class A = a;
    mpz_class t1, t2, t3, t4;
    bool swap = false;

    for (ssize_t i = mpz_sizeinbase(k.get_mpz_t(), 2) - 1; i >= 0; --i) {
        bool ki = mpz_tstbit(k.get_mpz_t(), i);
        if (swap != ki) {
            std::swap(x2, x3);
            std::swap(z2, z3);
            swap = ki;
        }

        t1 = (x2 + z2) % p;
        t2 = (x2 - z2 + p) % p;
        t3 = (x3 + z3) % p;
        t4 = (x3 - z3 + p) % p;
        mpz_class t5 = (t1 * t4) % p;
        mpz_class t6 = (t2 * t3) % p;

        x3 = ((t5 + t6) * (t5 + t6)) % p;
        z3 = (x1 * ((t5 - t6) * (t5 - t6))) % p;
        mpz_class t7 = (t1 * t1) % p;
        mpz_class t8 = (t2 * t2) % p;
        x2 = (t7 * t8) % p;
        z2 = ((t7 - t8) * ((t7 - t8 + A * ((t7 - t8) % p)) % p)) % p;
    }

    if (swap) std::swap(x2, x3), std::swap(z2, z3);

    mpz_class zinv;
    mpz_invert(zinv.get_mpz_t(), z2.get_mpz_t(), p.get_mpz_t());
    x2 = (x2 * zinv) % p;
    mpz_class y2 = (x2 * x2 * x2 + a * x2 + b) % p;
    mpz_sqrt(y2.get_mpz_t(), y2.get_mpz_t());

    return {x2, y2};
}

std::string ECCFrog512CK2::compress(std::pair<mpz_class, mpz_class> point) {
    return (mpz_even_p(point.second.get_mpz_t()) ? "02" : "03") + point.first.get_str(16);
}

bool ECCFrog512CK2::is_on_curve(mpz_class x, mpz_class y) {
    mpz_class lhs = (y * y) % p;
    mpz_class rhs = (x * x * x + a * x + b) % p;
    return (lhs == rhs);
}

bool ECCFrog512CK2::is_twist_secure() {
    // Apenas simulado: sabemos que o twist não é primo
    return false;
}

void ECCFrog512CK2::benchmark() {
    auto start = std::chrono::high_resolution_clock::now();
    scalar_mult_montgomery(123456789, Gx, Gy);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Montgomery Time: "
              << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
              << " µs\n";

    start = std::chrono::high_resolution_clock::now();
    scalar_mult_NAF(123456789, Gx, Gy);
    end = std::chrono::high_resolution_clock::now();
    std::cout << "NAF Time: "
              << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
              << " µs\n";
}
std::pair<mpz_class, mpz_class> ECCFrog512CK2::endomorphism(mpz_class x, mpz_class y) {
    // β ≡ 2^((p-1)/3) mod p
    static mpz_class beta;
    if (beta == 0) {
        mpz_class exp = (p - 1) / 3;
        mpz_powm(beta.get_mpz_t(), mpz_class(2).get_mpz_t(), exp.get_mpz_t(), p.get_mpz_t());
    }
    mpz_class new_x = (beta * x) % p;
    return {new_x, y};
}

std::pair<mpz_class, mpz_class> ECCFrog512CK2::scalar_mult_GLV(mpz_class k, mpz_class x, mpz_class y) {
    // Decomposição simples para simular GLV: k1 = k/2, k2 = k - k1
    mpz_class k1 = k / 2;
    mpz_class k2 = k - k1;

    auto P1 = scalar_mult_montgomery(k1, x, y);
    auto endo = endomorphism(x, y);
    auto P2 = scalar_mult_montgomery(k2, endo.first, endo.second);

    // Somar P1 + P2
    mpz_class lambda, num, den;

    if (P1.first == P2.first && P1.second == P2.second) {
        num = (3 * P1.first * P1.first + a) % p;
        den = (2 * P1.second) % p;
    } else {
        num = (P2.second - P1.second + p) % p;
        den = (P2.first - P1.first + p) % p;
    }

    mpz_invert(den.get_mpz_t(), den.get_mpz_t(), p.get_mpz_t());
    lambda = (num * den) % p;

    mpz_class xr = (lambda * lambda - P1.first - P2.first + 2 * p) % p;
    mpz_class yr = (lambda * (P1.first - xr + p) - P1.second + p) % p;

    return {xr, yr};
}
