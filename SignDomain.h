//===- SignDomain.h - The abstract domain ---------------------------------===//
//
// A five-point lattice recording sign information about an integer.
//
//                           Top
//                      ╱     │     ╲
//              Negative    Zero    Positive
//                      ╲     │     ╱
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

enum class Kind { Bottom, Negative, Zero, Positive, Top };

inline const char* name(Kind kind) {
    switch (kind) {
    case Kind::Bottom:
        return "bottom";
    case Kind::Negative:
        return "negative";
    case Kind::Zero:
        return "zero";
    case Kind::Positive:
        return "positive";
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

    /// Least upper bound.  Two disagreeing facts lose all information.
    static SignState join(const SignState& lhs, const SignState& rhs) {
        if (lhs.kind == Kind::Bottom) {
            return rhs;
        }
        if (rhs.kind == Kind::Bottom) {
            return lhs;
        }
        if (lhs.kind == rhs.kind) {
            return lhs;
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
