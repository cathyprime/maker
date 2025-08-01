#include <vector>
#include <future>
#include <string>
#include <cstdlib>
#include <functional>
#include <filesystem>
#include <type_traits>

#ifdef MAKER_PROJECT
#include <array>
#include <sstream>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <string_view>
#include <unordered_map>
#endif

#ifndef MAKER_LOG
#include <iostream>
#define MAKER_LOG(str) do {                     \
    using namespace std::string_literals;       \
    std::cout << ("[EXEC] :: "s + str + "\n"s); \
} while (0);
#endif

#ifndef MAKER_LOG_INFO
#include <iostream>
#define MAKER_LOG_INFO(str) do {                \
    using namespace std::string_literals;       \
    std::cout << ("[INFO] :: "s + str + "\n"s); \
} while (0);
#endif

#ifndef MAKER_FLAGS
#define MAKER_FLAGS "-std=c++17 -Wfatal-errors -Oz -fno-rtti -fno-exceptions -Wall -Wextra -march=native -s -Werror -Wpedantic"
#endif

namespace maker {
namespace utils {

template<typename... Strs>
std::string concat(Strs&&... strings)
{
    std::string result = "";
    static_assert((... && (std::is_convertible_v<Strs, std::string_view>)),
    "expected string like type");

    bool first = true;
    auto f = [&](const std::string_view &s) -> void {
        if (s.empty()) return;
        result += (first ? "" : " ");
        result += s;
        first = false;
    };

    (..., f(std::string_view(std::forward<Strs>(strings))));

    return result;
}

std::vector<std::string> split_args(const std::string &cmd);
std::vector<std::string> split_args(const char *cmd);
using Job = std::function<int(void)>;

template<typename T>
constexpr bool is_job = std::is_convertible_v<T, Job>;

const char *get_compiler();
const std::string header_path = __FILE__;

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

template<bool Capture_Output, typename It>
int __execute_impl(It begin, It end, std::string *output = nullptr)
{
    using Str = typename std::iterator_traits<It>::value_type;
    static_assert(
        std::is_same_v<Str, char*>       ||
        std::is_same_v<Str, const char*> ||
        std::is_same_v<Str, std::string> ||
        std::is_same_v<Str, std::string_view>,
        "expected string like type"
    );

    if (begin == end)
        return -1;

    int pipefd[2]{ -1, -1 };
    if constexpr (Capture_Output) {
        if (pipe(pipefd) == -1) return false;
    }

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);

    if constexpr (Capture_Output) {
        posix_spawn_file_actions_addclose(&fa, pipefd[0]);
        posix_spawn_file_actions_adddup2(&fa, pipefd[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&fa, pipefd[1]);
    }

    pid_t pid;

    std::vector<char*> argv;
    for (; begin != end; ++begin) {
        auto sv = static_cast<std::string_view>(*begin);
        argv.push_back(const_cast<char*>(sv.data()));
    }
    argv.push_back(nullptr);

    if (posix_spawnp(&pid, argv[0], &fa, nullptr, argv.data(), environ) != 0) {
        posix_spawn_file_actions_destroy(&fa);
        if constexpr (Capture_Output) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        return 1;
    }

    posix_spawn_file_actions_destroy(&fa);

    if constexpr (Capture_Output) {
        close(pipefd[1]);

        char buffer[1024];
        ssize_t count;
        while ((count = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            output->append(buffer, count);
        }
        close(pipefd[0]);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

};

template<bool Capture_Output = false, typename Container>
inline int execute(const Container &cmd, std::string *output = nullptr)
{
    return __execute_impl<Capture_Output>(std::begin(cmd), std::end(cmd), output);
}

template<typename It>
void print_cmd(It begin, It end)
{
    using Str = typename std::iterator_traits<It>::value_type;

    static_assert(
        std::is_same_v<Str, char*>       ||
        std::is_same_v<Str, const char*> ||
        std::is_same_v<Str, std::string> ||
        std::is_same_v<Str, std::string_view>,
        "expected string like type"
    );

    if (begin == end) return;

    std::string str = *begin;
    ++begin;
    for (; begin != end; ++begin) {
        str += " ";
        str += *begin;
    }

    MAKER_LOG(str);
}

}; // namespace utils

utils::Job from(std::string cmd);
bool should_rebuild(const std::filesystem::path &target, const std::filesystem::path &source);
void go_rebuild_yourself(std::filesystem::path &&source, std::filesystem::path &&executable, int argc, char **argv);

class Parallel {
    using Job = utils::Job;
    std::vector<Job> jobs;

public:
    template<typename... Funcs>
    explicit Parallel(Funcs&&... functions)
    {
        static_assert((utils::is_job<Funcs> && ...), "All functions need to be int(void)");
        jobs = { Job(std::forward<Funcs>(functions))... };
    }

    template<typename T>
    Parallel &operator+=(T &&func)
    {
        static_assert((utils::is_job<T>), "must be int(void)");
        jobs.push_back(std::forward<T>(func));
        return *this;
    }

    template<typename T>
    Parallel &push(T &&func)
    {
        *this += func;
        return *this;
    }

    template<typename T>
    Parallel &operator,(T &&func)
    {
        *this += std::forward<T>(func);
        return *this;
    }

    int operator()(int max_threads = 0)
    {
        size_t idx = 0;
        size_t result = 0;

        size_t batch_size = max_threads != 0
            ? max_threads
            : jobs.size();

        std::vector<std::future<int>> futures;
        futures.reserve(batch_size);

        while (idx < jobs.size()) {
            if (result != 0) break;

            size_t batch_idx = 0;
            for (; batch_idx < batch_size && idx + batch_idx < jobs.size(); ++batch_idx) {
                futures.push_back(std::async(std::launch::async, jobs[idx + batch_idx]));
            }

            idx += batch_idx;
            for (auto &it : futures) {
                int job_result = it.get();
                if (job_result != 0 && result == 0) {
                    result = job_result;
                }
            }
            futures.clear();
        }

        return result;
    }
};

#ifdef MAKER_PROJECT
struct Project
{
    std::filesystem::path build_directory = "out";
    std::filesystem::path source_directory = ".";
    std::string executable_name = "main";
    std::string compiler = utils::get_compiler();
    std::string flags = "";
    int max_threads = 0;
    bool force = false;

    using filter_t = std::function<bool(const std::filesystem::directory_entry&)>;
    filter_t filter_sources = [](const std::filesystem::directory_entry &entry) -> bool {
        return entry.path().stem() != "maker"
        && entry.path().extension() == ".cc";
    };

    int operator()();
private:
    using dep_t = std::vector<std::filesystem::path>;
    using dep_map_t = std::unordered_map<std::filesystem::path, dep_t>;

    int create_executable();
    dep_map_t get_dependency_map();
    int update_o_files();
};
#endif // MAKER_PROJECT

}; // namespace maker


#ifndef shift
#define shift(xs, size) (((size)--), *(xs)++)
#endif

#ifdef GO_REBUILD_YOURSELF
#undef GO_REBUILD_YOURSELF
#endif

#define GO_REBUILD_YOURSELF(argc, argv) maker::go_rebuild_yourself(__FILE__, shift(argv, argc), argc, argv);

#ifdef MAKER_IMPLEMENTATION
extern char **environ;

const char *maker::utils::get_compiler()
{
    const char *compiler = std::getenv("CXX");
    return compiler ? compiler : "c++";
}

#ifndef MAKER_NTH_RUN
#include <iostream>
#endif

void maker::go_rebuild_yourself(std::filesystem::path &&source, std::filesystem::path &&executable, int argc, char **argv)
{
    #ifdef MAKER_NTH_RUN
    if (maker::should_rebuild(executable, source) || maker::should_rebuild(executable, utils::header_path)) {
    #endif
        #ifndef MAKER_NTH_RUN
        MAKER_LOG_INFO("first run, optimizing the executable");
        #endif
        #ifdef MAKER_NTH_RUN
        MAKER_LOG_INFO("change detected, recompiling");
        #endif
        std::string compile_exec = maker::utils::get_compiler();
        compile_exec += (" -D MAKER_NTH_RUN " MAKER_FLAGS " -o ") + executable.lexically_normal().string() + " " + source.string();

        int result = maker::from(compile_exec)();
        if (result != 0) std::exit(result);

        std::string cmd = executable.string();
        while (argc) {
            cmd.push_back(' ');
            cmd += shift(argv, argc);
        }

        MAKER_LOG_INFO("restarting");
        std::exit(maker::from(cmd)());
    #ifdef MAKER_NTH_RUN
    }
    #endif
}

maker::utils::Job maker::from(std::string cmd)
{
    std::vector cmd_arr = maker::utils::split_args(cmd);
    return [cmd_arr = std::move(cmd_arr), cmd = std::move(cmd)]() mutable -> int {
        MAKER_LOG(cmd);
        return maker::utils::execute(cmd_arr);
    };
}

bool maker::should_rebuild(const std::filesystem::path &target, const std::filesystem::path &source)
{
    namespace fs = std::filesystem;

    if (!fs::exists(target))
        return true;

    if (!fs::exists(source))
        return false;

    auto target_time = fs::last_write_time(target);
    auto source_time = fs::last_write_time(source);

    return source_time > target_time;
}

std::vector<std::string> maker::utils::split_args(const std::string &str)
{
    std::vector<std::string> words;
    std::stringstream ss(str);
    std::string buf;

    while (ss >> buf)
        words.push_back(buf);

    return words;
}

std::vector<std::string> maker::utils::split_args(const char *str)
{
    return maker::utils::split_args(std::string(str));
}

#ifdef MAKER_PROJECT
int maker::Project::create_executable()
{
    namespace fs = std::filesystem;

    std::vector<fs::path> filenames;
    for (const auto &entry : fs::directory_iterator(this->source_directory)) {
        if (!this->filter_sources(entry)) {
            continue;
        }

        filenames.push_back(this->build_directory / entry.path().lexically_normal().replace_extension(".o"));
    }
    fs::path executable = this->build_directory / this->executable_name;
    bool exists = fs::exists(executable);
    bool any_ood = false;
    if (exists) {
        for (const auto &dep : filenames) {
            any_ood = maker::should_rebuild(executable, dep);
            if (any_ood) break;
        }
    }

    if (!exists || any_ood) {
        std::vector<std::string> cmd {
            this->compiler,
            "-o", this->build_directory / this->executable_name
        };

        std::transform(
            filenames.begin(), filenames.end(),
            std::back_inserter(cmd),
            [](const fs::path &p) { return p.string(); }
        );

        maker::utils::print_cmd(cmd.begin(), cmd.end());
        return maker::utils::execute(cmd);
    }

    return 0;
}

maker::Project::dep_map_t maker::Project::get_dependency_map()
{
    std::vector<std::string> cmd = {
        maker::utils::get_compiler(),
        "-MM"
    };

    for (auto &entry : std::filesystem::directory_iterator(this->source_directory)) {
        if (!this->filter_sources(entry)) {
            continue;
        }
        cmd.push_back(entry.path().string());
    }

    std::string output;
    int status = maker::utils::execute<true>(cmd, &output);
    assert(!status && "failed to execute");

    dep_map_t dependencies;

    std::istringstream output_s { output };
    for (std::string line; std::getline(output_s, line);) {
        std::istringstream line_stream { line };
        std::string key;
        line_stream >> key;
        key.erase(key.size() - 1);

        dependencies[this->build_directory / key] = {};
        std::string word;
        while (line_stream >> word)
            dependencies[this->build_directory / key].push_back(word);
    }

    return dependencies;
}

int maker::Project::update_o_files()
{
    namespace fs = std::filesystem;

    fs::create_directory(this->build_directory);
    auto deps = get_dependency_map();

    maker::Parallel parallel;
    for (const auto &entry : fs::directory_iterator(this->source_directory)) {
        if (!this->filter_sources(entry)) continue;

        fs::path copy = entry.path().lexically_normal();
        fs::path outfile = entry.path().lexically_normal();
        copy.replace_extension(".o");

        fs::path o_file = this->build_directory / copy;
        bool exists = fs::exists(o_file);

        bool any_ood = false;
        if (exists) {
            for (auto &dep : deps[o_file.string()]) {
                any_ood = maker::should_rebuild(o_file, dep);
                if (any_ood) break;
            }
        }

        if (!exists || any_ood || this->force) {
            parallel += maker::from(
                maker::utils::concat(
                    this->compiler,
                    "-c", this->flags,
                    "-o", o_file.string(),
                    outfile.string()));
        }
    }

    return parallel(this->max_threads);
}

int maker::Project::operator()()
{
    int status = update_o_files();

    if (status != 0)
        return status;

    return create_executable();
}

#endif // MAKER_PROJECT
#endif // MAKER_IMPLEMENTATION
