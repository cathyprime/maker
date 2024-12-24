#include <array>
#include <vector>
#include <string>
#include <future>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#ifndef MAKER_FLAGS
#define MAKER_FLAGS "-Oz -fno-rtti -fno-exceptions -Wall -Wextra -march=native -s -Werror -Wpedantic"
#endif

#define INF(mess) std::cerr << "[INFO]: " << mess << '\n'
#define WRN(mess) std::cerr << "[WARN]: " << mess << '\n'
#define ERR(mess) std::cerr << "[ERROR]: " << mess << '\n'
#define CMD(mess) std::cerr << "[CMD]: " << mess << '\n'
#define shift(argc, argv) (argc--, *argv++)

namespace maker {

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
    Rule(std::string &&t)
        : deps()
        , target(t)
        , cmd()
        , phony(false)
    {}

    Rule &with_cmd(std::string &&c)
    {
        cmd = std::move(c);
        return *this;
    }

    Rule &with_cmd(const std::string &c)
    {
        cmd = c;
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

    template<typename T,
             typename = std::enable_if<std::is_same<std::decay_t<T>, std::string>::value>>
    Rule(T &&t, std::vector<std::string>&& v)
        : deps(std::move(v))
        , target(std::forward<T>(t))
        , cmd()
        , phony(false)
    {}

    template<typename T,
             typename = std::enable_if<std::is_same<std::decay_t<T>, std::string>::value>>
    Rule(T &&t, std::vector<std::string> &vec)
        : deps(vec)
        , target(std::forward<T>(t))
        , cmd()
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
    std::vector<std::string> todo;

    Job() = default;
    Job(std::string &s)
        : todo()
    {
        todo.push_back(s);
    }

    size_t size()
    {
        return todo.size();
    }

    Job &operator+=(std::string &cmd)
    {
        todo.push_back(cmd);
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
            std::stringstream ss;
            ss << "[" << ++idx;
            ss << "/" << total;
            ss << "]: " << it << '\n';
            std::cout << ss.str();

            futures.push_back(std::async(std::launch::async, [&]() {
                return std::system(it.c_str());
            }));
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
                if (tree[dep].rule->cmd.empty())
                    continue;
                if (seen_commands.find(tree[dep].rule->cmd) == seen_commands.end()) {
                    seen_commands.insert(tree[dep].rule->cmd);
                    job += tree[dep].rule->cmd;
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

    Maker &operator+=(Rule &rule)
    {
        rules[rule.target] = rule;
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
            jobs.push_back(Job(tree.nodes[0].rule->cmd));
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
INF("New recipe detected, rebuilding...");                            \
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
            __MAKER_BUILD(compiler, argc, argv, execpath, filename)   \
        }                                                             \
    } while(0);
#else
#include <unistd.h>
#define GO_REBUILD_YOURSELF(compiler, argc, argv)                     \
    do {                                                              \
        std::string filename = __FILE__;                              \
        std::filesystem::path execpath = shift(argc, argv);           \
        __MAKER_BUILD(compiler, argc, argv, execpath, filename)       \
    } while(0);
#endif
