#include "eccfrog512ck2.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <openssl/sha.h>
#include <iostream>
#include <algorithm>

// Helper function to clean PGP armored data
static std::string clean_pgp_data(const std::string& pgp_data) {
    std::string cleaned;
    for (char c : pgp_data) {
        if (isxdigit(c)) {
            cleaned += static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
    }
    return cleaned;
}

ECCFrog512CK2::Point::Point() : x(0), y(0), at_infinity(true) {}

ECCFrog512CK2::Point::Point(const mpz_class& x_val, const mpz_class& y_val)
    : x(x_val), y(y_val), at_infinity(false) {}

std::string ECCFrog512CK2::Point::to_string() const {
    if (at_infinity) return "Point(infinity)";
    std::ostringstream oss;
    oss << "(" << x.get_str() << ", " << y.get_str() << ")";
    return oss.str();
}

std::string ECCFrog512CK2::Point::to_compressed_hex() const {
    if (at_infinity) return "00";
    std::ostringstream oss;
    int parity = mpz_tstbit(y.get_mpz_t(), 0);
    oss << (parity ? "03" : "02");

    std::string x_hex = x.get_str(16);
    if (x_hex.length() > 128) {
        throw std::runtime_error("x coordinate too large");
    }
    x_hex.insert(0, 128 - x_hex.length(), '0');
    oss << x_hex;

    return oss.str();
}

std::vector<unsigned char> ECCFrog512CK2::Point::to_uncompressed_bytes() const {
    if (at_infinity) {
        return {0x00};
    }

    std::vector<unsigned char> bytes(129);
    bytes[0] = 0x04; // Uncompressed prefix

    std::string x_hex = x.get_str(16);
    x_hex.insert(0, 128 - x_hex.length(), '0');
    for (int i = 0; i < 64; ++i) {
        unsigned long xbyte = std::stoul(x_hex.substr(i * 2, 2), nullptr, 16);
        bytes[1 + i] = static_cast<unsigned char>(xbyte);
    }

    std::string y_hex = y.get_str(16);
    y_hex.insert(0, 128 - y_hex.length(), '0');
    for (int i = 0; i < 64; ++i) {
        unsigned long ybyte = std::stoul(y_hex.substr(i * 2, 2), nullptr, 16);
        bytes[65 + i] = static_cast<unsigned char>(ybyte);
    }

    return bytes;
}

ECCFrog512CK2::ECCFrog512CK2() {
    p = mpz_class("9149012705592502490164965176888130701548053918699793689672344807772801105830681498780746622530729418858477103073591918058480028776841126664954537807339721");
    a = p - 7;
    b = mpz_class("95864189850957917703933006131793785649240252916618759767550461391845895018181");
    n = mpz_class("9149012705592502490164965176888130701548053918699793689672344807772801105830557269123255850915745063541133157503707284048429261692283957712127567713136519");
    h = 1;

    G = Point(
        mpz_class("8426241697659200371183582771153260966569955699615044232640972423431947060129573736112298744977332416175021337082775856058058394786264506901662703740544432"),
        mpz_class("4970129934163735248083452609809843496231929620419038489506391366136186485994288320758668172790060801809810688192082146431970683113557239433570011112556001")
    );
}

ECCFrog512CK2::Point ECCFrog512CK2::infinity() const {
    return Point();
}

ECCFrog512CK2::Point ECCFrog512CK2::add_points(const Point& P, const Point& Q) const {
    if (P.at_infinity) return Q;
    if (Q.at_infinity) return P;
    if (P.x == Q.x && (P.y != Q.y || P.y == 0)) return infinity();

    mpz_class lambda;
    if (P.x == Q.x) {
        mpz_class denom = (2 * P.y) % p;
        mpz_class denom_inv;
        if (!mpz_invert(denom_inv.get_mpz_t(), denom.get_mpz_t(), p.get_mpz_t())) {
            return infinity();
        }
        lambda = (3 * P.x * P.x + a) * denom_inv % p;
    } else {
        mpz_class denom = (Q.x - P.x) % p;
        mpz_class denom_inv;
        if (!mpz_invert(denom_inv.get_mpz_t(), denom.get_mpz_t(), p.get_mpz_t())) {
            return infinity();
        }
        lambda = (Q.y - P.y) * denom_inv % p;
    }

    mpz_class xr = (lambda * lambda - P.x - Q.x) % p;
    mpz_class yr = (lambda * (P.x - xr) - P.y) % p;
    if (xr < 0) xr += p;
    if (yr < 0) yr += p;

    return Point(xr, yr);
}

ECCFrog512CK2::Point ECCFrog512CK2::scalar_mul(const Point& P, const mpz_class& k) const {
    Point R = infinity();
    Point Q = P;
    mpz_class scalar = k;

    if (scalar < 0) {
        throw std::runtime_error("Invalid scalar: negative value");
    }

    while (scalar > 0) {
        if (mpz_tstbit(scalar.get_mpz_t(), 0)) {
            R = add_points(R, Q);
        }
        Q = add_points(Q, Q);
        scalar >>= 1;
    }
    return R;
}

ECCFrog512CK2::Point ECCFrog512CK2::point_from_compressed_hex(const std::string& hex) const {
    std::string clean_hex = clean_pgp_data(hex);

    if (clean_hex.size() != 66 ||
       (clean_hex.substr(0, 2) != "02" &&
        clean_hex.substr(0, 2) != "03")) {
        throw std::runtime_error("Invalid compressed point format: must be 66 hex chars starting with 02/03");
    }

    mpz_class x(clean_hex.substr(2), 16);
    if (x >= p) {
        throw std::runtime_error("x coordinate exceeds field size");
    }

    mpz_class y_sq = (x*x*x + a*x + b) % p;

    mpz_class y;
    mpz_class exponent = (p + 1) / 4;
    mpz_powm(y.get_mpz_t(), y_sq.get_mpz_t(), exponent.get_mpz_t(), p.get_mpz_t());

    bool y_parity = mpz_tstbit(y.get_mpz_t(), 0);
    bool desired_parity = (clean_hex.substr(0, 2) == "03");

    if (y_parity != desired_parity) {
        y = p - y;
    }

    mpz_class check = (y*y) % p;
    mpz_class expected = (x*x*x + a*x + b) % p;
    if (check != expected) {
        throw std::runtime_error("Derived point not on curve");
    }

    return Point(x, y);
}

ECCFrog512CK2::Point ECCFrog512CK2::point_from_uncompressed(const std::vector<unsigned char>& bytes) const {
    if (bytes.size() != 129 || bytes[0] != 0x04) {
        throw std::runtime_error("Invalid uncompressed format: must be 129 bytes starting with 0x04");
    }

    mpz_class x;
    std::string x_hex;
    for (int i = 1; i <= 64; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", bytes[i]);
        x_hex += buf;
    }
    x.set_str(x_hex, 16);

    mpz_class y;
    std::string y_hex;
    for (int i = 65; i <= 128; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", bytes[i]);
        y_hex += buf;
    }
    y.set_str(y_hex, 16);

    mpz_class lhs = (y * y) % p;
    mpz_class rhs = (x*x*x + a*x + b) % p;
    if (lhs != rhs) {
        throw std::runtime_error("Point not on curve");
    }

    return Point(x, y);
}

ECCFrog512CK2::Point ECCFrog512CK2::point_from_pgp(const std::string& pgp_data) const {
    std::string clean_hex = clean_pgp_data(pgp_data);

    if (clean_hex.empty()) {
        throw std::runtime_error("Empty PGP data");
    }

    if (clean_hex.size() == 66 &&
       (clean_hex.substr(0, 2) == "02" ||
        clean_hex.substr(0, 2) == "03")) {
        return point_from_compressed_hex(clean_hex);
    }
    else if (clean_hex.size() == 258 && clean_hex.substr(0, 2) == "04") {
        std::vector<unsigned char> bytes;
        for (size_t i = 0; i < clean_hex.length(); i += 2) {
            bytes.push_back(static_cast<unsigned char>(
                std::stoul(clean_hex.substr(i, 2), nullptr, 16)));
        }
        return point_from_uncompressed(bytes);
    }
    else {
        throw std::runtime_error("Unrecognized PGP key format");
    }
}
