#include <iostream>
#include <fstream>
#include <cmath>
#include <iomanip>

#define LUT_SIZE 512
#define PI 3.14159265

int main() {
    std::ofstream file("luts.h");
    file << "#pragma once\n";
    file << "#include <pgmspace.h>\n";
    file << "#define LUT_SIZE " << LUT_SIZE << "\n\n";

    // Sin table
    file << "const float sinLUT[LUT_SIZE] PROGMEM = {\n";
    for (int i = 0; i < LUT_SIZE; i++) {
        float val = sin(2 * PI * i / LUT_SIZE);
        file << std::fixed << std::setprecision(8) << val << "f";
        if (i != LUT_SIZE - 1) file << ",";
        if (i % 8 == 7) file << "\n";
    }
    file << "\n};\n\n";

    // Cos table
    file << "const float cosLUT[LUT_SIZE] PROGMEM = {\n";
    for (int i = 0; i < LUT_SIZE; i++) {
        float val = cos(2 * PI * i / LUT_SIZE);
        file << std::fixed << std::setprecision(8) << val << "f";
        if (i != LUT_SIZE - 1) file << ",";
        if (i % 8 == 7) file << "\n";
    }
    file << "\n};\n";

    file.close();
    std::cout << "luts.h generated with PROGMEM!\n";
    return 0;
}
