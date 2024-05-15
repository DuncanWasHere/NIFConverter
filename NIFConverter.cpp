#include <iostream>
#include <fstream>
#include <filesystem>
#include "nlohmann/json.hpp"

namespace fs = std::filesystem; 

const std::string INPUT_FOLDER_PATH = "Input";
const std::string OUTPUT_FOLDER_PATH = "Output";
int Files_Processed;

void processJsonFile(const std::string& inputFileName);
void replaceJsonSubstring(nlohmann::ordered_json& jsonObject, std::string substr, std::string replacement);

int main() {


    for (const auto& entry : fs::recursive_directory_iterator(INPUT_FOLDER_PATH)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            processJsonFile(entry.path().string());
        }
    }   

    std::cout << std::endl << std::endl << "#######################################################" << std::endl << "Operation completed successfully. Files Processed: " << Files_Processed << std::endl << "#######################################################" << std::endl;    system("pause");

    return 0;

}

void processJsonFile(const std::string& inputFileName) {

    // Open input Json
    std::ifstream inputJson(inputFileName);
    if (!inputJson.is_open()) {
        std::cerr << "Error: Unable to open input file!" << inputFileName << std::endl;
        return;
    }
    else
        std::cout << "Processing file: " << inputFileName << std::endl;

    nlohmann::ordered_json jsonData;
    inputJson >> jsonData;

    for (auto it = jsonData.begin(); it != jsonData.end(); it++) {
        if (it.key().find(" NiTriStripsData") != std::string::npos || it.key().find(" NiTriShapeData") != std::string::npos) {
            replaceJsonSubstring(it.value(), "UV_1", "Has_UV");
        }
    }


    for (auto it = jsonData.begin(); it != jsonData.end(); it++) {
        if (it.key().find(" bhk") != std::string::npos) {
            replaceJsonSubstring(it.value(), "OB_HAV_MAT", "FO_HAV_MAT");
            replaceJsonSubstring(it.value(), "OL_", "FOL_");
        }
    }


    // Update header info
    if (jsonData.contains("NiHeader")) {
        auto& niHeader = jsonData["NiHeader"];
        niHeader["Version"] = "20.2.0.7";
        niHeader["Magic"] = "Gamebryo File Format, Version 20.2.0.7";
        niHeader["User Version"] = "11";
        niHeader["User Version 2"] = "34";

        // Some files use a really old version of NifSkope without this property
        if (!niHeader.contains("Endian Type"))
            niHeader["Endian Type"] = "ENDIAN_LITTLE";
    }

    // Write Json data to new file
    fs::path outputFileName = fs::path(inputFileName).stem();
    outputFileName += ".json";
    std::ofstream outputJson(OUTPUT_FOLDER_PATH / outputFileName);
    outputJson << std::setw(4) << jsonData << std::endl;

    std::cout << "Updated Json file has been created in " << OUTPUT_FOLDER_PATH << '/' << outputFileName.string() << std::endl << std::endl;
    Files_Processed++;

}

void replaceJsonSubstring(nlohmann::ordered_json& jsonObject, std::string substr, std::string replacement) {

    for (auto& element : jsonObject.items()) {
        if (element.value().is_string()) {
            std::string str = element.value();
;            size_t pos = str.find(substr);
                
            while (pos != std::string::npos) {
                str.replace(pos, substr.length(), replacement);
                pos = str.find(substr, pos + replacement.length());
                element.value() = str;
            }
        }
        else if (element.value().is_object() || element.value().is_array()) {
            replaceJsonSubstring(element.value(), substr, replacement);
        }
    }

}