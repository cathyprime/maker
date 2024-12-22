#include "maker.hh"

int main() {
    maker::Maker mk;

    mk.rules["main.o"] = maker::Rule::with_deps("main.o", "main.cpp");
    mk.rules["main.o"].cmd = "g++ -c main.cpp -o main.o";

    mk.rules["utils.o"] = maker::Rule::with_deps("utils.o", "utils.cpp");
    mk.rules["utils.o"].cmd = "g++ -c utils.cpp -o utils.o";

    mk.rules["app"] = maker::Rule::with_deps("app", "main.o", "utils.o");
    mk.rules["app"].cmd = "g++ main.o utils.o -o app";

    for (const auto &kv: mk.rules) {
        std::cout << "key: " << kv.first << std::endl;
        std::cout << "target: " << kv.second.target << std::endl;
        for (const auto &x: kv.second.deps) {
            std::cout << "  dep: " << x << std::endl;
        }
        // std::cout << kv.second << std::endl;
    }

    // Testing the `Maker`
    mk("app");

    return 0;
}
