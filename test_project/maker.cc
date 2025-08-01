#define MAKER_PROJECT
#define MAKER_IMPLEMENTATION
#include "../maker.hh"

int main(int argc, char **argv)
{
    GO_REBUILD_YOURSELF(argc, argv);

    maker::Project project;
    project.flags = "-O3 -march=native -DNDEBUG";
    project();
}
