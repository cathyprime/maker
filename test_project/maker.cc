#include "../maker.hh"
#include <filesystem>

const std::string out_dir = "out/";
const std::string build_dir = "build/";
const std::string compiler = "g++ ";

using namespace maker;

int main(int argc, char **argv)
{
    GO_REBUILD_YOURSELF("g++", argc, argv);

    Maker mk;

    std::string files[3] = {
        "file1", "file2", "file3"
    };

    std::vector<std::string> main_deps;
    for (const auto &it: files) {
        std::string name = build_dir + it;
        mk += Rule(name + ".o", { it + ".cc", it + ".hh", build_dir })
            .with_cmd(from_string(compiler + "-c " + it + ".cc -o " + name + ".o"));
        main_deps.push_back(name + ".o");
    }

    auto main_deps_o = main_deps;
    main_deps.push_back(out_dir);

    std::stringstream main_cmd;
    std::string main_name = out_dir + "main";
    main_cmd << compiler + "-o " + main_name + " main.cc";
    for (const auto &it: main_deps_o) {
        main_cmd << ' ' << it;
    }

    auto build_rule = Rule(build_dir, { build_dir });
    build_rule.cmd = from_string("mkdir -p " + build_dir);
    build_rule.phony = false;
    mk += build_rule,
        Rule(out_dir, { out_dir }).with_cmd(from_string("mkdir -p " + out_dir)),
        Rule(main_name, { main_deps })
              .with_cmd(from_string(main_cmd.str()));

    if (argc <= 0) {
        mk(main_name);
    } else {
        mk(shift(argc, argv));
    }
}
