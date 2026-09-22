//===- SignDomain.h - The abstract domain ---------------------------------===//
//
// A seven-point lattice recording sign information about an integer.
//
//                          Top
//                        ╱     ╲
//                 ZeroOrNeg   ZeroOrPos
//                   |    ╲    ╱    |
//               Negative  Zero  Positive
//                       ╲   │   /
//                        Bottom
//
//   * a default constructor, which must produce the bottom element, because the
//     solver starts every value at bottom and raises it as facts arrive;
//   * a static join(), which must be commutative, associative, idempotent, and
//     monotone -- assertions in Lattice<> check monotonicity in debug builds;
//   * operator== and print().
//
//===----------------------------------------------------------------------===//

#ifndef SIGN_DOMAIN_H
#define SIGN_DOMAIN_H

#include "llvm/Support/raw_ostream.h"

namespace sign {

enum class Kind { Bottom, Zero, Negative, ZeroOrNeg, /* One,*/ Positive, ZeroOrPos, Top };

inline constexpr unsigned kKindCount = 7;

inline constexpr unsigned kindIndex(Kind kind) {
    switch (kind) {
    case Kind::Bottom:
        return 0;
    case Kind::Zero:
        return 1;
    case Kind::Negative:
        return 2;
    case Kind::ZeroOrNeg:
        return 3;
    case Kind::Positive:
        return 4;
    case Kind::ZeroOrPos:
        return 5;
    case Kind::Top:
        return 6;
    }
    return 6;
}

inline const char* name(Kind kind) {
    switch (kind) {
    case Kind::Bottom:
        return "bottom";
    case Kind::Zero:
        return "zero";
    case Kind::Negative:
        return "negative";
    case Kind::ZeroOrNeg:
        return "zero-or-negative";
    case Kind::Positive:
        return "positive";
    case Kind::ZeroOrPos:
        return "zero-or-positive";
    case Kind::Top:
        return "top";
    }
    return "top";
}

struct SignState {
    Kind kind = Kind::Bottom;

    SignState() = default;
    SignState(Kind kind) : kind(kind) {}

    static SignState bottom() { return Kind::Bottom; }
    static SignState top() { return Kind::Top; }

    bool isBottom() const { return kind == Kind::Bottom; }

    /// Least upper bound in the sign lattice.
    static SignState join(const SignState& lhs, const SignState& rhs) {
        constexpr Kind Bo = Kind::Bottom;
        constexpr Kind Ze = Kind::Zero;
        constexpr Kind Ne = Kind::Negative;
        constexpr Kind ZN = Kind::ZeroOrNeg;
        // constexpr Kind On = Kind::One;
        constexpr Kind Po = Kind::Positive;
        constexpr Kind ZP = Kind::ZeroOrPos;
        constexpr Kind To = Kind::Top;

        static constexpr Kind joinTable[kKindCount][kKindCount] = {
            //        Bo  Ze  Ne  ZN  Po  ZP  To
            /* Bo */ {Bo, Ze, Ne, ZN, Po, ZP, To},
            /* Ze */ {Ze, Ze, ZN, ZN, ZP, ZP, To},
            /* Ne */ {Ne, ZN, Ne, ZN, To, To, To},
            /* ZN */ {ZN, ZN, ZN, ZN, To, To, To},
            /* Po */ {Po, ZP, To, To, Po, ZP, To},
            /* ZP */ {ZP, ZP, To, To, ZP, ZP, To},
            /* To */ {To, To, To, To, To, To, To},
        };

        return joinTable[kindIndex(lhs.kind)][kindIndex(rhs.kind)];
    }

    bool operator==(const SignState& other) const { return kind == other.kind; }
    bool operator!=(const SignState& other) const { return kind != other.kind; }

    void print(llvm::raw_ostream& os) const { os << name(kind); }
};

inline llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const SignState& state) {
    state.print(os);
    return os;
}

} // namespace sign

#endif
