#include "../maker.hh"
#include <sstream>

const std::string out_dir = "out/";
const std::string build_dir = "build/";
const std::string compiler = "g++ ";

using namespace maker;

#define shift(argc, argv) (argc--, *argv++)

int main(int argc, char **argv)
{
    Maker mk;

    std::string files[3] = {
        "file1", "file2", "file3"
    };

    std::vector<std::string> main_deps;
    for (const auto &it: files) {
        std::string name = build_dir + it;
        mk += Rule(name + ".o", { it + ".cc", build_dir })
            .with_cmd(compiler + "-c " + it + ".cc -o " + name + ".o");
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
    build_rule.cmd = "mkdir -p " + build_dir;
    build_rule.phony = false;
    mk += build_rule,
        Rule(out_dir, { out_dir }).with_cmd("mkdir -p " + out_dir),
        Rule(main_name, { main_deps })
              .with_cmd(main_cmd.str());

    if (argc <= 1) {
        mk(main_name);
    } else {
        (void)shift(argc, argv);
        mk(shift(argc, argv));
    }
}
