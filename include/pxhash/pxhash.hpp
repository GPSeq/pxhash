#ifndef PXHASH_PXHASH_HPP
#define PXHASH_PXHASH_HPP

#include <cstdint>
#include <fstream>
#include <functional>
#include <ios>
#include <istream>
#include <iterator>
#include <limits>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
  #include <immintrin.h>
#endif

namespace pxhash {

static constexpr std::uint8_t EMPTY = 0x80;
static constexpr std::uint8_t DELETED = 0xFE;

#if defined(__AVX2__)
static constexpr std::size_t GROUP_SIZE = 32;
#else
static constexpr std::size_t GROUP_SIZE = 16;
#endif

inline std::size_t align_up(std::size_t n, std::size_t a) {
  return (n + (a - 1)) & ~(a - 1);
}

inline std::size_t next_power_of_two(std::size_t n) {
  if (n <= 1) return 1;
  --n;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
#if INTPTR_MAX == INT64_MAX
  n |= n >> 32;
#endif
  return n + 1;
}

inline std::size_t mix_hash(std::size_t h) {
  if constexpr (sizeof(std::size_t) == 8) {
    std::uint64_t x = static_cast<std::uint64_t>(h);
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return static_cast<std::size_t>(x);
  } else {
    std::uint32_t x = static_cast<std::uint32_t>(h);
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return static_cast<std::size_t>(x);
  }
}

inline std::uint8_t h2_from_hash(std::size_t h) {
  return static_cast<std::uint8_t>((h >> (sizeof(std::size_t) * 8 - 7)) & 0x7F);
}

template <class K, class V>
struct Slot {
  template <class KArg, class VArg>
  Slot(KArg&& k, VArg&& v) : key(std::forward<KArg>(k)), value(std::forward<VArg>(v)) {}

  K key;
  V value;
};

template <typename KeyType, typename ValueType, typename Hash = std::hash<KeyType>,
          typename Eq = std::equal_to<KeyType>,
          typename Allocator = std::allocator<Slot<KeyType, ValueType>>>
class PXHash {
public:
  using value_type = Slot<KeyType, ValueType>;
  using key_type = KeyType;
  using mapped_type = ValueType;
  using hasher = Hash;
  using key_equal = Eq;
  using allocator_type = Allocator;
  using size_type = std::size_t;

  static constexpr std::uint32_t kBinaryMagic = 0x50584842u;
  static constexpr std::uint16_t kBinaryVersion = 1;
  static constexpr float kMaxLoadFactor = 0.875F;

  PXHash() = default;

  explicit PXHash(const Allocator& allocator) : allocator_(allocator), slots_(allocator) {}

  explicit PXHash(size_type initial_capacity) {
    reserve(initial_capacity);
  }

  PXHash(size_type initial_capacity, Hash hash, Eq eq = Eq{})
      : hasher_(std::move(hash)), eq_(std::move(eq)) {
    reserve(initial_capacity);
  }

  PXHash(size_type initial_capacity, Hash hash, Eq eq, const Allocator& allocator)
      : hasher_(std::move(hash)), eq_(std::move(eq)), allocator_(allocator), slots_(allocator) {
    reserve(initial_capacity);
  }

  PXHash(const PXHash&) = default;
  PXHash& operator=(const PXHash&) = default;
  PXHash(PXHash&&) noexcept = default;
  PXHash& operator=(PXHash&&) noexcept = default;
  ~PXHash() = default;

  template <bool IsConst>
  class basic_iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = pxhash::Slot<KeyType, ValueType>;
    using reference = std::conditional_t<IsConst, const value_type&, value_type&>;
    using pointer = std::conditional_t<IsConst, const value_type*, value_type*>;
    using parent_type = std::conditional_t<IsConst, const PXHash, PXHash>;

    basic_iterator() = default;

    basic_iterator(parent_type* parent, size_type index) : parent_(parent), index_(index) {
      skip_empty();
    }

    reference operator*() const { return *parent_->slots_[index_]; }
    pointer operator->() const { return &*parent_->slots_[index_]; }

    basic_iterator& operator++() {
      ++index_;
      skip_empty();
      return *this;
    }

    basic_iterator operator++(int) {
      basic_iterator copy = *this;
      ++(*this);
      return copy;
    }

    friend bool operator==(const basic_iterator& a, const basic_iterator& b) {
      return a.parent_ == b.parent_ && a.index_ == b.index_;
    }

    friend bool operator!=(const basic_iterator& a, const basic_iterator& b) { return !(a == b); }

  private:
    void skip_empty() {
      if (parent_ == nullptr) return;
      while (index_ < parent_->capacity_ && !parent_->is_occupied(index_)) ++index_;
    }

    parent_type* parent_{nullptr};
    size_type index_{0};
  };

  using iterator = basic_iterator<false>;
  using const_iterator = basic_iterator<true>;

  [[nodiscard]] size_type size() const noexcept { return size_; }
  [[nodiscard]] size_type capacity() const noexcept { return capacity_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] allocator_type get_allocator() const { return allocator_; }
  [[nodiscard]] float max_load_factor() const noexcept { return kMaxLoadFactor; }
  [[nodiscard]] float load_factor() const noexcept {
    return capacity_ == 0 ? 0.0F : static_cast<float>(size_) / static_cast<float>(capacity_);
  }

  iterator begin() noexcept { return iterator(this, 0); }
  iterator end() noexcept { return iterator(this, capacity_); }
  const_iterator begin() const noexcept { return const_iterator(this, 0); }
  const_iterator end() const noexcept { return const_iterator(this, capacity_); }
  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

  void clear() noexcept {
    for (auto& slot : slots_) slot.reset();
    ctrl_.assign(capacity_ + GROUP_SIZE, EMPTY);
    size_ = 0;
    deleted_ = 0;
  }

  void reserve(size_type n) {
    if (n == 0) return;
    if (n > (std::numeric_limits<size_type>::max() / kDenom) * kNumer) {
      throw std::length_error("pxhash reserve request is too large");
    }

    size_type need = (n * kDenom) / kNumer + 1;
    size_type cap = next_power_of_two(need);
    if (cap < min_capacity()) cap = min_capacity();
    cap = align_up(cap, GROUP_SIZE);
    if (cap <= capacity_) return;
    rehash(cap);
  }

  bool insert(const KeyType& key, const ValueType& value) {
    return insert_or_assign(key, value);
  }

  bool insert(KeyType&& key, ValueType&& value) {
    return insert_or_assign(std::move(key), std::move(value));
  }

  template <class KArg, class VArg>
  bool insert_or_assign(KArg&& key, VArg&& value) {
    maybe_grow_for_insert();
    return insert_or_assign_impl(std::forward<KArg>(key), std::forward<VArg>(value));
  }

  template <class... Args>
  bool try_emplace(const KeyType& key, Args&&... args) {
    maybe_grow_for_insert();
    if (find_slot(key) != npos()) return false;
    return emplace_new(key, ValueType(std::forward<Args>(args)...));
  }

  template <class... Args>
  bool try_emplace(KeyType&& key, Args&&... args) {
    maybe_grow_for_insert();
    if (find_slot(key) != npos()) return false;
    return emplace_new(std::move(key), ValueType(std::forward<Args>(args)...));
  }

  ValueType& operator[](const KeyType& key) {
    static_assert(std::is_default_constructible_v<ValueType>,
                  "operator[] requires a default-constructible mapped type");
    maybe_grow_for_insert();
    const size_type found = find_slot(key);
    if (found != npos()) return slots_[found]->value;
    const size_type pos = find_insert_position(key);
    construct_slot(pos, key, ValueType{});
    return slots_[pos]->value;
  }

  ValueType* find(const KeyType& key) {
    const size_type pos = find_slot(key);
    return pos == npos() ? nullptr : &slots_[pos]->value;
  }

  const ValueType* find(const KeyType& key) const {
    const size_type pos = find_slot(key);
    return pos == npos() ? nullptr : &slots_[pos]->value;
  }

  template <class LookupKey, class = std::enable_if_t<!std::is_same_v<std::decay_t<LookupKey>, KeyType> &&
                                                      is_transparent_lookup_v<LookupKey>>>
  ValueType* find(const LookupKey& key) {
    const size_type pos = find_slot(key);
    return pos == npos() ? nullptr : &slots_[pos]->value;
  }

  template <class LookupKey, class = std::enable_if_t<!std::is_same_v<std::decay_t<LookupKey>, KeyType> &&
                                                      is_transparent_lookup_v<LookupKey>>>
  const ValueType* find(const LookupKey& key) const {
    const size_type pos = find_slot(key);
    return pos == npos() ? nullptr : &slots_[pos]->value;
  }

  bool find(const KeyType& key, ValueType& out_value) const {
    const ValueType* value = find(key);
    if (value == nullptr) return false;
    out_value = *value;
    return true;
  }

  template <class LookupKey, class = std::enable_if_t<!std::is_same_v<std::decay_t<LookupKey>, KeyType> &&
                                                      is_transparent_lookup_v<LookupKey>>>
  bool find(const LookupKey& key, ValueType& out_value) const {
    const ValueType* value = find(key);
    if (value == nullptr) return false;
    out_value = *value;
    return true;
  }

  [[nodiscard]] bool contains(const KeyType& key) const {
    return find(key) != nullptr;
  }

  template <class LookupKey, class = std::enable_if_t<!std::is_same_v<std::decay_t<LookupKey>, KeyType> &&
                                                      is_transparent_lookup_v<LookupKey>>>
  [[nodiscard]] bool contains(const LookupKey& key) const {
    return find(key) != nullptr;
  }

  bool erase(const KeyType& key) {
    if (capacity_ == 0) return false;

    const size_type pos = find_slot(key);
    if (pos == npos()) return false;

    slots_[pos].reset();
    set_ctrl(pos, DELETED);
    ++deleted_;
    --size_;
    if (deleted_ > (capacity_ >> 2)) rehash(capacity_);
    return true;
  }

  bool saveBinary(std::string_view path) const {
    return save_binary(path);
  }

  bool loadBinary(std::string_view path) {
    return load_binary(path);
  }

  bool save_binary(std::string_view path) const {
    if constexpr (!is_binary_serializable()) {
      return false;
    } else {
      std::ofstream out(std::string(path), std::ios::binary | std::ios::trunc);
      if (!out) return false;

      if (!write_exact(out, kBinaryMagic)) return false;
      if (!write_exact(out, kBinaryVersion)) return false;

      const std::uint16_t reserved = 0;
      if (!write_exact(out, reserved)) return false;

      const std::uint64_t entry_count = static_cast<std::uint64_t>(size_);
      if (!write_exact(out, entry_count)) return false;

      for (size_type i = 0; i < capacity_; ++i) {
        if (!is_occupied(i)) continue;
        if (!write_exact(out, slots_[i]->key)) return false;
        if (!write_exact(out, slots_[i]->value)) return false;
      }

      return out.good();
    }
  }

  bool load_binary(std::string_view path) {
    if constexpr (!is_binary_serializable()) {
      return false;
    } else {
      std::ifstream in(std::string(path), std::ios::binary);
      if (!in) return false;

      std::uint32_t magic = 0;
      std::uint16_t version = 0;
      std::uint16_t reserved = 0;
      std::uint64_t entry_count = 0;

      if (!read_exact(in, magic)) return false;
      if (!read_exact(in, version)) return false;
      if (!read_exact(in, reserved)) return false;
      if (!read_exact(in, entry_count)) return false;

      if (magic != kBinaryMagic || version != kBinaryVersion || reserved != 0) return false;
      if (entry_count > static_cast<std::uint64_t>(std::numeric_limits<size_type>::max())) return false;

      PXHash tmp(0, hasher_, eq_);
      tmp.reserve(static_cast<size_type>(entry_count));

      for (std::uint64_t i = 0; i < entry_count; ++i) {
        KeyType key{};
        ValueType value{};
        if (!read_exact(in, key)) return false;
        if (!read_exact(in, value)) return false;
        tmp.insert_or_assign(std::move(key), std::move(value));
      }

      char trailing = 0;
      if (in.read(&trailing, 1)) return false;
      if (!in.eof()) return false;
      if (tmp.size() != static_cast<size_type>(entry_count)) return false;

      *this = std::move(tmp);
      return true;
    }
  }

private:
  using slot_type = Slot<KeyType, ValueType>;
  using optional_slot = std::optional<slot_type>;
  using optional_slot_allocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<optional_slot>;

  static constexpr size_type kNumer = 7;
  static constexpr size_type kDenom = 8;

  static constexpr size_type min_capacity() { return GROUP_SIZE * 2; }
  static constexpr size_type npos() { return std::numeric_limits<size_type>::max(); }

  static constexpr bool is_binary_serializable() {
    return std::is_trivially_copyable_v<KeyType> && std::is_trivially_copyable_v<ValueType>;
  }

  template <class LookupKey>
  static constexpr bool is_transparent_lookup_v =
      std::is_invocable_r_v<size_type, const Hash&, const LookupKey&> &&
      std::is_invocable_r_v<bool, const Eq&, const KeyType&, const LookupKey&>;

  static bool is_full_ctrl(std::uint8_t c) noexcept {
    return c != EMPTY && c != DELETED;
  }

  bool is_occupied(size_type pos) const noexcept {
    return pos < capacity_ && is_full_ctrl(ctrl_[pos]) && slots_[pos].has_value();
  }

  template <typename T>
  static bool write_exact(std::ostream& out, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "binary writes require trivially copyable types");
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return static_cast<bool>(out);
  }

  template <typename T>
  static bool read_exact(std::istream& in, T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "binary reads require trivially copyable types");
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
  }

  static unsigned ctz(std::uint32_t x) {
#if defined(_MSC_VER)
    unsigned long idx = 0;
    _BitScanForward(&idx, x);
    return static_cast<unsigned>(idx);
#else
    return static_cast<unsigned>(__builtin_ctz(x));
#endif
  }

  void set_ctrl(size_type pos, std::uint8_t v) noexcept {
    ctrl_[pos] = v;
    if (pos < GROUP_SIZE) ctrl_[pos + capacity_] = v;
  }

  void init_table(size_type cap) {
    capacity_ = cap;
    mask_ = cap - 1;
    size_ = 0;
    deleted_ = 0;

    ctrl_.assign(capacity_ + GROUP_SIZE, EMPTY);
    slots_.clear();
    slots_.resize(capacity_);
  }

  void rehash(size_type new_cap) {
    PXHash tmp(0, hasher_, eq_);
    tmp.init_table(new_cap);

    for (size_type i = 0; i < capacity_; ++i) {
      if (is_occupied(i)) {
        tmp.insert_or_assign_impl(std::move(slots_[i]->key), std::move(slots_[i]->value));
      }
    }

    *this = std::move(tmp);
  }

  void maybe_grow_for_insert() {
    if (capacity_ == 0) {
      init_table(min_capacity());
      return;
    }

    const size_type used = size_ + deleted_;
    if (used * kDenom >= capacity_ * kNumer) {
      if (deleted_ > (capacity_ >> 3)) {
        rehash(capacity_);
      } else {
        rehash(capacity_ * 2);
      }
    }
  }

  static std::uint32_t match_h2_mask(const std::uint8_t* base, std::uint8_t h2) {
#if defined(__AVX2__)
    const __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(base));
    const __m256i t = _mm256_set1_epi8(static_cast<char>(h2));
    const __m256i c = _mm256_cmpeq_epi8(v, t);
    return static_cast<std::uint32_t>(_mm256_movemask_epi8(c));
#elif defined(__SSE2__)
    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base));
    const __m128i t = _mm_set1_epi8(static_cast<char>(h2));
    const __m128i c = _mm_cmpeq_epi8(v, t);
    return static_cast<std::uint32_t>(_mm_movemask_epi8(c));
#else
    std::uint32_t mask = 0;
    for (size_type i = 0; i < GROUP_SIZE; ++i) {
      if (base[i] == h2) mask |= (std::uint32_t{1} << i);
    }
    return mask;
#endif
  }

  static std::uint32_t empty_mask(const std::uint8_t* base) {
#if defined(__AVX2__)
    const __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(base));
    const __m256i t = _mm256_set1_epi8(static_cast<char>(EMPTY));
    const __m256i c = _mm256_cmpeq_epi8(v, t);
    return static_cast<std::uint32_t>(_mm256_movemask_epi8(c));
#elif defined(__SSE2__)
    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base));
    const __m128i t = _mm_set1_epi8(static_cast<char>(EMPTY));
    const __m128i c = _mm_cmpeq_epi8(v, t);
    return static_cast<std::uint32_t>(_mm_movemask_epi8(c));
#else
    std::uint32_t mask = 0;
    for (size_type i = 0; i < GROUP_SIZE; ++i) {
      if (base[i] == EMPTY) mask |= (std::uint32_t{1} << i);
    }
    return mask;
#endif
  }

  size_type wrapped_pos(size_type idx, unsigned bit) const noexcept {
    return (idx + static_cast<size_type>(bit)) & mask_;
  }

  template <class LookupKey>
  size_type find_slot(const LookupKey& key) const {
    if (capacity_ == 0) return npos();

    const size_type h = mix_hash(hasher_(key));
    const std::uint8_t h2 = h2_from_hash(h);
    size_type idx = h & mask_;

    for (;;) {
      const std::uint8_t* base = ctrl_.data() + idx;
      std::uint32_t m = match_h2_mask(base, h2);
      while (m) {
        const unsigned bit = ctz(m);
        const size_type pos = wrapped_pos(idx, bit);
        if (is_occupied(pos) && eq_(slots_[pos]->key, key)) return pos;
        m &= (m - 1);
      }

      if (empty_mask(base)) return npos();
      idx = (idx + GROUP_SIZE) & mask_;
    }
  }

  template <class LookupKey>
  size_type find_insert_position(const LookupKey& key) const {
    const size_type h = mix_hash(hasher_(key));
    size_type idx = h & mask_;

    for (;;) {
      const std::uint8_t* base = ctrl_.data() + idx;
      std::uint32_t avail = empty_mask(base) | match_h2_mask(base, DELETED);
      if (avail) return wrapped_pos(idx, ctz(avail));
      idx = (idx + GROUP_SIZE) & mask_;
    }
  }

  template <class KArg, class VArg>
  void construct_slot(size_type pos, KArg&& key, VArg&& value) {
    const size_type h = mix_hash(hasher_(key));
    const std::uint8_t h2 = h2_from_hash(h);
    const bool was_deleted = ctrl_[pos] == DELETED;

    slots_[pos].emplace(std::forward<KArg>(key), std::forward<VArg>(value));
    set_ctrl(pos, h2);
    if (was_deleted) --deleted_;
    ++size_;
  }

  template <class KArg, class VArg>
  bool emplace_new(KArg&& key, VArg&& value) {
    const size_type pos = find_insert_position(key);
    construct_slot(pos, std::forward<KArg>(key), std::forward<VArg>(value));
    return true;
  }

  template <class KArg, class VArg>
  bool insert_or_assign_impl(KArg&& key, VArg&& value) {
    const size_type found = find_slot(key);
    if (found != npos()) {
      slots_[found]->value = std::forward<VArg>(value);
      return false;
    }

    return emplace_new(std::forward<KArg>(key), std::forward<VArg>(value));
  }

  Hash hasher_{};
  Eq eq_{};
  Allocator allocator_{};
  size_type capacity_{0};
  size_type mask_{0};
  size_type size_{0};
  size_type deleted_{0};
  std::vector<std::uint8_t> ctrl_;
  std::vector<optional_slot, optional_slot_allocator> slots_{allocator_};
};

}  // namespace pxhash

#endif
