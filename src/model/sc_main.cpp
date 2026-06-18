#include <systemc.h>
#include <iostream>

int sc_main(int argc, char* argv[])
{
    if (argc > 1) {
        std::cout << "Program binary: " << argv[1] << std::endl;
    } else {
        std::cout << "No program binary provided." << std::endl;
    }

    return 0;
}
