// TODO(zyf):
// 并非直接迁移chromium实现，是一个简单的快速实现，后续考虑替换为chromium的实现或等待升级到C++23

#ifndef BASE_TYPES_EXPECTED_H_
#define BASE_TYPES_EXPECTED_H_
#include <stdexcept>
#include <utility>
#include <variant>
namespace kiwi {
namespace base {

template <typename E>
class unexpected {
 private:
  E err;

 public:
  constexpr explicit unexpected(const E& e) : err(e) {}
  constexpr explicit unexpected(E&& e) : err(std::move(e)) {}

  constexpr E& error() & { return err; }
  constexpr const E& error() const& { return err; }
  constexpr E&& error() && { return std::move(err); }
  constexpr const E&& error() const&& { return std::move(err); }
};

template <typename E>
constexpr unexpected<E> make_unexpected(E&& e) {
  return unexpected<E>(std::forward<E>(e));
}

template <typename T, typename E>
class expected {
 private:
  std::variant<T, E> data;

 public:
  constexpr expected(const T& value) : data(value) {}
  constexpr expected(T&& value) : data(std::move(value)) {}

  constexpr expected(const unexpected<E>& unex) : data(unex.error()) {}
  constexpr expected(unexpected<E>&& unex) : data(std::move(unex).error()) {}

  constexpr bool has_value() const noexcept {
    return std::holds_alternative<T>(data);
  }

  constexpr explicit operator bool() const noexcept { return has_value(); }

  constexpr T& value() & {
    if (!has_value()) {
      throw std::bad_variant_access();
    }
    return std::get<T>(data);
  }
  constexpr const T& value() const& {
    if (!has_value()) {
      throw std::bad_variant_access();
    }
    return std::get<T>(data);
  }
  constexpr T&& value() && {
    if (!has_value()) {
      throw std::bad_variant_access();
    }
    return std::move(std::get<T>(data));
  }
  constexpr const T&& value() const&& {
    if (!has_value()) {
      throw std::bad_variant_access();
    }
    return std::move(std::get<T>(data));
  }

  constexpr E& error() & {
    if (has_value()) {
      throw std::bad_variant_access();
    }
    return std::get<E>(data);
  }
  constexpr const E& error() const& {
    if (has_value()) {
      throw std::bad_variant_access();
    }
    return std::get<E>(data);
  }
  constexpr E&& error() && {
    if (has_value()) {
      throw std::bad_variant_access();
    }
    return std::move(std::get<E>(data));
  }
  constexpr const E&& error() const&& {
    if (has_value()) {
      throw std::bad_variant_access();
    }
    return std::move(std::get<E>(data));
  }
};
}  // namespace base
}  // namespace kiwi

#endif  // BASE_TYPES_EXPECTED_H_
