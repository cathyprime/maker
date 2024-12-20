#include <array>
#include <string>
#include <sstream>
#include <iostream>

#ifndef MAKER_FLAGS
#define MAKER_FLAGS "-Oz -fno-rtti -fno-exceptions"
#endif

/*
 * TODO
 * compilation rules
 * constant definitions
 * progress feedback
 * auto clean rule
 * default rule
 * -fno-rtti
 * -fno-exceptions
 * -Oz
 */

namespace maker {
class Rule;

template<int size, typename CharT = char, typename Traits = std::char_traits<CharT>>
class Cmd_Builder {
    using StringType = std::basic_string<CharT, Traits>;
    std::array<StringType, size> m_cmd;
    std::size_t m_idx;

  public:
    constexpr Cmd_Builder() = default;

    template <typename... Args>
    constexpr Cmd_Builder(Args... args)
        : m_cmd()
        , m_idx(0)
    {
        std::string strs[] = {args...};
        for (auto &s : strs) {
            m_cmd[m_idx++] = std::move(s);
        }
    }
    template<typename... Args>
    constexpr Cmd_Builder &push(Args... args) noexcept
    {
        std::string strs[] = {args...};
        for (auto &s : strs) {
            m_cmd[m_idx++] = std::move(s);
        }
        return *this;
    }
    constexpr Cmd_Builder &push(StringType &&s) noexcept
    {
        m_cmd[m_idx++] = std::move(s);
        return *this;
    }
    [[nodiscard]] constexpr std::string build() const noexcept
    {
        std::string result = m_cmd[0];

        for (auto it = m_cmd.begin() + 1; it != m_cmd.end(); ++it) {
            result += ' ' + *it;
        }
        return result;
    }
};

class Rule {
    std::vector<std::string> deps;
    std::string target;
    std::string cmd;
    bool phony;

    Recipe() = default;
    bool is_up_to_date()
    {
        for (const auto &dep : dependencies)
        if (!dep.is_up_to_date())
            return false;

        return true;
    }
    void rebuild()
    {
        if (is_up_to_date()) {
            // TODO: run cmd
        }
    }
};

}
