#pragma once

#include <memory>

template<typename vec_t1, typename vec_t2>
inline void append_move(vec_t1 &a, vec_t2 &b) {
  a.reserve(a.size() + b.size());
  a.insert(a.end(), std::make_move_iterator(b.begin()), std::make_move_iterator(b.end()));
}

template<typename vec_t>
inline void append_copy(vec_t &a, const vec_t &b) {
  a.reserve(a.size() + b.size());
  a.insert(a.end(), b.begin(), b.end());
}

// implementation from https://en.cppreference.com/w/cpp/memory/shared_ptr/pointer_cast, copied here for C++17
template< class T, class U > 
std::shared_ptr<T> dcast( const std::shared_ptr<U>& r ) noexcept {
  if (auto p = dynamic_cast<typename std::shared_ptr<T>::element_type*>(r.get())) {
    return std::shared_ptr<T>(r, p);
  } else {
    return std::shared_ptr<T>();
  }
}

// overload op constructor: from https://en.cppreference.com/w/cpp/utility/variant/visit
// helper constant for the visitor #3
template<char> inline constexpr bool always_false_v = false;
// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// hash function for std::tuple.
// from https://stackoverflow.com/a/7115547/11815215
namespace std {
namespace {
// Code from boost
// Reciprocal of the golden ratio helps spread entropy
//     and handles duplicates.
// See Mike Seymour in magic-numbers-in-boosthash-combine:
//     http://stackoverflow.com/questions/4948780

template <class T>
inline void hash_combine(std::size_t& seed, T const& v) {
  seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

// Recursive template code derived from Matthieu M.
template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
struct HashValueImpl {
  static void apply(size_t& seed, Tuple const& tuple) {
    HashValueImpl<Tuple, Index-1>::apply(seed, tuple);
    hash_combine(seed, std::get<Index>(tuple));
  }
};

template <class Tuple>
struct HashValueImpl<Tuple,0> {
  static void apply(size_t& seed, Tuple const& tuple) {
    hash_combine(seed, std::get<0>(tuple));
  }
};
}

template <typename ... TT>
struct hash<std::tuple<TT...>> {
  size_t
  operator()(std::tuple<TT...> const& tt) const {
    size_t seed = 0;
    HashValueImpl<std::tuple<TT...> >::apply(seed, tt);
    return seed;
  }
};
}
