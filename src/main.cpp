#include <iostream>
#include "core/Application.h"
#include "core/ConsoleOutput.h"

#include <clocale>


int main()
{

    try
    {
        std::setlocale(LC_ALL, "");
        core::disableRuntimeStdoutNoise();
        Application app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}