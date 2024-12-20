#include <iostream>

#include "file1.hh"
#include "file2.hh"
#include "file3.hh"

int main()
{
    std::cout << "Hello, world!" << std::endl;
    std::cout << "Hello from main function!" << std::endl;

    func1();
    func2();
    func3();
}
