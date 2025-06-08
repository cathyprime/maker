#include <vector>
#include <future>
#include <string>
#include <cstdlib>
#include <iostream>
#include <functional>
#include <filesystem>
#include <type_traits>

#ifndef MAKER_FLAGS
#define MAKER_FLAGS "-std=c++17 -Wfatal-errors -Oz -fno-rtti -fno-exceptions -Wall -Wextra -march=native -s -Werror -Wpedantic"
#endif

namespace maker {
namespace utils {

using Job = std::function<int(void)>;

template<typename T>
constexpr bool is_job = std::is_convertible_v<T, Job>;

const char *get_compiler();
const std::string header_path = __FILE__;

}; // namespace utils

utils::Job from(std::string cmd);
bool should_rebuild(const std::filesystem::path &target, const std::filesystem::path &source);
void go_rebuild_yourself();

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

    int operator()() const
    {
        std::vector<std::future<int>> futures;
        for (auto &it : jobs) {
            futures.push_back(std::async(std::launch::async, it));
        }

        int result = 0;
        for (auto &it : futures) {
            int job_result = it.get();
            if (job_result != 0 && result == 0) {
                result = job_result;
            }
        }

        return result;
    }
};

}; // namespace maker


#ifndef shift
#define shift(xs, size) (((size)--), *(xs)++)
#endif

#ifdef GO_REBUILD_YOURSELF
#undef GO_REBUILD_YOURSELF
#endif

#define GO_REBUILD_YOURSELF(argc, argv) maker::go_rebuild_yourself(__FILE__, shift(argv, argc), &argc, &argv);

#ifdef MAKER_IMPLEMENTATION
namespace maker {

namespace utils {

const char *get_compiler()
{
    const char *compiler = std::getenv("CXX");
    return compiler ? compiler : "c++";
}

};

void go_rebuild_yourself(std::filesystem::path &&source, std::filesystem::path &&executable, int *argc, char ***argv)
{
    if (should_rebuild(executable, source) || should_rebuild(executable, utils::header_path)) {
        std::string cmd = utils::get_compiler();
        cmd += (" " MAKER_FLAGS " -o ") + executable.string() + " " + source.string();
        std::cout << "[CMD]: " << cmd << std::endl;
        int result = std::system(cmd.c_str());
        if (result != 0) std::exit(result);
        cmd.clear();
        cmd += executable;
        while (*argc) {
            cmd.push_back(' ');
            cmd += shift(*argv, *argc);
        }
        std::cout << "[CMD]: " << cmd << std::endl;
        std::exit(std::system(cmd.c_str()));
    }
}

utils::Job from(std::string cmd)
{
    return [cmd = std::move(cmd)]() mutable {
        return std::system(cmd.data());
    };
}

bool should_rebuild(const std::filesystem::path &target, const std::filesystem::path &source)
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

} // namespace maker
#endif
