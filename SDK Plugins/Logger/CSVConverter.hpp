#include <fstream>
#include <filesystem>
#include <variant>
#include <cstdint>
#include <set> 
#include <algorithm>
#include <iterator>
#include <utility>

class CSVConverter {
private:
    std::filesystem::path filePath;
    std::ofstream asciifile;
    std::ofstream binaryfile;

public:
    CSVConverter() {};
    CSVConverter(std::filesystem::path fileDirectory, const std::vector<std::pair<std::string, std::string>> asciiData, const std::vector<std::pair<std::string, std::string>> binaryData)
        : filePath(fileDirectory), asciifile("ASCII_Measurements.csv"), binaryfile("Binary_Measurements.csv") {
        // Loop #1: ASCII Measurements
        std::set<std::string> CSVHeaders_ASCII;
        std::transform(asciiData.begin(), asciiData.end(), std::inserter(CSVHeaders_ASCII, CSVHeaders_ASCII.begin()), [](const auto& pair) { return pair.first; });

        // Write CSV headers
        for (const std::string& key : CSVHeaders_ASCII) {
            asciifile << key << ",";
        }
        asciifile << "\n";

        // values under each header
        for (int i = 0; i < asciiData.size(); i += CSVHeaders_ASCII.size()) {
            for (int j = 0; j < CSVHeaders_ASCII.size(); j++) {
                asciifile << asciiData[i+j].second << ",";
            }
            asciifile << "\n";
        }

        asciifile.close();

        // Loop #2: Binary Measurements
        std::set<std::string> CSVHeaders_BINARY;
        std::transform(binaryData.begin(), binaryData.end(), std::inserter(CSVHeaders_BINARY, CSVHeaders_BINARY.begin()), [](const auto& pair) { return pair.first; });
        
        // CSV headers
        for (const std::string& key : CSVHeaders_BINARY) {
            binaryfile << key << ",";
        }
        binaryfile << "\n";

        // values under each header
        for (int i = 0; i < binaryData.size(); i += CSVHeaders_BINARY.size()) {
            for (int j = 0; j < CSVHeaders_BINARY.size(); j++) {
                binaryfile << binaryData[i+j].second << ",";
            }
            binaryfile << "\n";
        }

        binaryfile.close();
    }
};