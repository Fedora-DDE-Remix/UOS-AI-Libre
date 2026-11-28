#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif // __clang__

namespace rust {
inline namespace cxxbridge1 {
// #include "rust/cxx.h"

struct unsafe_bitcopy_t;

namespace {
template <typename T>
class impl;
} // namespace

#ifndef CXXBRIDGE1_RUST_STRING
#define CXXBRIDGE1_RUST_STRING
class String final {
public:
  String() noexcept;
  String(const String &) noexcept;
  String(String &&) noexcept;
  ~String() noexcept;

  String(const std::string &);
  String(const char *);
  String(const char *, std::size_t);
  String(const char16_t *);
  String(const char16_t *, std::size_t);
#ifdef __cpp_char8_t
  String(const char8_t *s);
  String(const char8_t *s, std::size_t len);
#endif

  static String lossy(const std::string &) noexcept;
  static String lossy(const char *) noexcept;
  static String lossy(const char *, std::size_t) noexcept;
  static String lossy(const char16_t *) noexcept;
  static String lossy(const char16_t *, std::size_t) noexcept;

  String &operator=(const String &) & noexcept;
  String &operator=(String &&) & noexcept;

  explicit operator std::string() const;

  const char *data() const noexcept;
  char *data() noexcept;

  std::size_t size() const noexcept;
  std::size_t length() const noexcept;
  bool empty() const noexcept;

  const char *c_str() noexcept;

  std::size_t capacity() const noexcept;
  void reserve(size_t new_cap) noexcept;

  using iterator = char *;
  iterator begin() noexcept;
  iterator end() noexcept;

  using const_iterator = const char *;
  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;
  const_iterator cbegin() const noexcept;
  const_iterator cend() const noexcept;

  bool operator==(const String &) const noexcept;
  bool operator!=(const String &) const noexcept;
  bool operator<(const String &) const noexcept;
  bool operator<=(const String &) const noexcept;
  bool operator>(const String &) const noexcept;
  bool operator>=(const String &) const noexcept;

  void swap(String &) noexcept;

  String(unsafe_bitcopy_t, const String &) noexcept;

private:
  struct lossy_t;
  String(lossy_t, const char *, std::size_t) noexcept;
  String(lossy_t, const char16_t *, std::size_t) noexcept;
  friend void swap(String &lhs, String &rhs) noexcept { lhs.swap(rhs); }

  std::array<std::uintptr_t, 3> repr;
};
#endif // CXXBRIDGE1_RUST_STRING

#ifndef CXXBRIDGE1_RUST_STR
#define CXXBRIDGE1_RUST_STR
class Str final {
public:
  Str() noexcept;
  Str(const String &) noexcept;
  Str(const std::string &);
  Str(const char *);
  Str(const char *, std::size_t);

  Str &operator=(const Str &) & noexcept = default;

  explicit operator std::string() const;
#if __cplusplus >= 201703L
  explicit operator std::string_view() const;
#endif

  const char *data() const noexcept;
  std::size_t size() const noexcept;
  std::size_t length() const noexcept;
  bool empty() const noexcept;

  Str(const Str &) noexcept = default;
  ~Str() noexcept = default;

  using iterator = const char *;
  using const_iterator = const char *;
  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;
  const_iterator cbegin() const noexcept;
  const_iterator cend() const noexcept;

  bool operator==(const Str &) const noexcept;
  bool operator!=(const Str &) const noexcept;
  bool operator<(const Str &) const noexcept;
  bool operator<=(const Str &) const noexcept;
  bool operator>(const Str &) const noexcept;
  bool operator>=(const Str &) const noexcept;

  void swap(Str &) noexcept;

private:
  class uninit;
  Str(uninit) noexcept;
  friend impl<Str>;

  std::array<std::uintptr_t, 2> repr;
};
#endif // CXXBRIDGE1_RUST_STR

#ifndef CXXBRIDGE1_RUST_BOX
#define CXXBRIDGE1_RUST_BOX
template <typename T>
class Box final {
public:
  using element_type = T;
  using const_pointer =
      typename std::add_pointer<typename std::add_const<T>::type>::type;
  using pointer = typename std::add_pointer<T>::type;

  Box() = delete;
  Box(Box &&) noexcept;
  ~Box() noexcept;

  explicit Box(const T &);
  explicit Box(T &&);

  Box &operator=(Box &&) & noexcept;

  const T *operator->() const noexcept;
  const T &operator*() const noexcept;
  T *operator->() noexcept;
  T &operator*() noexcept;

  template <typename... Fields>
  static Box in_place(Fields &&...);

  void swap(Box &) noexcept;

  static Box from_raw(T *) noexcept;

  T *into_raw() noexcept;

  /* Deprecated */ using value_type = element_type;

private:
  class uninit;
  class allocation;
  Box(uninit) noexcept;
  void drop() noexcept;

  friend void swap(Box &lhs, Box &rhs) noexcept { lhs.swap(rhs); }

  T *ptr;
};

template <typename T>
class Box<T>::uninit {};

template <typename T>
class Box<T>::allocation {
  static T *alloc() noexcept;
  static void dealloc(T *) noexcept;

public:
  allocation() noexcept : ptr(alloc()) {}
  ~allocation() noexcept {
    if (this->ptr) {
      dealloc(this->ptr);
    }
  }
  T *ptr;
};

template <typename T>
Box<T>::Box(Box &&other) noexcept : ptr(other.ptr) {
  other.ptr = nullptr;
}

template <typename T>
Box<T>::Box(const T &val) {
  allocation alloc;
  ::new (alloc.ptr) T(val);
  this->ptr = alloc.ptr;
  alloc.ptr = nullptr;
}

template <typename T>
Box<T>::Box(T &&val) {
  allocation alloc;
  ::new (alloc.ptr) T(std::move(val));
  this->ptr = alloc.ptr;
  alloc.ptr = nullptr;
}

template <typename T>
Box<T>::~Box() noexcept {
  if (this->ptr) {
    this->drop();
  }
}

template <typename T>
Box<T> &Box<T>::operator=(Box &&other) & noexcept {
  if (this->ptr) {
    this->drop();
  }
  this->ptr = other.ptr;
  other.ptr = nullptr;
  return *this;
}

template <typename T>
const T *Box<T>::operator->() const noexcept {
  return this->ptr;
}

template <typename T>
const T &Box<T>::operator*() const noexcept {
  return *this->ptr;
}

template <typename T>
T *Box<T>::operator->() noexcept {
  return this->ptr;
}

template <typename T>
T &Box<T>::operator*() noexcept {
  return *this->ptr;
}

template <typename T>
template <typename... Fields>
Box<T> Box<T>::in_place(Fields &&...fields) {
  allocation alloc;
  auto ptr = alloc.ptr;
  ::new (ptr) T{std::forward<Fields>(fields)...};
  alloc.ptr = nullptr;
  return from_raw(ptr);
}

template <typename T>
void Box<T>::swap(Box &rhs) noexcept {
  using std::swap;
  swap(this->ptr, rhs.ptr);
}

template <typename T>
Box<T> Box<T>::from_raw(T *raw) noexcept {
  Box box = uninit{};
  box.ptr = raw;
  return box;
}

template <typename T>
T *Box<T>::into_raw() noexcept {
  T *raw = this->ptr;
  this->ptr = nullptr;
  return raw;
}

template <typename T>
Box<T>::Box(uninit) noexcept {}
#endif // CXXBRIDGE1_RUST_BOX

#ifndef CXXBRIDGE1_RUST_OPAQUE
#define CXXBRIDGE1_RUST_OPAQUE
class Opaque {
public:
  Opaque() = delete;
  Opaque(const Opaque &) = delete;
  ~Opaque() = delete;
};
#endif // CXXBRIDGE1_RUST_OPAQUE

#ifndef CXXBRIDGE1_IS_COMPLETE
#define CXXBRIDGE1_IS_COMPLETE
namespace detail {
namespace {
template <typename T, typename = std::size_t>
struct is_complete : std::false_type {};
template <typename T>
struct is_complete<T, decltype(sizeof(T))> : std::true_type {};
} // namespace
} // namespace detail
#endif // CXXBRIDGE1_IS_COMPLETE

#ifndef CXXBRIDGE1_LAYOUT
#define CXXBRIDGE1_LAYOUT
class layout {
  template <typename T>
  friend std::size_t size_of();
  template <typename T>
  friend std::size_t align_of();
  template <typename T>
  static typename std::enable_if<std::is_base_of<Opaque, T>::value,
                                 std::size_t>::type
  do_size_of() {
    return T::layout::size();
  }
  template <typename T>
  static typename std::enable_if<!std::is_base_of<Opaque, T>::value,
                                 std::size_t>::type
  do_size_of() {
    return sizeof(T);
  }
  template <typename T>
  static
      typename std::enable_if<detail::is_complete<T>::value, std::size_t>::type
      size_of() {
    return do_size_of<T>();
  }
  template <typename T>
  static typename std::enable_if<std::is_base_of<Opaque, T>::value,
                                 std::size_t>::type
  do_align_of() {
    return T::layout::align();
  }
  template <typename T>
  static typename std::enable_if<!std::is_base_of<Opaque, T>::value,
                                 std::size_t>::type
  do_align_of() {
    return alignof(T);
  }
  template <typename T>
  static
      typename std::enable_if<detail::is_complete<T>::value, std::size_t>::type
      align_of() {
    return do_align_of<T>();
  }
};

template <typename T>
std::size_t size_of() {
  return layout::size_of<T>();
}

template <typename T>
std::size_t align_of() {
  return layout::align_of<T>();
}
#endif // CXXBRIDGE1_LAYOUT
} // namespace cxxbridge1
} // namespace rust

struct BridgeService;

#ifndef CXXBRIDGE1_STRUCT_BridgeService
#define CXXBRIDGE1_STRUCT_BridgeService
struct BridgeService final : public ::rust::Opaque {
  ~BridgeService() = delete;

private:
  friend ::rust::layout;
  struct layout {
    static ::std::size_t size() noexcept;
    static ::std::size_t align() noexcept;
  };
};
#endif // CXXBRIDGE1_STRUCT_BridgeService

::rust::Box<::BridgeService> new_service(::rust::Str config_json) noexcept;

::rust::Box<::BridgeService> new_service_from_default_config() noexcept;

::rust::String create_session(::BridgeService &service, ::rust::Str model) noexcept;

::rust::String send_message(::BridgeService &service, ::rust::Str session_id, ::rust::Str text, ::rust::Str images_json) noexcept;

::rust::String wait_event(::BridgeService &service, ::std::uint64_t timeout_ms) noexcept;

::rust::String poll_event(::BridgeService &service) noexcept;

void cancel_session(::BridgeService &service, ::rust::Str session_id) noexcept;

bool approve(::BridgeService &service, ::rust::Str request_id, ::std::int32_t decision) noexcept;

bool respond_interaction(::BridgeService &service, ::rust::Str request_id, ::rust::Str response_json) noexcept;

::rust::String tool_names(::BridgeService &service, ::rust::Str session_id) noexcept;

::rust::String tool_catalog(::BridgeService &service) noexcept;

::rust::String policy_info(::BridgeService &service) noexcept;

::rust::String save_policy(::BridgeService &service, ::rust::Str fields_json) noexcept;

::rust::String save_policy_rules(::BridgeService &service, ::rust::Str rules_json, ::rust::Str expected_fingerprint) noexcept;

::rust::String preview_policy_command(::BridgeService &service, ::rust::Str command, ::rust::Str draft_rules_json) noexcept;

::rust::String session_approvals(::BridgeService &service, ::rust::Str session_id) noexcept;

::rust::String revoke_session_approval(::BridgeService &service, ::rust::Str session_id, ::rust::Str kind, ::rust::Str first, ::rust::Str second) noexcept;

::rust::String clear_session_approvals(::BridgeService &service, ::rust::Str session_id) noexcept;

::rust::String skill_catalog(::BridgeService &service) noexcept;

::rust::String refresh_skills_live(::BridgeService &service) noexcept;

::rust::String trust_skill(::BridgeService &service, ::rust::Str name) noexcept;

::rust::String skill_read(::BridgeService &service, ::rust::Str name, ::std::uint32_t max_bytes) noexcept;

::rust::String skill_verify(::BridgeService &service, ::rust::Str name) noexcept;

::rust::String skill_remove(::BridgeService &service, ::rust::Str name) noexcept;

::rust::String skill_search_start(::BridgeService &service, ::rust::Str query, ::std::uint32_t limit) noexcept;

::rust::String skill_install_start(::BridgeService &service, ::rust::Str skill_id) noexcept;

::rust::String mcp_status(::BridgeService &service) noexcept;

::rust::String refresh_mcp_tools(::BridgeService &service) noexcept;

::rust::String mcp_reload(::BridgeService &service) noexcept;

::rust::String mcp_add(::BridgeService &service, ::rust::Str args_json) noexcept;

::rust::String mcp_update(::BridgeService &service, ::rust::Str name, ::rust::Str args_json) noexcept;

::rust::String mcp_remove(::BridgeService &service, ::rust::Str name) noexcept;

::rust::String mcp_set_master_enabled(::BridgeService &service, bool enabled) noexcept;

::rust::String mcp_set_enabled(::BridgeService &service, ::rust::Str name, bool enabled) noexcept;

::rust::String mcp_set_trust(::BridgeService &service, ::rust::Str name, ::rust::Str tool, ::rust::Str level) noexcept;

::rust::String mcp_test(::BridgeService &service, ::rust::Str name) noexcept;

::rust::String mcp_test_draft(::BridgeService &service, ::rust::Str args_json) noexcept;

::rust::String memory_list(::BridgeService &service, ::std::uint32_t limit) noexcept;

::rust::String audit_tail(::BridgeService &service, ::std::uint32_t limit) noexcept;

::rust::String clear_audit_events(::BridgeService &service) noexcept;

::rust::String list_sessions(::BridgeService &service, ::std::uint32_t limit) noexcept;

::rust::String resume_session(::BridgeService &service, ::rust::Str session_id) noexcept;

::rust::String resume_session_window(::BridgeService &service, ::rust::Str session_id, ::std::uint32_t limit) noexcept;

::rust::String history_page(::BridgeService &service, ::rust::Str session_id, ::std::uint32_t before, ::std::uint32_t limit) noexcept;

::rust::String undo_to_message(::BridgeService &service, ::rust::Str session_id, ::std::uint32_t keep_count) noexcept;

::rust::String delete_session(::BridgeService &service, ::rust::Str session_id) noexcept;

::rust::String rename_session(::BridgeService &service, ::rust::Str session_id, ::rust::Str title) noexcept;

::rust::String config_info(::BridgeService &service) noexcept;

::rust::String tool_loop_limit(::BridgeService &service) noexcept;

::rust::String save_tool_loop_limit(::BridgeService &service, ::std::uint32_t limit) noexcept;

::rust::String save_model_config(::BridgeService &service, ::rust::Str model, ::rust::Str api_base, ::rust::Str api_key) noexcept;

::rust::String provider_info(::BridgeService &service) noexcept;

::rust::String save_provider_config(::BridgeService &service, ::rust::Str name, ::rust::Str api_base, ::rust::Str api_key, ::rust::Str model, ::rust::Str protocol, bool activate, ::rust::Str current_name) noexcept;

::rust::String delete_provider_account(::BridgeService &service, ::rust::Str name) noexcept;

::rust::String model_fetch_start(::BridgeService &service, ::rust::Str api_base, ::rust::Str api_key, ::rust::Str protocol) noexcept;

::rust::String set_workspace(::BridgeService &service, ::rust::Str session_id, ::rust::Str path) noexcept;

void shutdown(::BridgeService &service) noexcept;

#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
