#ifndef TAGGED_H
#define TAGGED_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>

#if __cplusplus >= 201703L
#   define TP_DEPRECATED [[deprecated]]
#   define TP_NODISCARD  [[nodiscard]]
#else
#   define TP_DEPRECATED
#   define TP_NODISCARD
#endif

namespace tp {

namespace detail {
template <unsigned long long Value>
struct byte_req {
    enum {
        value
            = Value <= (1 << 8)  ? 1
            : Value <= (1 << 16) ? 2
            :                      4
    };
};

template <int>  struct byte_wide;
template <>     struct byte_wide<1> { using type = int8_t;  };
template <>     struct byte_wide<2> { using type = int16_t; };
template <>     struct byte_wide<4> { using type = int32_t; };

template <std::size_t Max>
struct normalize_ignore {
    using type = typename byte_wide<byte_req<Max>::value>::type;

    type value;

    constexpr normalize_ignore(type value_ = {}) noexcept
        : value(value_)
    {}
};

template <std::size_t Max>
struct normalize_clamp {
    using type = typename byte_wide<byte_req<Max>::value>::type;

    type value;

    // Max is always a bunch of ones left-padded with zeros
    constexpr normalize_clamp(type value_ = {}) noexcept
        : value(value_ & static_cast<type>(Max))
    {}
};

template <std::size_t Max>
struct normalize_assert {
    using type = typename byte_wide<byte_req<Max>::value>::type;

    type value;

    constexpr normalize_assert(type value_ = {}) noexcept
        : value(value_) {
        assert(value <= Max &&
               "[tagged_ptr] tag value outside of alignment range");
    }
};

template <std::size_t Max>
struct normalize_except {
    using type = typename byte_wide<byte_req<Max>::value>::type;

    struct exception : std::exception {
        virtual char const* what() const noexcept
        { return "[tagged_ptr]: tag value outside of alignment range"; }
    };

    type value;

    constexpr normalize_except(type value_ = {})
        : value(value_ <= static_cast<type>(Max) ? value_ : throw exception {})
    {}
};

}

template <
    class T,
    template <std::size_t> class Normalize = detail::normalize_clamp,
    std::size_t Align = alignof(T)
>
class tagged_ptr {
public:
    enum { BIT_MASK = Align - 1 };

    using tag_normalize = Normalize<BIT_MASK>;
    using tag_type      = typename tag_normalize::type;
    using pointer       = T*;

    explicit tagged_ptr(T* ptr, tag_normalize tag = {}) noexcept
        : ptr_ (reinterpret_cast<std::intptr_t>(ptr) | tag.value)
    {}

    tagged_ptr(std::nullptr_t = nullptr) noexcept
        : ptr_ { reinterpret_cast<std::intptr_t>(pointer {}) }
    {}

    tagged_ptr(tagged_ptr const&) noexcept = default;

    template <class U, template <std::size_t> class N, std::size_t A,
        class = typename std::enable_if<
            std::is_convertible<
                typename tagged_ptr<U, N, A>::pointer,
                pointer
            >::value && A <= Align
        >::type
    >
    tagged_ptr(tagged_ptr<U, N, A> other)
        : tagged_ptr { static_cast<T*>(other.get()), other.tag() }
    {}

    tagged_ptr& operator =(tagged_ptr const&) noexcept = default;

    template <std::size_t I = 0>
    TP_NODISCARD auto get() const noexcept
    { return get_detail(get_tag<I> {}); }

    TP_NODISCARD T& operator *() const noexcept
    { return *get(); }

    TP_NODISCARD pointer operator ->() const noexcept
    { return get(); }

    TP_NODISCARD explicit operator bool() const noexcept
    { return get() != nullptr; }

    TP_NODISCARD tag_type tag() const noexcept
    { return ptr_ & BIT_MASK; }

    tagged_ptr& operator |=(tag_normalize rhs) noexcept
    { ptr_ |= rhs.value; return *this; }

    tagged_ptr& operator &=(tag_normalize rhs) noexcept
    { ptr_ = ptr_ & ~BIT_MASK | ptr_ & rhs.value; return *this; }

    TP_NODISCARD tag_type operator ~() const noexcept
    { return ~tag(); }

private:
    std::intptr_t ptr_;

    template <std::size_t>
    struct get_tag {};

    pointer get_detail(get_tag<0>) const noexcept
    { return reinterpret_cast<T*>(ptr_ & ~BIT_MASK); }

    tag_type get_detail(get_tag<1>) const noexcept
    { return tag(); }
};

template <class T, template <std::size_t> class N, std::size_t A,
          class U, template <std::size_t> class M, std::size_t B>
inline bool operator ==(tagged_ptr<T, N, A> lhs, tagged_ptr<U, M, B> rhs)
    noexcept {
    return lhs.get() == rhs.get();
}

template <class T, template <std::size_t> class N, std::size_t A>
inline bool operator ==(tagged_ptr<T, N, A> lhs, std::nullptr_t) noexcept {
    return !static_cast<bool>(lhs);
}

template <class T, template <std::size_t> class N, std::size_t A>
inline bool operator ==(std::nullptr_t, tagged_ptr<T, N, A> rhs) noexcept {
    return !static_cast<bool>(rhs);
}

template <class T, template <std::size_t> class N, std::size_t A,
          class U, template <std::size_t> class M, std::size_t B>
inline bool operator !=(tagged_ptr<T, N, A> lhs, tagged_ptr<U, M, B> rhs)
    noexcept {
    return lhs.get() != rhs.get();
}

template <typename T, template <std::size_t> class N, std::size_t A>
inline bool operator !=(tagged_ptr<T, N, A> lhs, std::nullptr_t) noexcept {
    return static_cast<bool>(lhs);
}

template <typename T, template <std::size_t> class N, std::size_t A>
inline bool operator !=(std::nullptr_t, tagged_ptr<T, N, A> rhs) noexcept {
    return static_cast<bool>(rhs);
}

template <class T, template <std::size_t> class N, std::size_t A,
          class U, template <std::size_t> class M, std::size_t B>
inline bool operator <(tagged_ptr<T, N, A> lhs, tagged_ptr<U, M, B> rhs)
    noexcept {
    return std::less<typename std::common_type<
        typename decltype(lhs)::pointer, typename decltype(rhs)::pointer
    >::type>()(lhs.get(), rhs.get());
}

template <class T, template <std::size_t> class N, std::size_t A>
inline bool operator <(tagged_ptr<T, N, A> lhs, std::nullptr_t) noexcept {
    return std::less<typename decltype(lhs)::pointer>()(lhs.get(), nullptr);
}

template <class T, template <std::size_t> class N, std::size_t A>
inline bool operator <(std::nullptr_t, tagged_ptr<T, N, A> rhs) noexcept {
    return std::less<typename decltype(rhs)::pointer>()(nullptr, rhs.get());
}

template <class T, template <std::size_t> class N, std::size_t A,
          class U, template <std::size_t> class M, std::size_t B>
inline bool operator <=(tagged_ptr<T, N, A> lhs, tagged_ptr<U, M, B> rhs)
    noexcept {
    return !(rhs < lhs);
}

template <class T, template <std::size_t> class N, std::size_t A>
inline bool operator <=(tagged_ptr<T, N, A> lhs, std::nullptr_t) noexcept {
    return !(nullptr < lhs);
}

template <class T, template <std::size_t> class N, std::size_t A>
inline bool operator <=(std::nullptr_t, tagged_ptr<T, N, A> rhs) noexcept {
    return !(rhs < nullptr);
}

template <class T, template <std::size_t> class N, std::size_t A,
          class U, template <std::size_t> class M, std::size_t B>
inline bool operator >(tagged_ptr<T, N, A> lhs, tagged_ptr<U, M, B> rhs)
    noexcept {
    return rhs < lhs;
}

template <class T, template <std::size_t> class N, std::size_t A>
inline bool operator >(tagged_ptr<T, N, A> lhs, std::nullptr_t) noexcept {
    return nullptr < lhs;
}

template <class T, template <std::size_t> class N, std::size_t A>
inline bool operator >(std::nullptr_t, tagged_ptr<T, N, A> rhs) noexcept {
    return rhs < nullptr;
}

template <class T, template <std::size_t> class N, std::size_t A,
          class U, template <std::size_t> class M, std::size_t B>
inline bool operator >=(tagged_ptr<T, N, A> lhs, tagged_ptr<U, M, B> rhs)
    noexcept {
    return !(lhs < rhs);
}

template <class T, template <std::size_t> class N, std::size_t A>
inline bool operator >=(tagged_ptr<T, N, A> lhs, std::nullptr_t) noexcept {
    return !(lhs < nullptr);
}

template <class T, template <std::size_t> class N, std::size_t A>
inline bool operator >=(std::nullptr_t, tagged_ptr<T, N, A> rhs) noexcept {
    return !(nullptr < rhs);
}

}

namespace std {

template <class T, template <size_t> class N, size_t A>
struct pointer_traits<tp::tagged_ptr<T, N, A>> {
private:
    template <class U>
    struct rebind_detail {
        static_assert(alignof(U) <= A,
                      "tried to rebind to a type with smaller alignment");
        using type = tp::tagged_ptr<U, N, A>;
    };

public:
    using pointer         = tp::tagged_ptr<T, N, A>;
    using element_type    = T;
    using difference_type = ptrdiff_t;

    template <class U>
    using rebind = typename rebind_detail<U>::type;

    static pointer pointer_to(element_type& r) noexcept {
        return pointer { std::addressof(r) };
    }

    static element_type* to_address(pointer p) noexcept {
        return p.get();
    }
};

template <class T, template <size_t> class N, size_t A>
struct tuple_size<tp::tagged_ptr<T, N, A>> : integral_constant<size_t, 2> {};

template <class T, template <size_t> class N, size_t A>
struct tuple_element<0, tp::tagged_ptr<T, N, A>> {
    using type = typename tp::tagged_ptr<T, N, A>::pointer;
};

template <class T, template <size_t> class N, size_t A>
struct tuple_element<1, tp::tagged_ptr<T, N, A>> {
    using type = typename tp::tagged_ptr<T, N, A>::tag_type;
};

template <class T, template <size_t> class N, size_t A>
struct hash<tp::tagged_ptr<T, N, A>> {
    using argument_type TP_DEPRECATED = tp::tagged_ptr<T, N, A>;
    using result_type   TP_DEPRECATED = size_t;

    size_t operator ()(tp::tagged_ptr<T, N, A> p) const noexcept
    { return hash<T*>()(p.get()); }
};

}
#endif
