#define MAKER_PROJECT
#define MAKER_IMPLEMENTATION
#include "../maker.hh"

constexpr static const char *optimization = "-O3";
constexpr static const char *arch = "-march=native";
constexpr static const char *no_debug_symbols = "-DNDEBUG";

int main(int argc, char **argv)
{
    GO_REBUILD_YOURSELF(argc, argv);

    maker::Project project;

    project();
}
