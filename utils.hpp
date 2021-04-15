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
