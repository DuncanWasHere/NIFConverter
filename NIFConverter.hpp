#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <windows.h>
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;

const std::string INPUT_FOLDER_PATH = "Input";
const std::string OUTPUT_FOLDER_PATH = "Output";
const std::string TEMP_JSON_FOLDER_PATH = "Temp";
const std::string SNIFF_INI_DEFAULT = "SNIFF Configs/SniffDefault.ini";
const std::string SNIFF_INI_NIF_TO_JSON_PATH = "SNIFF Configs/SniffNifToJson.ini";
const std::string SNIFF_INI_JSON_TO_NIF_PATH = "SNIFF Configs/SniffJsonToNif.ini";
const std::string SNIFF_INI_REMOVE_TEXTURING_PROPERTY = "SNIFF Configs/SniffRemoveTexturingProperty.ini";
const std::string SNIFF_INI_REMOVE_BINARY_EXTRA_DATA = "SNIFF Configs/SniffRemoveBinaryExtraData.ini";
const std::string SNIFF_INI_REMOVE_VERTEX_COLOR_PROPERTY = "SNIFF Configs/SniffRemoveVertexColorProperty.ini";
const std::string SNIFF_INI_REMOVE_STRING_PALETTE = "SNIFF Configs/SniffRemoveStringPalette.ini";
const std::string LOG_PATH = "NIFConverter.log";
std::ofstream LOG_FILE(LOG_PATH, std::ios::app);
int Files_Processed;

void processJsonFile(const fs::path& inputFilePath);
void replaceJsonSubstring(nlohmann::ordered_json& jsonObject, std::string substr, std::string replacement);
nlohmann::ordered_json GetNIFBlockFromArray(nlohmann::ordered_json jsonData, nlohmann::ordered_json objectToParse, std::string searchString);
void runSniffCommand(const std::string& operation, const fs::path& inputDir, const fs::path& outputDir, const std::string& iniFile);
void waitForSniff();
void outputToConsoleAndFile(const std::string& message, std::ostream& outputStream);