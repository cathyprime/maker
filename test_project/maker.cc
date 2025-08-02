#define MAKER_PROJECT
#define MAKER_IMPLEMENTATION
#include "../maker.hh"

constexpr std::string_view optimization = "-O3";
constexpr std::string_view arch = "-march=native";
constexpr std::string_view no_debug_symbols = "-DNDEBUG";

int main(int argc, char **argv)
{
    GO_REBUILD_YOURSELF(argc, argv);

    maker::Project project;

    maker::utils::flag_concat libs { "-l" };
    libs->push_back("m");

    project.cflags =
        maker::utils::concat(optimization, arch, no_debug_symbols);
    project.ldflags = libs;

    project();
}
