#pragma once

#include <cstdint>
#include <exception>
#include <type_traits>

#define TAGGED_CXX11 (__cplusplus >= 201103L)
#define TAGGED_CXX14 (__cplusplus >= 201402L)
#define TAGGED_CXX17 (__cplusplus >= 201703L)

#if TAGGED_CXX17
#   define TAGGED_DEPRECATED [[deprecated]]
#   define TAGGED_NODISCARD  [[nodiscard]]
#else
#   define TAGGED_DEPRECATED
#   define TAGGED_NODISCARD
#endif

namespace tagged {

template<unsigned long long Width>
class tag {
public:
    static constexpr auto max  = 1 << Width;
    static constexpr auto mask = Width - 1;

    intptr_t value;

    constexpr tag()
        : tag(0, std::true_type {})
    {}

    constexpr tag(intptr_t value)
        : tag(value, std::true_type {})
    {}

    static constexpr auto unchecked(intptr_t value) {
        return tag { value, std::false_type {} };
    }

    struct exception : std::exception {
        char const* what() const noexcept override {
            return "[tagged::tag] value out of width range";
        }
    };

private:
    constexpr tag(intptr_t value, std::true_type)
        : value(value)
    {
        if (value >= max)
            throw exception {};
    }

    constexpr tag(intptr_t value, std::false_type)
        : value(value)
    {}
};

namespace detail {

template<unsigned long long>
struct sink {};

template<class T, class>
struct aligner {
    static constexpr auto value = 0ULL;
};

template<class T>
struct aligner<T, sink<!alignof(T)>> {
    static constexpr auto value = alignof(T);
};

template<class T>
static constexpr auto align = aligner<T, sink<0>>::value;

} // namespace detail

template<class T, unsigned long long Align = detail::align<T>>
class ptr {
public:
    using pointer         = T*;
    using element_type    = T;
    using difference_type = ptrdiff_t;
    using tag_type        = tag<Align>;

    template<class U>
    using is_ptr_convertible_from = std::is_convertible<
        typename U::pointer,
        pointer
    >;

    template<class U>
    using is_narrower_eq = std::integral_constant<
        bool,
        tag_type::max <= U::tag_type::max
    >;

    template<class U>
    using rebind = ptr<U, Align>;

    static ptr pointer_to(element_type& r) noexcept {
        return ptr { std::addressof(r) };
    }

    static element_type* to_address(ptr p) noexcept {
        return p.get();
    }

    explicit ptr(pointer p, tag_type t = {}) noexcept
        : value(int_cast(p) | t.value)
    {}

    ptr(std::nullptr_t = nullptr) noexcept
        : value(int_cast({}))
    {}

    ptr(ptr const&) noexcept = default;

    template<class U, unsigned long long A,
        class = typename std::enable_if<
            is_ptr_convertible_from<ptr<U, A>>::value &&
            is_narrower_eq<ptr<U, A>>::value
        >::type
    >
    ptr(ptr<U, A> other) noexcept
        : ptr(other.get(), other.tag())
    {}

    static constexpr auto from(intptr_t p, tag_type t = {}) noexcept {
        return ptr { p, t };
    }

    static constexpr auto unchecked(intptr_t p, intptr_t t) {
        return ptr { p, tag_type::unchecked(t) };
    }

    template<unsigned long long A>
    static constexpr auto unchecked(ptr<T, A> p) {
        return ptr { p.value };
    }

    ptr& operator =(ptr const&) noexcept = default;

    template<size_t I = 0>
    TAGGED_NODISCARD auto get() const noexcept
    { return get(get_tag<I> {}); }

    TAGGED_NODISCARD element_type& operator *() const noexcept
    { return *get(); }

    TAGGED_NODISCARD pointer operator ->() const noexcept
    { return get(); }

    TAGGED_NODISCARD explicit operator bool() const noexcept
    { return get() != nullptr; }

    TAGGED_NODISCARD intptr_t tag() const noexcept
    { return value & tag_type::mask; }

    ptr& operator |=(tag_type rhs) noexcept {
        value |= rhs.value;
        return *this;
    }

    ptr& operator &=(tag_type rhs) noexcept {
        value = (value & ~tag_type::mask) | (value & rhs.value);
        return *this;
    }

    TAGGED_NODISCARD intptr_t operator ~() const noexcept
    { return ~tag(); }

    TAGGED_NODISCARD bool fits(tag_type t) const noexcept
    { return t.value <= tag_type::max; }

private:
    static auto ptr_cast(intptr_t i) noexcept {
        return reinterpret_cast<pointer>(i);
    }

    static auto int_cast(pointer p) noexcept {
        return reinterpret_cast<intptr_t>(p);
    }

    explicit constexpr ptr(intptr_t p, tag_type t = {})
        : value(p | t.value)
    {}

    template<size_t>
    struct get_tag {};

    pointer get(get_tag<0>) const noexcept
    { return ptr_cast(value & ~tag_type::mask); }

    intptr_t get(get_tag<1>) const noexcept
    { return tag(); }

    intptr_t value;
};
} // namespace tagged
