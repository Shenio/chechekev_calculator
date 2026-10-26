#include <cmath>
#include <stdexcept>

extern "C" __declspec(dllexport) const char* GetFunctionName() {
    return "deg";
}

extern "C" __declspec(dllexport) int GetArity() {
    return 1;
}

extern "C" __declspec(dllexport) double Execute(const double* args, int argc) {
    if (!args || argc != 1) {
        throw std::invalid_argument("deg expects exactly 1 argument");
    }
    const double pi = 3.14159265358979323846264338327950288;
    return args[0] * pi / 180.0;
}
