#ifndef LIBARY
#include "value.hpp"
#include <windows.h>

HMODULE load_native_libary(std::string path) {
	HMODULE hdll = LoadLibraryA(path.c_str());
	return hdll;
}

#endif 