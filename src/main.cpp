#include <iostream>
#include "Client.hpp"

int main(int argc, char** argv)
{
    CELV::Client client;

    if (argc == 1)
        client.Run();
    else if (argc == 2)
        client.Run(argv[1]);
    else
        std::cerr << "Too many arguments!" << std::endl;

    return 0;
}