#include "debug.hpp"

using namespace DEBUG_NS;

int main() {
    DBG_printer();
    for(int i = 0; i < 1000000000; i++) {}
    DBG_printer();
}