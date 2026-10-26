#include <cmath>
#include <stdexcept>

extern "C" __declspec(dllexport) const char* GetFunctionName() {
    return "sin";
}

extern "C" __declspec(dllexport) int GetArity() {
    return 1;
}

extern "C" __declspec(dllexport) double Execute(const double* args, int argc) {
    if (!args || argc != 1) {
        throw std::invalid_argument("sin expects exactly 1 argument");
    }
    return std::sin(args[0]);
}
