#include <fstream>
#include <filesystem>
#include <vector> 

class TextConverter {
private:
    std::filesystem::path filePath;
    std::ofstream asciifile;
    std::ofstream binaryfile; 

public:
    TextConverter() {};
    TextConverter(std::filesystem::path fileDirectory, const std::vector<std::string> asciiData, const std::vector<std::string> binaryData)
        : filePath(fileDirectory), asciifile("ASCII_Measurements.txt"), binaryfile("Binary_Measurements.txt") {
        // Loop #1: ASCII Measurements
        for (const auto& line : asciiData) {
            if (line.size() <= 32) { // weeds out unnecessary content not already filtered in previous functions
                continue;
            }
            else {
                size_t pos = line.find("$V"); // Further removal
                if (pos != std::string::npos) {
                    asciifile << line.substr(pos) << std::endl;
                }
                else {
                    asciifile << line << std::endl;
                }
            }
        }

        asciifile.close();

        // Loop #2: Binary Measurements 
        for (const auto& line : binaryData) {
            binaryfile << line << std::endl;
        }
        binaryfile.close();
    }
};