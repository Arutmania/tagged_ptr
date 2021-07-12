#pragma once

#include <cstdint>
#include <exception>
#include <type_traits>

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
} // namespace tagged
