#include "../maker.hh"

using namespace maker;

int main()
{
    Maker mk;

    mk.rules["file1.o"] = Rule::with_deps("file1.o", "file1.cc");
    mk.rules["file1.o"].cmd = "g++ -c file1.cc -o file1.o";
    mk.rules["file2.o"] = Rule::with_deps("file2.o", "file2.cc");
    mk.rules["file2.o"].cmd = "g++ -c file2.cc -o file2.o";
    mk.rules["file3.o"] = Rule::with_deps("file3.o", "file3.cc");
    mk.rules["file3.o"].cmd = "g++ -c file3.cc -o file3.o";

    mk.rules["main"] = Rule::with_deps("main", "file1.o", "file2.o", "file3.o");
    mk.rules["main"].cmd = "g++ -o main main.cc file1.o file2.o file3.o";

    mk("main");
}

// int main()
// {
//     Maker mk;
//
//     Rule file1_rule = Rule::with_deps("file1.o", "file1.cc");
//     file1_rule.cmd = "g++ -c file1.cc -o file1.o";
//
//     Rule file2_rule = Rule::with_deps("file2.o", "file2.cc");
//     file2_rule.cmd = "g++ -c file2.cc -o file2.o";
//
//     Rule file3_rule = Rule::with_deps("file3.o", "file3.cc");
//     file3_rule.cmd = "g++ -c file3.cc -o file3.o";
//
//     Rule main_rule = Rule::with_deps("main", "file1.o", "file2.o", "file3.o");
//     main_rule.cmd = "g++ -o main main.cc file1.o file2.o file3.o";
//
//     mk += main, file1_rule, file2_rule, file3_rule;
//
//     mk("main");
// }
