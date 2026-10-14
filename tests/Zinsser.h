#pragma once

#include <string>
#include <iostream>

namespace Zinsser {

template <typename Func>
inline void describe(const std::string& suiteName, Func&& fn) {
    std::cout << "\n--- William Zinsser Suite: " << suiteName << " ---\n";
    fn();
    std::cout << "PASS: William Zinsser Suite [" << suiteName << "] passed!\n";
}

template <typename Func>
inline void it(const std::string& sentence, Func&& fn) {
    std::cout << "  [it] " << sentence << "\n";
    fn();
}

} // namespace Zinsser
