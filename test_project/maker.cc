#include "../maker.hh"

using namespace maker;

int main()
{
    Maker mk;

    mk +=
        Rule::with_deps("file1.o", "file1.cc").with_cmd("g++ -c file1.cc -o file1.o"),
        Rule::with_deps("file2.o", "file2.cc").with_cmd("g++ -c file2.cc -o file2.o"),
        Rule::with_deps("file3.o", "file3.cc").with_cmd("g++ -c file3.cc -o file3.o"),
        Rule::with_deps("main", {"file1.o", "file2.o", "file3.o"})
              .with_cmd("g++ -o main main.cc file1.o file2.o file3.o");

    mk("main");
}
