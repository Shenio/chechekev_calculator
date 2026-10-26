#include <cmath>
#include <stdexcept>

extern "C" __declspec(dllexport) const char* GetFunctionName() {
    return "ln";
}

extern "C" __declspec(dllexport) int GetArity() {
    return 1;
}

extern "C" __declspec(dllexport) double Execute(const double* args, int argc) {
    if (!args || argc != 1) {
        throw std::invalid_argument("ln expects exactly 1 argument");
    }
    if (args[0] <= 0.0) {
        throw std::domain_error("ln of non-positive value");
    }
    return std::log(args[0]);
}
