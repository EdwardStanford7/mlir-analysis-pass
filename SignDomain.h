//===- SignDomain.h - The abstract domain ---------------------------------===//
//
// An eight-point lattice recording sign information about an integer.
//
//                              Top
//                         ╱           ╲
//                ZeroOrNeg           ZeroOrPos
//                  ╱     ╲           ╱      ╲
//          Negative       Zero          Positive
//                  ╲       │                   /
//                   ╲      │            One
//                    ╲     │            ╱
//                         Bottom
//
// This is the file to replace first when building a different analysis.  MLIR's
// dataflow framework asks only three things of a lattice value:
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

enum class Kind { Bottom, Zero, Negative, ZeroOrNeg, One, Positive, ZeroOrPos, Top };

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
    case Kind::One:
        return "one";
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
        // Bits represent negative values, zero, one, and positive values other
        // than one. Each lattice element is the smallest available abstraction
        // containing its bits.
        auto mask = [](Kind kind) -> unsigned {
            switch (kind) {
            case Kind::Bottom:
                return 0b0000;
            case Kind::Negative:
                return 0b0001;
            case Kind::Zero:
                return 0b0010;
            case Kind::One:
                return 0b0100;
            case Kind::Positive:
                return 0b1100;
            case Kind::ZeroOrNeg:
                return 0b0011;
            case Kind::ZeroOrPos:
                return 0b1110;
            case Kind::Top:
                return 0b1111;
            }
            return 0b1111;
        };

        unsigned joined = mask(lhs.kind) | mask(rhs.kind);
        for (Kind candidate : {Kind::Bottom, Kind::Negative, Kind::Zero, Kind::One, Kind::Positive, Kind::ZeroOrNeg, Kind::ZeroOrPos, Kind::Top}) {
            unsigned candidateMask = mask(candidate);
            if ((candidateMask & joined) == joined) {
                return candidate;
            }
        }
        return top();
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
