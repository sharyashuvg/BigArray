# BigArray
header-only small library for growing array that is saved in a file so you can save Gb's of data while still preserving array syntax for convenience.
compiled it in vs22 c++20 and with g++12.2.0 built by msys2 with "g++ -g -o test.exe test.cpp -std=c++20"
*faster in msvc because it uses nolock versions to red\write to file.
