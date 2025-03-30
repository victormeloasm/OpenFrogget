#ifndef ECCFROG512CK2_H
#define ECCFROG512CK2_H

#include <gmpxx.h>
#include <vector>
#include <string>

class ECCFrog512CK2 {
public:
    struct Point {
        mpz_class x, y;
        bool at_infinity;

        Point();
        Point(const mpz_class& x_val, const mpz_class& y_val);
        std::string to_string() const;
        std::string to_compressed_hex() const;
        std::vector<unsigned char> to_uncompressed_bytes() const;
    };

    ECCFrog512CK2();

    mpz_class get_n() const { return n; }
    Point get_G() const { return G; }

    Point infinity() const;
    Point add_points(const Point& P, const Point& Q) const;
    Point scalar_mul(const Point& P, const mpz_class& k) const;
    Point point_from_compressed_hex(const std::string& hex) const;
    Point point_from_uncompressed(const std::vector<unsigned char>& bytes) const;
    Point point_from_pgp(const std::string& pgp_data) const;

private:
    mpz_class p, a, b, n, h;
    Point G;
};

#endif