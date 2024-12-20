#include <array>
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>

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

template<std::size_t size, typename CharT = char, typename Traits = std::char_traits<CharT>>
class Cmd_Builder {
    using StringType = std::basic_string<CharT, Traits>;
    std::array<StringType, size> m_cmd;
    std::size_t m_idx;

  public:
    constexpr Cmd_Builder() = default;

    template <typename... Args, typename = std::enable_if_t<(std::is_convertible_v<Args, StringType> && ...)>>
    constexpr Cmd_Builder(Args... args) noexcept
        : m_cmd()
        , m_idx(0)
    {
        static_assert(sizeof...(args) <= size, "Too many arguments for Cmd_Builder");
        StringType strs[] = {args...};
        for (auto &s : strs) {
            m_cmd[m_idx++] = std::move(s);
        }
    }
    template<typename... Args, typename = std::enable_if_t<(std::is_convertible_v<Args, StringType> && ...)>>
    constexpr Cmd_Builder &push(Args... args) noexcept
    {
        static_assert(sizeof...(args) <= size, "Too many arguments for Cmd_Builder");
        StringType strs[] = {args...};
        for (auto &s : strs) {
            if (m_idx < size) {
                m_cmd[m_idx++] = std::move(s);
            }
        }
        return *this;
    }
    constexpr Cmd_Builder &push(StringType &&s) noexcept
    {
        if (m_idx < size) {
            m_cmd[m_idx++] = std::move(s);
        }
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

template<std::size_t size>
struct Rule {
  private:
    std::size_t idx;
  public:
    std::array<std::string, size> deps;
    std::string target;
    std::string cmd;
    bool phony;

    template<>
    constexpr Rule(const Rule<size> &other)
        : idx()
        , deps(other.deps)
        , target(other.target)
        , cmd(other.cmd)
        , phony(other.phony)
    {}
    constexpr Rule(std::string &&t)
        : idx()
        , deps()
        , target(t)
        , cmd()
        , phony(false)
    {}
    template<typename... Args>
    constexpr Rule(std::string &&t, Args... args)
        : idx()
        , deps()
        , target(t)
        , cmd()
        , phony(false)
    {
        static_assert(sizeof...(args) <= size, "Too many arguments for Rule");
        std::string strs[] = {args...};
        for (auto &s : strs) {
            if (idx < size) {
                deps[idx++] = std::move(s);
            }
        }
    }
};

namespace {

struct Tree_Node {
    std::string target;
    std::vector<std::string> deps;
};

}

/*
 * check dates
 * rebuild
 */

class Maker {
    std::vector<Tree_Node> nodes;
    std::unordered_map<std::string, Rule> rules;

  public:
    constexpr Maker()
        : rules()
    {}
};

}
