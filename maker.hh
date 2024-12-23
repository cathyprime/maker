#include <array>
#include <vector>
#include <string>
#include <future>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#ifndef MAKER_FLAGS
#define MAKER_FLAGS "-Wfatal-errors -Oz -fno-rtti -fno-exceptions -Wall -Wextra -march=native -s -Werror -Wpedantic"
#endif

#define INF(mess) std::cerr << "[INFO]: " << mess << '\n'
#define WRN(mess) std::cerr << "[WARN]: " << mess << '\n'
#define ERR(mess) std::cerr << "[ERROR]: " << mess << '\n'
#define CMD(mess) std::cerr << "[CMD]: " << mess << '\n'
#define shift(argc, argv) (argc--, *argv++)

namespace maker {

using cmd_t = std::function<int(void)>;

struct Cmd {
    cmd_t func;
    std::string description;
};

template<typename T, typename U>
using isU = std::enable_if<std::is_same<std::decay_t<T>, U>::value>;

template<typename T>
using isString = isU<T, std::string>;

template<typename T>
using isCmd = isU<T, Cmd>;

template<typename S, typename = isString<S>>
constexpr Cmd from_string(S &&str)
{
    Cmd cmd;
    cmd.func = [=](){
        return std::system(str.c_str());
    };
    cmd.description = std::forward<S>(str);
    return cmd;
}

struct Rule {
    std::optional<Cmd> cmd;
    std::vector<std::string> deps;
    std::string target;
    bool phony;

    Rule()
        : cmd()
        , deps()
        , target()
        , phony(false)
    {}
    template<typename S, typename = isString<S>>
    Rule(S &&t)
        : cmd()
        , deps()
        , target(std::forward<S>(t))
        , phony(false)
    {}

    template<typename F, typename = isCmd<F>>
    Rule &with_cmd(F &&c)
    {
        cmd = std::make_optional(std::forward<F>(c));
        return *this;
    }

    bool should_rebuild()
    {
        if (!std::filesystem::exists(target))
            return true;
        if (std::filesystem::is_directory(target))
            return false;

        auto target_mod_time = std::filesystem::last_write_time(target);
        for (const auto &dep: deps) {
            if (!std::filesystem::exists(dep))
                return true;

            if (std::filesystem::is_directory(dep))
                continue;

            auto dependency_mod_time = std::filesystem::last_write_time(dep);
            if (dependency_mod_time > target_mod_time)
                return true;
        }
        return false;
    }

    template<typename T, typename = isString<T>>
    Rule(T &&t, std::vector<std::string>&& v)
        : cmd()
        , deps(std::move(v))
        , target(std::forward<T>(t))
        , phony(false)
    {}

    template<typename T, typename = isString<T>>
    Rule(T &&t, std::vector<std::string> &vec)
        : cmd()
        , deps(vec)
        , target(std::forward<T>(t))
        , phony(false)
    {}
};

namespace {

struct Tree_Node {
    Rule *rule;
    std::vector<size_t> deps;
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

    Tree_Node &operator[](size_t idx)
    {
        return nodes[idx];
    }
    size_t add_node()
    {
        nodes.emplace_back();
        return nodes.size() - 1;
    }
    size_t add_node(Rule *rule)
    {
        nodes.emplace_back(rule);
        return nodes.size() - 1;
    }
};

struct Job {
    std::vector<Cmd> todo;

    Job() = default;
    Job(Cmd &s)
        : todo()
    {
        todo.push_back(s);
    }

    size_t size()
    {
        return todo.size();
    }

    template<typename F, typename = isCmd<F>>
    Job &operator+=(F &&cmd)
    {
        todo.push_back(std::forward<F>(cmd));
        return *this;
    }

    operator bool()
    {
        return !todo.empty();
    }

    bool operator()(size_t &idx, size_t total)
    {
        std::vector<std::future<int>> futures;

        for (auto &it: todo) {
            std::string cmd;
            cmd += "[" + std::to_string(++idx);
            cmd += "/" + std::to_string(total);
            cmd += "]: " + it.description + '\n';
            std::cout << cmd;

            futures.push_back(std::async(std::launch::async, it.func));
        }

        for (auto &it: futures) {
            if (it.get() != 0) {
                return false;
            }
        }

        return true;
    }
};

}

struct Maker {
  // private:
    std::unordered_map<std::string, Rule> rules;

    void build_tree(Tree &tree, size_t node_idx)
    {
        Tree_Node *node = &tree.nodes[node_idx];
        if (tree.nodes[node_idx].visited) return;
        node->visited = true;

        for (const auto &dep: node->rule->deps) {
            if (rules.find(dep) == rules.end()) continue;

            Rule* rule = &rules[dep];

            size_t new_node_idx = tree.add_node();
            Tree_Node &new_node = tree[new_node_idx];
            tree.nodes[node_idx].deps.push_back(new_node_idx);
            new_node.rule = rule;
            if (!(tree.nodes[node_idx].rule->target == dep))
                build_tree(tree, tree.nodes.size() - 1);
        }
    }

    bool recursive_rebuild(Tree &tree, size_t current_idx, std::vector<Job> &jobs)
    {
        Job job;
        bool any_true = false;
        static std::unordered_set<std::string> seen_commands;
        Tree_Node &current_node = tree[current_idx];

        for (const auto &dep: current_node.deps) {
            if (recursive_rebuild(tree, dep, jobs)) {
                any_true = true;
                if (!tree[dep].rule->cmd)
                    continue;
                if (seen_commands.find(tree[dep].rule->cmd->description) == seen_commands.end()) {
                    seen_commands.insert(tree[dep].rule->cmd->description);
                    if (tree[dep].rule->cmd)
                    job += *(tree[dep].rule->cmd);
                }
            }
        }

        if (job) jobs.push_back(job);
        return any_true || current_node.rule->phony || current_node.rule->should_rebuild();
    }

    void clean()
    {
        bool run_once = false;
        for (const auto &it: rules) {
            if (!it.second.phony) {
                if (!std::filesystem::exists(it.second.target))
                    continue;
                if (std::filesystem::is_directory(it.second.target))
                    continue;
                run_once = true;
                std::string cmd = "rm -r ";
                cmd += it.first;
                CMD(cmd);
                std::system(cmd.c_str());
            }
        }
        if (!run_once) {
            INF("Already spotless clean...");
        }
    }

  public:
    Maker()
        : rules()
    {}

    template<typename R, typename = isU<R, Rule>>
    Maker &operator+=(R &&rule)
    {
        rules[rule.target] = std::forward<R>(rule);
        return *this;
    }

    Maker &operator,(Rule &rule)
    {
        rules[rule.target] = rule;
        return *this;
    }

    void operator()(const std::string &target)
    {
        if (target == "clean") {
            clean();
            return;
        }

        Tree tree;
        if (rules.find(target) == rules.end()) {
            ERR("rule " << target << " not found");
            return;
        }
        Rule *rule = &rules[target];
        (void)tree.add_node(rule);
        build_tree(tree, 0);
        std::vector<Job> jobs;

        if (recursive_rebuild(tree, 0, jobs)) {
            if (tree.nodes[0].rule->cmd)
                jobs.push_back(Job(*tree.nodes[0].rule->cmd));
        } else {
            INF("nothing to be done for '" << target << "'");
            return;
        }

        size_t total = 0;
        for (auto &it: jobs) {
            total += it.size();
        }

        size_t count = 0;
        for (auto &it: jobs) {
            it(count, total);
        }
    }
};

}

#define __MAKER_BUILD(compiler, argc, argv, execpath, filename)       \
std::string cmd;                                                      \
cmd += std::string(compiler) + " -o ";                                \
cmd += "'" + execpath.string() + "' ";                                \
cmd += filename + ' ';                                                \
cmd += std::string(MAKER_FLAGS) + ' ';                                \
cmd += "-D_MAKER_OPTIMIZED";                                          \
int result = std::system(cmd.c_str());                                \
if(result == 0) {                                                     \
    INF("compiled successfully! Restarting...");                      \
    execv(execpath.string().c_str(), --argv);                         \
} else {                                                              \
    ERR("Compilation failed, fix errors and try again!");             \
    return result;                                                    \
}

#ifdef _MAKER_OPTIMIZED
#include <unistd.h>
#define GO_REBUILD_YOURSELF(compiler, argc, argv)                     \
    do {                                                              \
        std::string filename = __FILE__;                              \
        std::filesystem::path execpath = shift(argc, argv);           \
        auto exec_time = std::filesystem::last_write_time(execpath);  \
        auto file_time = std::filesystem::last_write_time(filename);  \
        if (file_time > exec_time) {                                  \
            INF("New recipe detected, rebuilding...");                \
            __MAKER_BUILD(compiler, argc, argv, execpath, filename)   \
        }                                                             \
    } while(0);
#else
#include <unistd.h>
#define GO_REBUILD_YOURSELF(compiler, argc, argv)                     \
    do {                                                              \
        std::string filename = __FILE__;                              \
        std::filesystem::path execpath = shift(argc, argv);           \
        INF("First run detected, optimizing...");                     \
        __MAKER_BUILD(compiler, argc, argv, execpath, filename)       \
    } while(0);
#endif
