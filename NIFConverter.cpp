#include "NIFConverter.hpp"


int main() {

	// Log to file
	if (!LOG_FILE.is_open()) {
		outputToConsoleAndFile("ERROR: Failed to open log file: " + LOG_PATH + "\n", LOG_FILE);
		return 1;
	}

	if (!fs::exists(TEMP_JSON_FOLDER_PATH))
		fs::create_directories(TEMP_JSON_FOLDER_PATH);

	if (!fs::exists(INPUT_FOLDER_PATH))
		fs::create_directories(INPUT_FOLDER_PATH);

	if (!fs::exists(OUTPUT_FOLDER_PATH))
		fs::create_directories(OUTPUT_FOLDER_PATH);

	// Prevent other instances of SNIFF from locking the thread
	if (FindWindowA(NULL, "Sniff.exe")) {
		outputToConsoleAndFile("ERROR: SNIFF is already running. Please close it before running NIFConverter.\n", LOG_FILE);
		return 1;
	}

	// Convert to JSON
	outputToConsoleAndFile("Waiting for SNIFF to convert to JSON...\n", LOG_FILE);
	runSniffCommand("Convert to and from JSON", INPUT_FOLDER_PATH, TEMP_JSON_FOLDER_PATH, SNIFF_INI_NIF_TO_JSON_PATH);
	waitForSniff();
	outputToConsoleAndFile("Done! Updating NIF format...\n", LOG_FILE);

	// Process JSON files
	for (const auto& entry : fs::recursive_directory_iterator(TEMP_JSON_FOLDER_PATH)) {
		if (entry.is_regular_file() && entry.path().extension() == ".json") {
			processJsonFile(entry.path());
		}
	}

	// Convert to NIF/KF
	outputToConsoleAndFile("Waiting for SNIFF to convert to NIF/KF...\n", LOG_FILE);
	runSniffCommand("Convert to and from JSON", TEMP_JSON_FOLDER_PATH, OUTPUT_FOLDER_PATH, SNIFF_INI_JSON_TO_NIF_PATH);
	waitForSniff();

	// Remove NiTexturingProperty
	outputToConsoleAndFile("Waiting for SNIFF to remove NiTexturingProperty...\n", LOG_FILE);
	runSniffCommand("Remove nodes", OUTPUT_FOLDER_PATH, OUTPUT_FOLDER_PATH, SNIFF_INI_REMOVE_TEXTURING_PROPERTY);
	waitForSniff();

	// Remove NiBinaryExtraData
	outputToConsoleAndFile("Waiting for SNIFF to remove NiBinaryExtraData...\n", LOG_FILE);
	runSniffCommand("Remove nodes", OUTPUT_FOLDER_PATH, OUTPUT_FOLDER_PATH, SNIFF_INI_REMOVE_BINARY_EXTRA_DATA);
	waitForSniff();

	// Remove NiVertexColorProperty
	outputToConsoleAndFile("Waiting for SNIFF to remove NiVertexColorProperty...\n", LOG_FILE);
	runSniffCommand("Remove nodes", OUTPUT_FOLDER_PATH, OUTPUT_FOLDER_PATH, SNIFF_INI_REMOVE_VERTEX_COLOR_PROPERTY);
	waitForSniff();

	// Remove NiStringPalette
	outputToConsoleAndFile("Waiting for SNIFF to remove NiStringPalette...\n", LOG_FILE);
	runSniffCommand("Remove nodes", OUTPUT_FOLDER_PATH, OUTPUT_FOLDER_PATH, SNIFF_INI_REMOVE_STRING_PALETTE);
	waitForSniff();

	// Update Tangents
	outputToConsoleAndFile("Waiting for SNIFF to update tangents...\n", LOG_FILE);
	runSniffCommand("Update tangents and binormals", OUTPUT_FOLDER_PATH, OUTPUT_FOLDER_PATH, SNIFF_INI_DEFAULT);
	waitForSniff();

	// Add Skinned Flag
	outputToConsoleAndFile("Waiting for SNIFF to add Skinned flag...\n", LOG_FILE);
	runSniffCommand("Add missing Skinned flag", OUTPUT_FOLDER_PATH, OUTPUT_FOLDER_PATH, SNIFF_INI_DEFAULT);
	waitForSniff();

	// Skinned meshes inexplicably break in NifSkope when converted from JSON but seem to work fine in game. Reconverting them somehow solves the issue

	// Convert to JSON
	outputToConsoleAndFile("Waiting for SNIFF to convert to JSON...\n", LOG_FILE);
	runSniffCommand("Convert to and from JSON", OUTPUT_FOLDER_PATH, TEMP_JSON_FOLDER_PATH, SNIFF_INI_NIF_TO_JSON_PATH);
	waitForSniff();

	// Convert to NIF/KF
	outputToConsoleAndFile("Waiting for SNIFF to convert to NIF/KF...\n", LOG_FILE);
	runSniffCommand("Convert to and from JSON", TEMP_JSON_FOLDER_PATH, OUTPUT_FOLDER_PATH, SNIFF_INI_JSON_TO_NIF_PATH);
	waitForSniff();



	// Clear temp files
	outputToConsoleAndFile("Deleting temp files...\n", LOG_FILE);
	std::filesystem::remove_all(TEMP_JSON_FOLDER_PATH);
	outputToConsoleAndFile("\n\n------------------------------------------------------\nOperation completed successfully! Files Processed: " + std::to_string(Files_Processed) + "\n------------------------------------------------------\n", LOG_FILE);
	system("pause");

	return 0;

}

void processJsonFile(const fs::path& inputDir) {

	// Open input JSON
	fs::path relativeDir = fs::relative(inputDir, TEMP_JSON_FOLDER_PATH);
	fs::path outputDir = TEMP_JSON_FOLDER_PATH / relativeDir;
	std::ifstream inputJson(inputDir);

	if (!inputJson.is_open()) {
		outputToConsoleAndFile("ERROR: Unable to open input file: " + relativeDir.u8string() + "\n", LOG_FILE);
		return;
	}
	else
		outputToConsoleAndFile("Processing file: " + relativeDir.u8string() + "\n", LOG_FILE);

	fs::create_directories(outputDir.parent_path());


	nlohmann::ordered_json jsonData;
	inputJson >> jsonData;
	inputJson.close();


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
		outputToConsoleAndFile("ERROR: NiHeader not found in: " + relativeDir.u8string() + " File is likely the wrong format or corrupted!\n", LOG_FILE);


	// Add BSShaderPPLightingProperty, BSShaderTextureSet, and update NiGeomMorpherController and Vertex properties
	auto blockTypeIndex = jsonData["NiHeader"]["Block Type Index"];
	size_t blockTypeIndexRange = blockTypeIndex.size();

	for (auto& i : blockTypeIndex.items()) {

		std::string str = i.value();

		if (str == "NiTriStrips" || str == "NiTriShape") {
			nlohmann::ordered_json& geomBlock = jsonData[i.key() + " " + str];
			nlohmann::ordered_json texturingProperty = GetNIFBlockFromArray(jsonData, geomBlock["Properties"], "NiTexturingProperty");
			if (!texturingProperty.is_null()) {
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

					blockTypeIndexRange++;
				}
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

		else if (str == "NiControllerSequence") {
			size_t numControlledBlocks = 0;
			nlohmann::ordered_json& controllerSequence = jsonData[i.key() + " " + str];
			if (controllerSequence.contains("Controlled Blocks")) {
				nlohmann::ordered_json& stringPaletteBlock = jsonData[controllerSequence["String Palette"]];
				std::string stringPalette = stringPaletteBlock["Palette"];
				nlohmann::ordered_json& controlledBlocks = controllerSequence["Controlled Blocks"];
				numControlledBlocks = controlledBlocks.size();

				// Extract substrings based on offsets
				std::vector<std::string> nodeNames(numControlledBlocks);
				size_t nodeNameOffset;
				std::vector<std::string> controllerTypes(numControlledBlocks);
				size_t controllerTypeOffset;
				std::vector<std::string> interpolatorIDs(numControlledBlocks);
				size_t interpolatorIDOffset;
				size_t nextNullTerminatorPos;

				for (auto& j : controlledBlocks.items()) {
					nlohmann::ordered_json& controlledBlock = j.value();

					// Node Name
					nodeNameOffset = std::stoul(controlledBlock["Node Name Offset"].get<std::string>());
					nextNullTerminatorPos = stringPalette.find('\u0000', nodeNameOffset);
					controlledBlock["Node Name"] = stringPalette.substr(nodeNameOffset, nextNullTerminatorPos - nodeNameOffset);

					// Controller Type
					controllerTypeOffset = std::stoul(controlledBlock["Controller Type Offset"].get<std::string>());
					nextNullTerminatorPos = stringPalette.find('\u0000', controllerTypeOffset);
					controlledBlock["Controller Type"] = stringPalette.substr(controllerTypeOffset, nextNullTerminatorPos - controllerTypeOffset);

					// Interpolator ID
					if (controlledBlock["Controller Type"] == "NiGeomMorpherController") {
						interpolatorIDOffset = std::stoul(controlledBlock["Interpolator ID Offset"].get<std::string>());
						nextNullTerminatorPos = stringPalette.find('\u0000', interpolatorIDOffset);
						controlledBlock["Interpolator ID"] = stringPalette.substr(interpolatorIDOffset, nextNullTerminatorPos - interpolatorIDOffset);
					}
				}
			}
			else
				outputToConsoleAndFile("ERROR: NiControllerSequence found but no Controlled Blocks!\n", LOG_FILE);
		}

	}


	// Update collision layers and materials
	for (auto it = jsonData.begin(); it != jsonData.end(); it++) {
		if (it.key().find(" bhk") != std::string::npos) {
			replaceJsonSubstring(it.value(), "OB_HAV_MAT", "FO_HAV_MAT");
			replaceJsonSubstring(it.value(), "OL_", "FOL_");
			replaceJsonSubstring(it.value(), "SKYL_", "FOL_");
			replaceJsonSubstring(it.value(), "SNOW", "DIRT");
		}
	}


	// Write JSON data to new file
	std::ofstream outputJson(outputDir);
	outputJson << std::setw(4) << jsonData << std::endl;
	outputToConsoleAndFile("File successfully processed: " + relativeDir.u8string() + "\n\n", LOG_FILE);

	Files_Processed++;


}

void replaceJsonSubstring(nlohmann::ordered_json& jsonObject, std::string substr, std::string replacement) {

	for (auto& element : jsonObject.items()) {
		if (element.value().is_string()) {
			std::string str = element.value();
			size_t pos = str.find(substr);
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

nlohmann::ordered_json GetNIFBlockFromArray(nlohmann::ordered_json jsonData, nlohmann::ordered_json objectToParse, std::string searchString) {
	if (objectToParse.is_array()) {
		for (auto& i : objectToParse.items()) {
			std::string str = i.value();
			if (str.find(searchString) != std::string::npos)
				return jsonData[i.value()];
		}
		outputToConsoleAndFile("ERROR: String not found: " + searchString + "\n", LOG_FILE);
		return nlohmann::ordered_json();
	}
	outputToConsoleAndFile("ERROR: Property found but is not an array!\n", LOG_FILE);
	return nlohmann::ordered_json();
}

void runSniffCommand(const std::string& operation, const fs::path& inputDir, const fs::path& outputDir, const std::string& iniFile) {
	std::string command = "Sniff.exe -OP:\"" + operation + "\""
		+ " -I:\"" + inputDir.string() + "\""
		+ " -O:\"" + outputDir.string() + "\""
		+ " -S:\"" + iniFile + "\""
		+ " -subdir:yes -skip:yes -log:\"Sniff.log\"";
	int result = system(command.c_str());
	if (result != 0) {
		outputToConsoleAndFile("ERROR: SNIFF command failed with code " + std::to_string(result) + "\n", LOG_FILE);
	}
}

void waitForSniff() {
	while (FindWindowA(NULL, "Sniff.exe")) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void outputToConsoleAndFile(const std::string& message, std::ostream& outputStream) {
	std::cout << message;
	outputStream << message;
}