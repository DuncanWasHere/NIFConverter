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
nlohmann::ordered_json GetNIFBlockFromArray(nlohmann::ordered_json jsonData, nlohmann::ordered_json objectToParse, std::string searchString);


int main() {
    try {

        for (const auto& entry : fs::recursive_directory_iterator(INPUT_FOLDER_PATH)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                processJsonFile(entry.path().string());
            }
        }

        std::cout << std::endl << std::endl
            << "#######################################################" << std::endl
            << "Operation completed successfully. Files Processed: " << Files_Processed << std::endl
            << "#######################################################" << std::endl;
        system("pause");

    }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
    }
    catch (const nlohmann::json::type_error& e) {
        std::cerr << "Type error: " << e.what() << std::endl;
    }
    catch (const    std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    catch (const    std::runtime_error& e) {
        std::cerr << "Runtime Error: " << e.what() << std::endl;
    }

    return 0;

}

void processJsonFile(const std::string& inputFileName) {


    // Open input JSON
    std::ifstream inputJson(inputFileName);
    if (!inputJson.is_open()) {
        throw std::exception();
        std::cerr << "Error: Unable to open input file " << inputFileName << " !" << std::endl;
        return;
    }
    else {
        std::cout << "Processing file: " << inputFileName << std::endl;
    }


    nlohmann::ordered_json jsonData;
    inputJson >> jsonData;


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
    else
        throw std::exception("NiHeader not found! File is likely the wrong format or corrupted.");


    // Add BSShaderPPLightingProperty, BSShaderTextureSet, and update NiGeomMorpherController properties

    auto blockTypeIndex = jsonData["NiHeader"]["Block Type Index"];
    size_t blockTypeIndexRange = blockTypeIndex.size();

    for (auto& i : blockTypeIndex.items()) {

        std::string str = i.value();
        
        if (str == "NiTriStrips" || str == "NiTriShape") {
            nlohmann::ordered_json& geomBlock = jsonData[i.key() + " " + str];
            nlohmann::ordered_json texturingProperty = GetNIFBlockFromArray(jsonData, geomBlock["Properties"], "NiTexturingProperty");
            nlohmann::ordered_json sourceTexture = jsonData[texturingProperty["Base Texture"]["Source"]];
            std::string texturePath = sourceTexture["File Name"];
            size_t pos = texturePath.rfind(".dds");
            texturePath.erase(pos);
            size_t geomBlockNumProperties = geomBlock["Properties"].size();

            auto& properties = geomBlock["Properties"];
            properties.push_back(std::to_string(blockTypeIndexRange) + " BSShaderPPLightingProperty");

            nlohmann::ordered_json shaderPPLightingProperty;
            jsonData[std::to_string(blockTypeIndexRange) + " BSShaderPPLightingProperty"] = {
                {"Name", ""},
                {"Extra Data List", nlohmann::json::array()},
                {"Controller", "None"},
                {"Flags", "1"},
                {"Shader Type", "SHADER_DEFAULT"},
                {"Shader Flags 1", ""},
                {"Shader Flags 2", ""},
                {"Environment Map Scale", "1.000000"},
                {"Texture Clamp Mode", "WRAP_S_WRAP_T"},
                {"Texture Set", std::to_string(blockTypeIndexRange + 1) + " BSShaderTextureSet"},
                {"Refraction Strength", "0.000000"},
                {"Refraction File Period", "0"},
                {"Parallax Max Passes", "4.000000"},
                {"Parallax Scale", "1.000000"}
            };

            jsonData["NiHeader"]["Block Type Index"].push_back("BSShaderPPLightingProperty");
            if (!jsonData["NiHeader"]["Block Types"].contains("BSShaderPPLightingProperty"))
                jsonData["NiHeader"]["Block Types"].push_back("BSShaderPPLightingProperty");
            blockTypeIndexRange++;

            jsonData[std::to_string(blockTypeIndexRange) + " BSShaderTextureSet"] = {
                {"Textures", nlohmann::json::array({texturePath + ".dds", texturePath + "_n.dds", "", "", "", ""})},
            };
            jsonData["NiHeader"]["Block Type Index"].push_back("BSShaderTextureSet");
            if (!jsonData["NiHeader"]["Block Types"].contains("BSShaderTextureSet"))
                jsonData["NiHeader"]["Block Types"].push_back("BSShaderTextureSet");

            std::string numBlocks = jsonData["NiHeader"]["Num Blocks"];
            numBlocks = std::to_string(std::stoi(numBlocks) + 2);
            jsonData["NiHeader"]["Num Blocks"] = numBlocks;

            auto it = jsonData.find("NiFooter");
            if (it != jsonData.end()) {
                nlohmann::json niFooter = it.value();
                jsonData.erase(it);
                jsonData["NiFooter"] = niFooter;
            }
        }

        else if (str == "NiGeomMorpherController") {
            nlohmann::ordered_json& geomMorpherController = jsonData[i.key() + " " + str];
            std::vector<std::string> interpolators;
            interpolators = geomMorpherController["Interpolators"];
            geomMorpherController["Interpolator Weights"];
            for (auto& j : interpolators) {
                geomMorpherController["Interpolator Weights"].push_back({
                    {"Interpolator", j},
                    {"Weight", "0.000000"}
                });
            }
        }

        else if (str == "NiTriStripsData" || str == "NiTriShapeData") {
            nlohmann::ordered_json& geomDataBlock = jsonData[i.key() + " " + str];
            replaceJsonSubstring(geomDataBlock, "UV_1", "Has_UV");
            replaceJsonSubstring(geomDataBlock, "UV_2", "Has_UV");
            replaceJsonSubstring(geomDataBlock, "UV_3", "Has_UV");
            replaceJsonSubstring(geomDataBlock, "UV_4", "Has_UV");
        }

    }


    // Update collision layers and materials
    for (auto it = jsonData.begin(); it != jsonData.end(); it++) {
        if (it.key().find(" bhk") != std::string::npos) {
            replaceJsonSubstring(it.value(), "OB_HAV_MAT", "FO_HAV_MAT");
            replaceJsonSubstring(it.value(), "OL_", "FOL_");
            replaceJsonSubstring(it.value(), "SKYL_", "FOL_");
        }
    }


    // Write JSON data to new file
    fs::path outputFileName = fs::path(inputFileName).stem();
    outputFileName += ".json";
    std::ofstream outputJson(OUTPUT_FOLDER_PATH / outputFileName);
    outputJson << std::setw(4) << jsonData << std::endl;

    std::cout << "Updated JSON file has been created at " << OUTPUT_FOLDER_PATH << '/' << outputFileName.string() << std::endl << std::endl;
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

nlohmann::ordered_json GetNIFBlockFromArray( nlohmann::ordered_json jsonData, nlohmann::ordered_json objectToParse, std::string searchString ) { 

    if (objectToParse.is_array()) {
        for (auto& i : objectToParse.items()) {
            std::string str = i.value();
            if (str.find(searchString) != std::string::npos)
                return jsonData[i.value()];
        }
        std::cout << "ERROR: String not found: " + searchString << std::endl;
        return nlohmann::ordered_json();
    }
    
    std::cout << "ERROR: Object " << objectToParse << " is not an array!" << std::endl;
    return nlohmann::ordered_json();

}