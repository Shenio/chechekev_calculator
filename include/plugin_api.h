#pragma once

// Common plugin API for the calculator.
// Each plugin DLL must export the following C functions:
//   const char* GetFunctionName();
//   int         GetArity();
//   double      Execute(const double* args, int argc);
//
// The name returned by GetFunctionName() is the name used in expressions.
// The DLL file name is intentionally NOT used to determine the function name.

extern "C" {

// Returns the name of the function provided by this plugin.
// Must not return nullptr or an empty string.
const char* GetFunctionName();

// Returns the number of arguments the function expects (>= 1).
int GetArity();

// Executes the function. May throw C++ exceptions, which the host
// catches at the DLL boundary.
double Execute(const double* args, int argc);

} // extern "C"
