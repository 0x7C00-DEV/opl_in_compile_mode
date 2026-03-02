//
// Created by User on 2026/3/2.
//

#ifndef COPL_PATHPROC_HPP
#define COPL_PATHPROC_HPP
#include <algorithm>
#include "iostream"
#include <string>
#include <cctype>
#include <string>

std::string get_file_ext(const std::string& filepath) {
	size_t lastSlash = filepath.find_last_of("/\\");
	size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
	size_t lastDot = filepath.rfind('.');
	if (lastDot != std::string::npos &&
	    lastDot > start &&
	    lastDot < filepath.length() - 1) {
		return filepath.substr(lastDot + 1);
	}
	return "";
}
std::string to_lower(const std::string& str) {
	std::string result = str;
	std::transform(result.begin(), result.end(), result.begin(),
	               [](unsigned char ch) { return std::tolower(ch); });
	return result;
}

bool end_with(std::string path, std::string ext_name) {
	return to_lower(get_file_ext(path)) == ext_name;
}


#endif //COPL_PATHPROC_HPP
