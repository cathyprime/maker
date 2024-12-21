#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <string_view>
#include <unordered_map>

#ifndef MAKER_FLAGS
#define MAKER_FLAGS "-Oz -fno-rtti -fno-exceptions"
#endif

#define INF(mess) std::cerr << "[INFO]: " << mess << '\n'
#define WRN(mess) std::cerr << "[WARN]: " << mess << '\n'
#define ERR(mess) std::cerr << "[ERROR]: " << mess << '\n'

/*
 * TODO
 * progress feedback
 * auto clean rule
 * default rule
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

struct Rule {
    std::vector<std::string> deps;
    std::string target;
    std::string cmd;
    bool phony;

    Rule()
        : deps()
        , target()
        , cmd()
        , phony(false)
    {}
    Rule(const Rule &other)
        : deps(other.deps)
        , target(other.target)
        , cmd(other.cmd)
        , phony(other.phony)
    {}
    Rule(std::string &&t)
        : deps()
        , target(t)
        , cmd()
        , phony(false)
    {}
    template<typename... Args>
    Rule(std::string &&t, Args... args)
        : deps()
        , target(t)
        , cmd()
        , phony(false)
    {
        std::string strs[] = {args...};
        for (auto &s : strs) {
            deps.push_back(std::move(s));
        }
    }
    bool is_target_older()
    {
        auto target_file = std::filesystem::path(target);
        for (const auto &dep: deps) {
            auto dep_file = std::filesystem::path(dep);
            if (std::filesystem::last_write_time(dep_file) > std::filesystem::last_write_time(target))
                return true;
        }
        return false;
    }
};

namespace {

struct Tree_Node {
    Rule *rule;
    std::vector<Tree_Node*> deps;
    bool visited;

    Tree_Node()
        : rule(nullptr)
        , deps()
        , visited(false)
    {}

    Tree_Node(Rule* rule)
        : rule(rule)
        , deps()
        , visited(false)
    {}
};

struct Tree {
    std::vector<Tree_Node> nodes;

    Tree_Node *add_node()
    {
        nodes.emplace_back();
        return &nodes.back();
    }
    Tree_Node *add_node(Rule *rule)
    {
        nodes.emplace_back(rule);
        return &nodes.back();
    }
};

void print_nodes(const maker::Tree_Node *node, size_t depth = 0) {
    if (!node || !node->rule) {
        std::cout << std::string(depth * 2, ' ') << "Node without rule\n";
        return;
    }

    // Print the current node
    std::cout << std::string(depth * 2, ' ') << "Target: " << node->rule->target 
              << ", Phony: " << (node->rule->phony ? "true" : "false")
              << ", Visited: " << (node->visited ? "true" : "false") << '\n';

    // Print dependencies
    for (const auto &dep : node->deps) {
        print_nodes(dep, depth + 1);
    }
}

}

class Maker {
    public:
    std::unordered_map<std::string, Rule> rules;

    void recursive_build(Tree *tree, size_t node_idx)
    {
        Tree_Node *node = &tree->nodes[node_idx];
        if (tree->nodes[node_idx].visited) return;
        node->visited = true;

        for (const auto &dep: node->rule->deps) {
            Tree_Node *new_node = tree->add_node();
            Tree_Node *node = &tree->nodes[node_idx];

            if (rules.find(dep) == rules.end()) {
                ERR("rule " << dep << " not found");
                return;
            }
            new_node->rule = &rules[dep];
            node->deps.push_back(new_node);
            recursive_build(tree, tree->nodes.size() - 1);
        }
    }

  public:
    Maker()
        : rules()
    {}

    void operator()(const std::string &target)
    {
        Tree tree;
        if (rules.find(target) == rules.end()) {
            ERR("rule " << target << " not found");
            return;
        }
        Rule *rule = &rules[target];
        (void)tree.add_node(rule);
        recursive_build(&tree, tree.nodes.size() - 1);
        print_nodes(&tree.nodes[0]);
    }
};

}
