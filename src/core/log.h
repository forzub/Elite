#pragma once
#include <iostream>

#ifdef ENABLE_LOG
    #define LOG(x) do { std::cout << x << std::endl; } while(0)
#else
    #define LOG(x) do {} while(0)
#endif
