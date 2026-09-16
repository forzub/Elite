#if defined(_WIN32)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001UL;
__declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 0x00000001UL;
}
#endif
