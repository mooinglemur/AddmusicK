#include "globals.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <algorithm>		// // //
// // //
#include "../AM405Remover/AM405Remover.h"
#include <cstdint>
// // //#include "lodepng.h"
#include <thread>
#include "asardll.h"		// // //
#include "SoundEffect.h"		// // //
#include "Music.h"		// // //
#include "MML/Lexer.h"		// // //

bool waitAtEnd = true;
fs::path ROMName;		// // //
std::vector<uint8_t> rom;		// // //

Music musics[256];		// // //
SoundEffect soundEffects[SFX_BANKS][256];		// // //
//SoundEffect (&soundEffectsDF9)[256] = soundEffects[0];
//SoundEffect (&soundEffectsDFC)[256] = soundEffects[1];
std::vector<uint8_t> romHeader;		// // //

void cleanROM();
void tryToCleanSampleToolData();
void tryToCleanAM4Data();
void tryToCleanAMMData();

void assembleSNESDriver();
void assembleSPCDriver();
void loadMusicList();
void loadSampleList();
void loadSFXList();
void compileSFX();
void compileGlobalData();
void compileMusic();
void fixMusicPointers();
void generateSPCs();
void assembleSNESDriver2();
void generateMSC();
void cleanUpTempFiles();

int SNESToPC(int addr);		// // //
int PCToSNES(int addr);		// // //

//std::time_t getLastModifiedTime();		// // //

void generatePNGs();

void checkMainTimeStamps();
bool recompileMain = true;

time_t mostRecentMainModification = 0;		// The most recent modification to any sound effect file, any global song file, any list file, or any asm file


bool justSPCsPlease = false;
std::vector<std::string> textFilesToCompile;

int main(int argc, char **argv) try {		// // //
/*
	const std::locale loc {"en_US.UTF8"};		// // //
	std::locale::global(loc);
	std::cout.imbue(loc);
	std::cerr.imbue(loc);
*/
	auto startTime = std::chrono::steady_clock::now();		// // //

	std::cout << "AddmusicK version " << AMKVERSION << "." << AMKMINOR << "." << AMKREVISION << " by Kipernal\n";
	std::cout << "Parser version " << PARSER_VERSION << "\n\n";
	std::cout << "Protip: Be sure to read the readme! If there's an error or something doesn't\nseem right, it may have your answer!\n\n\n";

	std::vector<std::string> arguments;

	if (fs::exists("Addmusic_options.txt")) {		// // //
		// This is probably a catastrophicly bad idea on several levels, but I don't have the time do redo this entire section of code.
		// AddmusicK 2.0: Now with actually good programming habits! (probably not)
		auto optionStr = openTextFile("Addmusic_options.txt");		// // //
		AMKd::MML::SourceView options {optionStr};
		using namespace AMKd::MML::Lexer;
		while (auto line = GetParameters<Row>(options))
			arguments.push_back(std::string {line.get<0>()});
	}
	else
		for (int i = 1; i < argc; i++)
			arguments.push_back(argv[i]);

	std::ofstream redirectOut, redirectErr;		// // //

	for (const auto &arg : arguments) {		// // //
		waitAtEnd = false;			// If the user entered a command line argument, chances are they don't need the "Press Any Key To Continue . . ." prompt.
		if (arg == "-c")
			convert = false;
		else if (arg == "-e")
			checkEcho = false;
		else if (arg == "-b")
			bankStart = 0x080000;
		else if (arg == "-v")
			verbose = true;
		else if (arg == "-a")
			aggressive = true;
		else if (arg == "-d")
			dupCheck = false;
		else if (arg == "-h")
			validateHex = false;
		else if (arg == "-p")
			doNotPatch = true;
		else if (arg == "-u")
			optimizeSampleUsage = false;
		else if (arg == "-s")
			allowSA1 = false;
		else if (arg == "-dumpsfx")
			sfxDump = true;
		else if (arg == "-visualize")
			visualizeSongs = true;
		else if (arg == "-g")
			forceSPCGeneration = true;
		else if (arg == "-noblock")
			forceNoContinuePrompt = true;
		else if (arg == "-streamredirect") {
			redirectStandardStreams = true;
			redirectOut.open("AddmusicK_stdout", std::ios::out | std::ios::trunc);		// // //
			redirectErr.open("AddmusicK_stderr", std::ios::out | std::ios::trunc);
			std::cout.rdbuf(redirectOut.rdbuf());
			std::cerr.rdbuf(redirectErr.rdbuf());
		}
		else if (arg == "-norom") {
			if (!ROMName.empty())		// // //
				fatalError("Error: -norom cannot be used after a filepath has already been used.\n"		// // //
						   "Input your text files /after/ the -norom option.");
			justSPCsPlease = true;
		}
		else if (ROMName.empty() && arg[0] != '-')
			if (!justSPCsPlease)
				ROMName = arg;
			else
				textFilesToCompile.push_back(arg);
		else {
			if (arg != "-?")
				std::cout << "Unknown argument \"%s\"." << arg << "\n\n";

			std::cout << "Options:\n"		// // //
						 "\t-e: Turn off echo buffer checking.\n"
						 "\t-c: Force off conversion from Addmusic 4.05 and AddmusicM\n"
						 "\t-b: Do not attempt to save music data in bank 0x40 and above.\n"
						 "\t-v: Turn verbosity on.  More information will be displayed using this.\n"
						 "\t-a: Make free space finding more aggressive.\n"
						 "\t-d: Turn off duplicate sample checking.\n"
						 "\t-h: Turn off hex command validation.\n"
						 "\t-p: Create a patch, but do not patch it to the ROM.\n"
						 "\t-norom: Only generate SPC files, no ROM required.\n"
						 "\t-?: Display this message.\n\n";

			if (arg != "-?")
				quit(1);
		}
	}

	useAsarDLL = asar_init();		// // //

	if (justSPCsPlease == false) {
		if (ROMName.empty()) {		// // //
			std::cout << "Enter your ROM name: ";		// // //
			std::string temp;
			std::getline(std::cin, temp);
			ROMName = temp;
			puts("\n\n");
		}

		fs::path smc = fs::path {ROMName} += ".smc";		// // //
		fs::path sfc = fs::path {ROMName} += ".sfc";
		if (fs::exists(smc) && fs::exists(sfc))
			fatalError("Error: Ambiguity detected; there were two ROMs with the specified name (one\n"
					   "with a .smc extension and one with a .sfc extension). Either delete one or\n"
					   "include the extension in your filename.");
		else if (fs::exists(smc))
			ROMName = smc;
		else if (fs::exists(sfc))
			ROMName = sfc;
		else if (!fs::exists(ROMName))
			fatalError("ROM not found.");
		rom = openFile(ROMName);		// // //

		tryToCleanAM4Data();
		tryToCleanAMMData();

		if (rom.size() % 0x8000 != 0) {
			romHeader.assign(rom.begin(), rom.begin() + 0x200);
			rom.erase(rom.begin(), rom.begin() + 0x200);
		}
		//rom.openFromFile(ROMName);

		if (rom.size() <= 0x80000)
			fatalError("Error: Your ROM is too small. Save a level in Lunar Magic or expand it with\n"
					   "Lunar Expand, then try again.");		// // //

		usingSA1 = (rom[SNESToPC(0xFFD5)] == 0x23 && allowSA1);		// // //
		cleanROM();
	}

	loadSampleList();
	loadMusicList();
	loadSFXList();

	checkMainTimeStamps();

	assembleSNESDriver();		// We need this for the upload position, where the SPC file's PC starts.  Luckily, this function is very short.

	if (recompileMain || forceSPCGeneration) {
		assembleSPCDriver();
		compileSFX();
		compileGlobalData();
	}

	if (justSPCsPlease) {
		for (int i = highestGlobalSong + 1; i < 256; i++)
			musics[i].exists = false;

		for (int i = 0, n = textFilesToCompile.size(); i < n; ++i) {		// // //
			if (highestGlobalSong + i >= 256)
				fatalError("Error: The total number of requested music files to compile exceeded 255.");		// // //
			musics[highestGlobalSong + 1 + i].exists = true;
			// // //
			musics[highestGlobalSong + 1 + i].name = textFilesToCompile[i];
		}
	}

	compileMusic();
	fixMusicPointers();

	generateSPCs();

	if (visualizeSongs)
		generatePNGs();

	if (justSPCsPlease == false) {
		assembleSNESDriver2();
		generateMSC();
#ifndef _DEBUG
		cleanUpTempFiles();
#endif
	}

	auto elapsed = std::chrono::duration<double> {std::chrono::steady_clock::now() - startTime}.count();		// // //

	std::cout << "\nSuccess!\n\n";
	if (elapsed - 1 < 0.02)
		std::cout << "Completed in 1 second.\n";
	else
		std::cout << "Completed in " << std::dec << std::setprecision(2) << std::fixed << elapsed << " seconds.\n";

	if (waitAtEnd)
		quit(0);
	return 0;
}
catch (std::exception &e) {		// // //
	std::cerr << "Unexpected error: \n" << e.what() << '\n';
#ifdef _DEBUG
	__debugbreak();
#endif
	if (waitAtEnd)
		quit(1);
	return 1;
}

void displayNewUserMessage() {
#ifdef _WIN32
	(void)system("cls");
#else
	(void)system("clear");
#endif

	if (forceNoContinuePrompt == false) {
		std::cout << "This is a clean ROM you're using AMK on.";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\n\nSo here's a message for new users.";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\n\nIf there's some error you don't understand,";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\nOr if something weird happens and you don't know why,";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\n\n\nRead the whole ";
		std::this_thread::sleep_for(std::chrono::milliseconds(750));
		std::cout << "buggin' ";
		std::this_thread::sleep_for(std::chrono::milliseconds(750));
		std::cout << "ever ";
		std::this_thread::sleep_for(std::chrono::milliseconds(750));
		std::cout << "lovin' ";
		std::this_thread::sleep_for(std::chrono::milliseconds(750));
		std::cout << "README!";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\n\n\nReally, it has answers to some of the most basic questions.";
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		std::cout << "\nIf for no one else than yourself, read the readme first.";
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		std::cout << "\nThat way you don't get chastised for asking something answered by it.";
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		std::cout << "\nNot every possible answer is in there,";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\nBut save yourself some time and at least make an effort.";
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		std::cout << "\n\nDo we have a deal?";
		std::this_thread::sleep_for(std::chrono::milliseconds(2500));
		std::cout << "\nAlright. Cool. Now go out there and use/make awesome music.";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\n\n(Power users: Use -noblock to skip this on future new ROMs.)";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	}
}

void cleanROM() {
	//tryToCleanAM4Data();
	//tryToCleanAMMData();
	tryToCleanSampleToolData();

	if (rom[0x70000] == 0x3E && rom[0x70001] == 0x0E) {	// If this is a "clean" ROM, then we don't need to do anything.
		//displayNewUserMessage();
		writeFile("asm/SNES/temp.sfc", rom);
		return;
	}
	else {
		//"New Super Mario World Sample Utility 2.0 by smkdan"

		std::string romprogramname;
		romprogramname += (char)*(rom.data() + SNESToPC(0x0E8000));
		romprogramname += (char)*(rom.data() + SNESToPC(0x0E8001));
		romprogramname += (char)*(rom.data() + SNESToPC(0x0E8002));
		romprogramname += (char)*(rom.data() + SNESToPC(0x0E8003));

		if (romprogramname != "@AMK") {
			std::cout << "Error: The identifier for this ROM, \"" << romprogramname << "\", could not be identified. It should\n"
				 "be \"@AMK\". This either means that some other program has modified this area of\n"
				 "your ROM, or your ROM is corrupted. Continue?\n";
			if (!YesNo())		// // //
				quit(1);
		}

		int romversion = *(unsigned char *)(rom.data() + SNESToPC(0x0E8004));

		if (romversion > DATA_VERSION) {
			std::cout << "WARNING: This ROM was modified using a newer version of AddmusicK.\n";
			std::cout << "You can continue, but it is HIGHLY recommended that you upgrade AMK first.\n";
			std::cout << "Continue anyway?\n";
			if (!YesNo())		// // //
				quit(1);
		}

		int address = SNESToPC(*(unsigned int *)(rom.data() + 0x70005) & 0xFFFFFF);	// Address, in this case, is the sample numbers list.
		clearRATS(rom, address - 8);		// // // So erase it all.

		int baseAddress = SNESToPC(*(unsigned int *)(rom.data() + 0x70008));		// Address is now the address of the pointers to the songs and samples.

		bool erasingSamples = false;
		while (true) {
			address = *(unsigned int *)(rom.data() + baseAddress) & 0xFFFFFF;
			if (address == 0xFFFFFF) {						// 0xFFFFFF indicates an end of pointers.
				if (std::exchange(erasingSamples, true))		// // //
					break;
			}
			else {
				if (address != 0)
					clearRATS(rom, SNESToPC(address - 8));		// // //
			}

			baseAddress += 3;
		}
	}

	writeFile("asm/SNES/temp.sfc", rom);
}

// // // moved
// Scans an integer value that comes after the specified string within another string.  Must be in $XXXX format (or $XXXXXX, etc.).
unsigned scanInt(const std::string &str, const std::string &value) {
	size_t i = str.find(value);		// // //
	if (i == std::string::npos || str[i + value.size()] != '$')
		fatalError("Error: Could not find \"" + value + "\"");		// // //

	unsigned ret;
	std::stringstream ss {str.substr(i + value.size() + 1, 8)};		// // //
	ss >> std::hex >> ret;
	return ret;
}

void assembleSNESDriver() {
	programUploadPos = scanInt(openTextFile("asm/SNES/patch.asm"), "!DefARAMRet = ");		// // //
}

void assembleSPCDriver() {
	removeFile("temp.log");		// // //
	removeFile("asm/main.bin");

	programPos = scanInt(openTextFile("asm/main.asm"), "base ");		// // //
	if (verbose)
		std::cout << "Compiling main SPC program, pass 1.\n";

	//execute("asar asm/main.asm asm/main.bin 2> temp.log > temp.txt");

	//if (fileExists("temp.log"))
	if (!asarCompileToBIN("asm/main.asm", "asm/main.bin"))
		fatalError("asar reported an error while assembling asm/main.asm. Refer to temp.log for\ndetails.");		// // //

	std::string temptxt = openTextFile("temp.txt");		// // //
	mainLoopPos = scanInt(temptxt, "MainLoopPos: ");
	reuploadPos = scanInt(temptxt, "ReuploadPos: ");
	SRCNTableCodePos = scanInt(temptxt, "SRCNTableCodePos: ");

	removeFile("temp.log");

	programSize = static_cast<unsigned>(fs::file_size("asm/main.bin"));		// // //
}

void loadMusicList() {
	const std::string SONG_LIST {"Addmusic_list.txt"};
	auto listStr = openTextFile(SONG_LIST);		// // //
	AMKd::MML::SourceView list {listStr};
	using namespace AMKd::MML::Lexer;

	bool inGlobals = false;
	bool inLocals = false;
	int shallowSongCount = 0;

	highestGlobalSong = -1;		// // //

	while (list.HasNextToken()) {		// // //
		if (list.Trim("Globals:")) {
			inGlobals = true;
			inLocals = false;
		}
		else if (list.Trim("Locals:")) {
			inGlobals = false;
			inLocals = true;
		}
		else {
			if (!inGlobals && !inLocals)
				fatalError("Error: Could not find \"Globals:\" or \"Locals:\" label.",
						   SONG_LIST, list.GetLineNumber());		// // //

			auto param = GetParameters<HexInt>(list);
			if (!param || param.get<0>() > 0xFFu || !list.SkipSpaces())
				fatalError("Invalid song index.", SONG_LIST, list.GetLineNumber());
			int index = param.get<0>();
			if (inGlobals)
				highestGlobalSong = std::max(highestGlobalSong, index);
			else if (inLocals && index <= highestGlobalSong)
				fatalError("Error: Local song numbers must be lower than the largest global song number.",
						   SONG_LIST, list.GetLineNumber());		// // //

			auto name = list.Trim("[^\\r\\n]+");
			if (!name)
				fatalError("Error: Could not read file name.", SONG_LIST, list.GetLineNumber());
			musics[index].name = *name;
			musics[index].exists = true;
//			if (inLocals && !justSPCsPlease)
//				musics[index].text = openTextFile(fs::path("music") / tempName);		// // //
			++shallowSongCount;
		}
	}

	if (verbose)
		std::cout << "Read in all " << shallowSongCount << " songs.\n";		// // //

	for (int i = 255; i >= 0; i--) {
		if (musics[i].exists) {
			songCount = i + 1;
			break;
		}
	}
}

void loadSampleList() {
	const std::string SAMPGROUP_NAME {"Addmusic_sample groups.txt"};
	auto listStr = openTextFile(SAMPGROUP_NAME);		// // //
	AMKd::MML::SourceView list {listStr};
	using namespace AMKd::MML::Lexer;

	while (list.HasNextToken()) {		// // //
		auto nameParam = GetParameters<Sep<'#'>, String, Sep<'{'>>(list);
		if (!nameParam)
			fatalError("Error: Could not find sample group name.", SAMPGROUP_NAME, list.GetLineNumber());
		BankDefine sg;		// // //
		sg.bankName = nameParam.get<0>();		// // //

		while (auto item = GetParameters<QString, Option<Sep<'!'>>>(list))
			sg.samples.push_back({std::move(item.get<0>()), item.get<1>().has_value()});

		if (!GetParameters<Sep<'}'>>(list))
			fatalError("Error: Unexpected character in sample group definition.", SAMPGROUP_NAME, list.GetLineNumber());
		bankDefines.push_back(std::move(sg));
	}

/*
	for (const auto &def : bankDefines) {
		for (const auto &samp : def.samples) {
			if (!std::any_of(samples.cbegin(), samples.cend(), [&] (const Sample &s) { return s.name == samp; })) {
				//loadSample(samp, &samples[samples.size()]);
				samples[samples.size()].exists = true;
			}
		}
	}
*/
}

void loadSFXList() {		// Very similar to loadMusicList, but with a few differences.
	const std::string SFX_LIST {"Addmusic_sound effects.txt"};
	auto listStr = openTextFile(SFX_LIST);		// // //
	AMKd::MML::SourceView list {listStr};
	using namespace AMKd::MML::Lexer;

	int bank = -1;		// // //
	int SFXCount = 0;
	const fs::path BANK_FOLDER[SFX_BANKS] = {"1DF9", "1DFC"};

	while (list.HasNextToken()) {		// // //
		if (list.Trim("SFX1DF9:"))
			bank = 0;
		else if (list.Trim("SFX1DFC:"))
			bank = 1;
		else {
			if (bank == -1)
				fatalError("Error: Could not find \"SFX1DF9:\" or \"SFX1DFC:\" label.",
						   SFX_LIST, list.GetLineNumber());		// // //

			auto param = GetParameters<HexInt>(list);
			if (!param || param.get<0>() > 0xFFu || !list.SkipSpaces())
				fatalError("Invalid sound effect index.", SFX_LIST, list.GetLineNumber());
			int index = param.get<0>();

			bool isPointer = list.Trim('*');
			bool add0 = !isPointer && !list.Trim('?');
			auto name = (list.SkipSpaces(), list.Trim("[^\\r\\n]+"));
			if (!name)
				fatalError("Error: Could not read file name.", SFX_LIST, list.GetLineNumber());

			auto &samp = soundEffects[bank][index];
			(isPointer ? samp.pointName : samp.name) = *name;		// // //
			samp.exists = true;
			samp.add0 = add0;
			if (!isPointer)
				samp.loadMML(openTextFile(BANK_FOLDER[bank] / samp.name));		// // //
			++SFXCount;
		}
	}

	if (verbose)
		std::cout << "Read in all " << SFXCount << " sound effects.\n";		// // //
}

void compileSFX() {
	for (int i = 0; i < 2; i++) {
		for (int j = 1; j < 256; j++) {
			soundEffects[i][j].bank = i;
			soundEffects[i][j].index = j;
			if (soundEffects[i][j].pointName.length() > 0) {
				for (int k = 1; k < 256; k++) {
					if (soundEffects[i][j].pointName == soundEffects[i][k].name) {
						soundEffects[i][j].pointsTo = k;
						break;
					}
				}
				if (soundEffects[i][j].pointsTo == 0) {
					std::ostringstream r;
					r << "Error: The sound effect that sound effect 0x" << std::hex << j << " points to could not be found.";
					fatalError(r.str());		// // //
				}
			}
		}
	}
}

void compileGlobalData() {
	int dataTotal[SFX_BANKS] = { };		// // //
	int sfxCount[SFX_BANKS] = { };
	std::vector<uint16_t> sfxPointers[SFX_BANKS];

	const auto getSFXSpace = [&] (int bank) {		// // //
		return sfxCount[bank] * 2 + dataTotal[bank];
	};

	for (int bank = 0; bank < SFX_BANKS; ++bank)
		for (int i = 255; i >= 0; i--) {
			if (soundEffects[bank][i].exists) {
				sfxCount[bank] = i;
				break;
			}
		}

	std::vector<uint8_t> allSFXData;		// // //

	for (int bank = 0; bank < SFX_BANKS; ++bank) {		// // //
		for (int i = 0; i <= sfxCount[bank]; ++i) {
			auto &sfx = soundEffects[bank][i];		// // //
			if (sfx.exists && !sfx.pointsTo) {
				sfx.posInARAM = static_cast<uint16_t>(getSFXSpace(0) + getSFXSpace(1) + programSize + programPos);
				sfx.compile();
				sfxPointers[bank].push_back(sfx.posInARAM);
				dataTotal[bank] += sfx.dataStream.GetSize() + sfx.code.size();		// // //
			}
			else if (!sfx.exists)
				sfxPointers[bank].push_back(0xFFFF);
			else if (i > sfx.pointsTo)
				sfxPointers[bank].push_back(sfxPointers[bank][sfx.pointsTo]);
			else
				fatalError("Error: A sound effect that is a pointer to another sound effect must come after\n"
						   "the sound effect that it points to.");
			sfx.dataStream.Flush(allSFXData);		// // //
			allSFXData.insert(allSFXData.cend(), sfx.code.cbegin(), sfx.code.cend());
		}
	}

	if (errorCount > 0)
		fatalError("There were errors when compiling the sound effects.  Compilation aborted.  Your\n"
				   "ROM has not been modified.");

	if (verbose) {
		std::cout << "Total space used by 1DF9 sound effects: 0x" << hex4 << getSFXSpace(0) << '\n';
		std::cout << "Total space used by 1DFC sound effects: 0x" << hex4 << getSFXSpace(1) << '\n';
	}

	std::cout << "Total space used by all sound effects: 0x" << hex4 << (getSFXSpace(0) + getSFXSpace(1)) << std::dec << '\n';

	for (auto &x : sfxPointers)		// // //
		x.erase(x.begin(), x.begin() + 1);

	writeFile("asm/SFX1DF9Table.bin", sfxPointers[0]);
	writeFile("asm/SFX1DFCTable.bin", sfxPointers[1]);
	writeFile("asm/SFXData.bin", allSFXData);

	writeTextFile("asm/tempmain.asm", [] {
		std::string str = openTextFile("asm/main.asm");		// // //
		size_t pos = str.find("SFXTable0:");
		if (pos == std::string::npos)
			fatalError("Error: SFXTable0 not found in main.asm.");
		str.insert(pos + 10, "\r\nincbin \"SFX1DF9Table.bin\"\r\n");
		pos = str.find("SFXTable1:");
		if (pos == std::string::npos)
			fatalError("Error: SFXTable1 not found in main.asm.");
		str.insert(pos + 10, "\r\nincbin \"SFX1DFCTable.bin\"\r\nincbin \"SFXData.bin\"\r\n");
		return str;
	});

	removeFile("asm/main.bin");		// // //

	if (verbose)
		std::cout << "Compiling main SPC program, pass 2.\n";

	//execute("asar asm/tempmain.asm asm/main.bin 2> temp.log > temp.txt");
	//if (fileExists("temp.log")) 
	if (!asarCompileToBIN("asm/tempmain.asm", "asm/main.bin"))
		fatalError("asar reported an error while assembling asm/main.asm. Refer to temp.log for\ndetails.\n");		// // //

	programSize = static_cast<unsigned>(fs::file_size("asm/main.bin"));		// // //

	std::cout << "Total size of main program + all sound effects: 0x" << hex4 << programSize << std::dec << '\n';
}

void compileMusic() {
	if (verbose)
		std::cout << "Compiling music...\n";

	int totalSamplecount = 0;
	// // //
	for (int i = 0; i < 256; i++) {
		if (musics[i].exists) {
			if (!(i <= highestGlobalSong && !recompileMain)) {
				musics[i].index = i;
				musics[i].init();		// // //
				musics[i].compile();
				if (errorCount)		// // // TODO: remove
					::fatalError("There were errors when compiling the music file.  Compilation aborted.  Your ROM has not been modified.");
				musics[i].pointersFirstPass();		// // //
				musics[i].displaySongData();		// // //
				totalSamplecount += musics[i].mySamples.size();
			}
		}
	}

	//int songSampleListSize = 0;

	//for (int i = 0; i < songCount; i++)
	//{
	//	songSampleListSize += musics[i].mySampleCount + 1;
	//}

	std::stringstream songSampleList;

	songSampleListSize = 8;

	songSampleList << "db $53, $54, $41, $52\t\t\t\t; Needed to stop Asar from treating this like an xkas patch.\n";
	songSampleList << "dw SGEnd-SampleGroupPtrs-$01\ndw SGEnd-SampleGroupPtrs-$01^$FFFF\nSampleGroupPtrs:\n\n";

	for (int i = 0; i < songCount; i++) {
		if (i % 16 == 0)
			songSampleList << "\ndw ";
		if (musics[i].exists == false)
			songSampleList << "$" << hex4 << 0;
		else
			songSampleList << "SGPointer" << hex2 << i;
		songSampleListSize += 2;

		if (i != songCount - 1 && (i & 0xF) != 0xF)
			songSampleList << ", ";
		//s = songSampleList.str();
	}

	songSampleList << "\n\n";

	for (int i = 0; i < songCount; i++) {
		if (!musics[i].exists) continue;

		songSampleListSize++;

		songSampleList << "\n" << "SGPointer" << hex2 << i << ":\n";

		if (i > highestGlobalSong) {
			songSampleList << "db $" << hex2 << musics[i].mySamples.size() << "\ndw";
			for (unsigned int j = 0; j < musics[i].mySamples.size(); j++) {
				songSampleListSize += 2;
				songSampleList << " $" << hex4 << (int)(musics[i].mySamples[j]);
				if (j != musics[i].mySamples.size() - 1)
					songSampleList << ",";
			}
		}
	}

	songSampleList << "\nSGEnd:";

	writeTextFile("asm/SNES/SongSampleList.asm", [&] {
		std::stringstream tempstream;
		tempstream << "org $" << hex6 << PCToSNES(findFreeSpace(songSampleListSize, bankStart, rom)) << "\n\n\n";
		return tempstream.str() + songSampleList.str();
	});
}

void fixMusicPointers() {
	if (verbose)
		std::cout << "Fixing song pointers...\n";

	// int pointersPos = programSize + 0x400;
	std::stringstream globalPointers;
	std::stringstream incbins;

	auto songDataARAMPos = static_cast<uint16_t>(programSize + programPos + highestGlobalSong * 2 + 2);		// // //
	//                    size + startPos + pointer to each global song + pointer to local song.
	//int songPointerARAMPos = programSize + programPos;

	bool addedLocalPtr = false;

	for (auto &music : musics) if (music.exists) {		// // //
		const bool isGlobal = (music.index <= highestGlobalSong);

		music.posInARAM = songDataARAMPos;
		music.adjustHeaderPointers();		// // //
		music.adjustLoopPointers();		// // //

		int sizeWithPadding = (music.minSize > 0) ? music.minSize : music.totalSize;

		auto finalData = music.getSongData();		// // //
		if (isGlobal && music.minSize > 0 && finalData.size() < music.minSize)
			finalData.resize(music.minSize);

		if (!isGlobal) {
			std::vector<uint8_t> header(0x0C);
			int RATSSize = music.totalSize + 4 - 1;
			assign_val<4>(header.begin(), 0x52415453); // "STAR"
			assign_short(header.begin() + 4, RATSSize);
			assign_short(header.begin() + 6, ~RATSSize);
			assign_short(header.begin() + 8, sizeWithPadding);
			assign_short(header.begin() + 10, songDataARAMPos);
			music.finalData = finalData;
			finalData.insert(finalData.cbegin(), header.cbegin(), header.cend());
		}

		std::stringstream fname;
		fname << "asm/SNES/bin/music" << hex2 << music.index << ".bin";
		writeFile(fname.str(), finalData);

		if (isGlobal)
			songDataARAMPos += static_cast<uint16_t>(sizeWithPadding);		// // //
		else if (checkEcho) {
			auto &info = music.spaceInfo;		// // //
			info.songStartPos = songDataARAMPos;
			info.songEndPos = info.songStartPos + sizeWithPadding;

			int checkPos = (songDataARAMPos + sizeWithPadding + 0xFF) / 0x100 * 0x100;		// // //

			info.sampleTableStartPos = checkPos;
			checkPos += music.mySamples.size() * 4;
			info.sampleTableEndPos = checkPos;

			for (auto thisSample : music.mySamples) {		// // //
				int thisSampleSize = samples[thisSample].data.size();
				bool sampleIsImportant = samples[thisSample].important;
				info.individualSamples.push_back({checkPos, checkPos + thisSampleSize, sampleIsImportant});		// // //
				checkPos += thisSampleSize;
			}

			//info.echoBufferStartPos = checkPos;
			checkPos = (checkPos + (music.echoBufferSize << 11) + 0xFF) / 0x100 * 0x100;		// // //
			//info.echoBufferEndPos = checkPos;

			if (music.echoBufferSize > 0) {
				info.echoBufferStartPos = 0x10000 - (music.echoBufferSize << 11);
				info.echoBufferEndPos = 0x10000;
			}
			else {
				info.echoBufferStartPos = 0xFF00;
				info.echoBufferEndPos = 0xFF04;
			}

			if (checkPos > 0x10000) {
				std::stringstream ss;		// // //
				ss << music.getFileName() << ":\nEcho buffer exceeded total space in ARAM by 0x" <<
					hex4 << checkPos - 0x10000 << " bytes.";
				fatalError(ss.str());
			}
		}

		if (isGlobal) {
			globalPointers << "\ndw song" << hex2 << music.index;
			incbins << "song" << hex2 << music.index << ": incbin \"" << "SNES/bin/" << "music" << hex2 << music.index << ".bin\"\n";
		}
		else if (!std::exchange(addedLocalPtr, true)) {
			globalPointers << "\ndw localSong";
			incbins << "localSong: ";
		}
	}

	if (recompileMain) {
		if (verbose)
			std::cout << "Compiling main SPC program, final pass.\n";

		std::string old = openTextFile("asm/tempmain.asm");		// // //
		writeTextFile("asm/tempmain.asm", [&] {
			return old + globalPointers.str() + "\n" + incbins.str();
		});

		//removeFile("asm/SNES/bin/main.bin");

		//execute("asar asm/tempmain.asm asm/SNES/bin/main.bin 2> temp.log > temp.txt");

		//if (fileExists("temp.log"))
		if (!asarCompileToBIN("asm/tempmain.asm", "asm/SNES/bin/main.bin"))
			fatalError("asar reported an error while assembling asm/main.asm. Refer to temp.log for\ndetails.\n");		// // //
	}

	std::vector<uint8_t> temp = openFile("asm/SNES/bin/main.bin");		// // //
	programSize = temp.size();		// // //
	std::vector<uint8_t> temp2(4);		// // //
	assign_short(temp2.begin(), programSize);
	assign_short(temp2.begin() + 2, programPos);
	temp2.insert(temp2.cend(), temp.cbegin(), temp.cend());
	writeFile("asm/SNES/bin/main.bin", temp2);

	if (verbose)
		std::cout << "Total space in ARAM left for local songs: 0x" << hex4 << (0x10000 - programSize - 0x400) << " bytes." << std::dec << '\n';

	// Can't do this now since we can't get a sample correctly outside of a song.

	/*
	int defaultIndex = -1, optimizedIndex = -1;
	for (unsigned int i = 0; i < bankDefines.size(); i++) {
		if (bankDefines[i].name == "default")
			defaultIndex = i;
		if (bankDefines[i].name == "optimized")
			optimizedIndex = i;
	}

	if (defaultIndex != -1)
	{
		int groupSize = 0;
		for (unsigned int i = 0; i < bankDefines[defaultIndex]->samples.size(); i++)
		{
			int j = getSample(File(*(bankDefines[defaultIndex]->samples[i])));
			if (j == -1) goto end1;
			groupSize += samples[j].data.size() + 4;
		}

		std::cout << "Total space in ARAM for local songs using #default: 0x" << hex4 << (0x10000 - programSize - 0x400 - groupSize) << " bytes." << std::dec << '\n';
	}
end1:

	if (optimizedIndex != -1)
	{
		int groupSize = 0;
		for (unsigned int i = 0; i < bankDefines[optimizedIndex]->samples.size(); i++)
		{
			int j = getSample(File(*(bankDefines[optimizedIndex]->samples[i])));
			if (j == -1) goto end2;
			groupSize += samples[j].data.size() + 4;
		}

		std::cout << "Total space in ARAM for local songs using #optimized: 0x" << hex4 << (0x10000 - programSize - 0x400 - groupSize) << " bytes." << std::dec << '\n';
	}
end2:;
	*/
}

void generateSPCs() {
	if (checkEcho == false)		// If echo buffer checking is off, then the overflow may be due to too many samples.
		return;			// In this case, trying to generate an SPC would crash.
	//byte base[0x10000];

	std::vector<uint8_t> programData = openFile("asm/SNES/bin/main.bin");		// // //
	programData.erase(programData.begin(), programData.begin() + 4);	// Erase the upload data.

	unsigned int localPos = programData.size() + programPos;

	//for (i = 0; i < programPos; i++) base[i] = 0;

	//for (i = 0; i < programSize; i++)
	//	base[i + programPos] = programData[i];

	enum : size_t
	{
		PC = 0x25,
		TITLE = 0x2E,
		GAME = 0x4E,
		DUMPER = 0x6E,
		COMMENT = 0x7E,
		DATE = 0x9E,
		LENGTH = 0xA9,
		FADEOUT = 0xAC,
		AUTHOR = 0xB1,
		RAM = 0x100,
		DSP_ADDR = RAM + 0x10000,
		SPC_SIZE = DSP_ADDR + 0x100,
	};

	std::vector<uint8_t> SPCBase = openFile("asm/SNES/SPCBase.bin");
	std::vector<uint8_t> DSPBase = openFile("asm/SNES/SPCDSPBase.bin");
	SPCBase.resize(SPC_SIZE);

	int SPCsGenerated = 0;

	enum spc_mode_t {MUSIC, SFX1, SFX2};		// // //

	const auto makeSPCfn = [&] (int i, const spc_mode_t mode, bool yoshi) {		// // //
		std::vector<uint8_t> SPC = SPCBase;		// // //

		if (mode == MUSIC) {		// // //
			std::copy_n(musics[i].title.cbegin(), std::min<unsigned>(32u, musics[i].title.size()), SPC.begin() + TITLE);
			std::copy_n(musics[i].game.cbegin(), std::min<unsigned>(32u, musics[i].game.size()), SPC.begin() + GAME);
			std::copy_n(musics[i].comment.cbegin(), std::min<unsigned>(32u, musics[i].comment.size()), SPC.begin() + COMMENT);
			std::copy_n(musics[i].author.cbegin(), std::min<unsigned>(32u, musics[i].author.size()), SPC.begin() + AUTHOR);
			std::copy_n(musics[i].dumper.cbegin(), std::min<unsigned>(16u, musics[i].dumper.size()), SPC.begin() + DUMPER);		// // //
		}

		std::copy_n(programData.cbegin(), programSize, SPC.begin() + RAM + programPos);

		int backupIndex = i;
		if (mode == MUSIC)		// // //
			std::copy(musics[i].finalData.cbegin(), musics[i].finalData.cend(), SPC.begin() + RAM + localPos);		// // //
		else
			i = highestGlobalSong + 1;		// While dumping SFX, pretend that the current song is the lowest local song

		int tablePos = localPos + musics[i].finalData.size();
		if ((tablePos & 0xFF) != 0)
			tablePos = (tablePos & 0xFF00) + 0x100;

		auto samplePos = static_cast<uint16_t>(tablePos + musics[i].mySamples.size() * 4);		// // //
		auto srcTable = SPC.begin() + RAM + tablePos;

		for (const auto &id : musics[i].mySamples) {		// // //
			const auto &samp = samples[id];
			unsigned short loopPoint = samp.loopPoint;
			unsigned short newLoopPoint = loopPoint + samplePos;

			assign_short(srcTable, samplePos);		// // //
			assign_short(srcTable + 2, newLoopPoint);
			srcTable += 4;

			std::copy(samp.data.cbegin(), samp.data.cend(), SPC.begin() + RAM + samplePos);
			samplePos += static_cast<uint16_t>(samp.data.size());
		}

		std::copy(DSPBase.cbegin(), DSPBase.cend(), SPC.begin() + DSP_ADDR);		// // //

		SPC[DSP_ADDR + 0x5D] = static_cast<uint8_t>(tablePos >> 8); // sample directory

		SPC[RAM + 0x5F] = 0x20;

		SPC[LENGTH    ] = static_cast<uint8_t>(musics[i].seconds / 100 % 10) + '0';		// Why on Earth is the value stored as plain text...?
		SPC[LENGTH + 1] = static_cast<uint8_t>(musics[i].seconds / 10 % 10) + '0';
		SPC[LENGTH + 2] = static_cast<uint8_t>(musics[i].seconds / 1 % 10) + '0';

		SPC[FADEOUT    ] = '1';
		SPC[FADEOUT + 1] = '0';
		SPC[FADEOUT + 2] = '0';
		SPC[FADEOUT + 3] = '0';
		SPC[FADEOUT + 4] = '0';

		assign_short(SPC.begin() + PC, programUploadPos);		// // // Set the PC to the main loop.

		i = backupIndex;

		switch (mode) {		// // //
		case MUSIC: SPC[RAM + 0xF6] = static_cast<uint8_t>(highestGlobalSong + 1); break;	// Tell the SPC to play this song.
		case SFX1:  SPC[RAM + 0xF4] = static_cast<uint8_t>(i); break;						// Tell the SPC to play this SFX
		case SFX2:  SPC[RAM + 0xF7] = static_cast<uint8_t>(i); break;						// Tell the SPC to play this SFX
		}
		if (yoshi)		// // //
			SPC[RAM + 0xF5] = 2;

		std::stringstream timeField;		// // //
		std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm time;
#ifdef _MSVC_LANG
		::localtime_s(&time, &t);
#else
		//::localtime_s(&t, &time);
		::localtime_r(&t, &time);
#endif
		timeField << std::put_time(&time, "%m/%d/%Y");
		auto timeStr = timeField.str();
		std::copy_n(timeStr.cbegin(), std::min<unsigned>(10u, timeStr.size()), SPC.begin() + DATE);

		auto fname = fs::path {mode == SFX1 ? soundEffects[0][i].name :
			mode == SFX2 ? soundEffects[1][i].name : musics[i].getFileName()}.stem();		// // //
		switch (mode) {
		case MUSIC: break;
		case SFX1: fname = "1DF9" / fname; break;
		case SFX2: fname = "1DFC" / fname; break;
		}

		fname = "SPCs" / fname;
		if (yoshi)		// // //
			fname.replace_filename(fname.filename().string() + " (Yoshi)");
		fname.replace_extension(".spc");

		if (verbose)
			std::cout << "Wrote \"" << fname << "\" to file.\n";

		writeFile(fname, SPC);
		++SPCsGenerated;		// // //
	};

//	std::time_t recentMod = getLastModifiedTime();		// // // If any main program modifications were made, we need to update all SPCs.

	// Cannot generate SPCs for global songs as required samples, SRCN table, etc. cannot be determined.
	for (int i = highestGlobalSong + 1; i < 256; ++i)		// // //
		if (musics[i].exists) {
			//time_t spcTimeStamp = getTimeStamp((File)fname);

			/*if (!forceSPCGeneration)
				if (fileExists(fname))
				if (getTimeStamp((File)("music/" + musics[i].name)) < spcTimeStamp)
				if (getTimeStamp((File)"./samples") < spcTimeStamp)
				if (recentMod < spcTimeStamp)
				continue;*/
			if (musics[i].hasYoshiDrums)
				makeSPCfn(i, MUSIC, true);
			makeSPCfn(i, MUSIC, false);
		}

	if (sfxDump)
		for (int i = 0; i < 256; i++) {		// // //
			if (soundEffects[0][i].exists)
				makeSPCfn(i, SFX1, false);
			if (soundEffects[1][i].exists)
				makeSPCfn(i, SFX2, false);
		}

	if (verbose) {
		if (SPCsGenerated == 1)
			std::cout << "Generated 1 SPC file.\n";
		else if (SPCsGenerated > 0)
			std::cout << "Generated " << SPCsGenerated << " SPC files.\n";
	}
}

void assembleSNESDriver2() {
	if (verbose)
		std::cout << "\nGenerating SNES driver...\n\n";

	std::string patch = openTextFile("asm/SNES/patch.asm");		// // //

	insertValue(reuploadPos, 4, "!ExpARAMRet = ", patch);
	insertValue(SRCNTableCodePos, 4, "!TabARAMRet = ", patch);
	insertValue(mainLoopPos, 4, "!DefARAMRet = ", patch);
	insertValue(songCount, 2, "!SongCount = ", patch);
	insertValue(highestGlobalSong, 2, "!GlobalMusicCount = #", patch);

	//removeFile("asm/SNES/temppatch.sfc");

	//writeTextFile("asm/SNES/temppatch.asm", patch);

	//execute("asar asm/SNES/temppatch.asm 2> temp.log");
	//if (fileExists("temp.log"))
	//{
	//	std::cout << "asar reported an error assembling patch.asm. Refer to temp.log for details.\n";
	//	quit(1);
	//}

	//std::vector<uint8_t> patchBin;		// // //
	//openFile("asm/SNES/temppatch.sfc", patchBin);

	size_t pos = patch.find("MusicPtrs:");		// // //
	if (pos == std::string::npos)
		fatalError("Error: \"MusicPtrs:"" could not be found.");
	patch = patch.substr(0, pos) + openTextFile("asm/SNES/patch2.asm");		// // //

	std::stringstream musicPtrStr {"MusicPtrs: \ndl "};		// // //
	std::stringstream samplePtrStr {"\n\nSamplePtrs:\ndl "};
	std::stringstream sampleLoopPtrStr {"\n\nSampleLoopPtrs:\ndw "};
	std::stringstream musicIncbins {"\n\n"};
	std::stringstream sampleIncbins {"\n\n"};

	if (verbose)
		std::cout << "Writing music files...\n";

	for (int i = 0; i < songCount; i++) {
		if (musics[i].exists == true && i > highestGlobalSong) {
			std::stringstream musicBinPath;
			musicBinPath << "asm/SNES/bin/music" << hex2 << i << ".bin";
			unsigned requestSize = static_cast<unsigned>(fs::file_size(musicBinPath.str()));		// // //
			int freeSpace = findFreeSpace(requestSize, bankStart, rom);
			if (freeSpace == -1)
				fatalError("Error: Your ROM is out of free space.");

			freeSpace = PCToSNES(freeSpace);
			musicPtrStr << "music" << hex2 << i << "+8";
			musicIncbins << "org $" << hex6 << freeSpace << "\nmusic" << hex2 << i << ": incbin \"bin/music" << hex2 << i << ".bin\"\n";
		}
		else {
			musicPtrStr << "$" << hex6 << 0;
		}

		if ((i & 0xF) == 0xF && i != songCount - 1)
			musicPtrStr << "\ndl ";
		else if (i != songCount - 1)
			musicPtrStr << ", ";
	}

	if (verbose)
		std::cout << "Writing sample files...\n";

	for (size_t i = 0, n = samples.size(); i < n; ++i) {		// // //
		if (samples[i].exists) {
			const size_t ssize = samples[i].data.size();		// // //
			std::vector<uint8_t> temp {
				'S', 'T', 'A', 'R',
				static_cast<uint8_t>((ssize + 1) & 0xFF), static_cast<uint8_t>((ssize + 1) >> 8),
				static_cast<uint8_t>(~(ssize + 1) & 0xFF), static_cast<uint8_t>(~(ssize + 1) >> 8),
				static_cast<uint8_t>(ssize & 0xFF), static_cast<uint8_t>(ssize >> 8),
			};
			temp.insert(temp.cend(), samples[i].data.cbegin(), samples[i].data.cend());		// // //

			std::stringstream filename;
			filename << "asm/SNES/bin/brr" << hex2 << i << ".bin";
			writeFile(filename.str(), temp);

			unsigned requestSize = static_cast<unsigned>(fs::file_size(filename.str()));		// // //
			int freeSpace = findFreeSpace(requestSize, bankStart, rom);
			if (freeSpace == -1)
				fatalError("Error: Your ROM is out of free space.");

			freeSpace = PCToSNES(freeSpace);
			samplePtrStr << "brr" << hex2 << i << "+8";
			sampleIncbins << "org $" << hex6 << freeSpace << "\nbrr" << hex2 << i << ": incbin \"bin/brr" << hex2 << i << ".bin\"\n";

		}
		else
			samplePtrStr << "$" << hex6 << 0;

		sampleLoopPtrStr << "$" << hex4 << samples[i].loopPoint;

		if ((i & 0xF) == 0xF && i != n - 1) {
			samplePtrStr << "\ndl ";
			sampleLoopPtrStr << "\ndw ";
		}
		else if (i != n - 1) {
			samplePtrStr << ", ";
			sampleLoopPtrStr << ", ";
		}
	}

	removeFile("asm/SNES/temppatch.sfc");		// // //

	writeTextFile("asm/SNES/temppatch.asm", [&] {
		return openTextFile("asm/SNES/AMUndo.asm") + patch + "pullpc\n\n" +
			musicPtrStr.str() + "\ndl $FFFFFF\n" +
			samplePtrStr.str() + "\ndl $FFFFFF\n" +
			sampleLoopPtrStr.str() + musicIncbins.str() + sampleIncbins.str() +
			"\n\norg !SPCProgramLocation\nincbin \"bin/main.bin\"";		// // //
	});

	if (verbose)
		std::cout << "Final compilation...\n";

	if (!doNotPatch) {

		//execute("asar asm/SNES/temppatch.asm asm/SNES/temp.sfc 2> temp.log");
		//if (fileExists("temp.log"))
		if (!asarPatchToROM("asm/SNES/temppatch.asm", "asm/SNES/temp.sfc"))
			fatalError("asar reported an error.  Refer to temp.log for details.");		// // //

		//execute("asar asm/SNES/tweaks.asm asm/SNES/temp.sfc 2> temp.log");
		//if (fileExists("temp.log"))
		//	fatalError("asar reported an error.  Refer to temp.log for details.");

		//execute("asar asm/SNES/NMIFix.asm asm/SNES/temp.sfc 2> temp.log");
		//if (fileExists("temp.log"))
		//	fatalError("asar reported an error.  Refer to temp.log for details.");

		std::vector<uint8_t> final = romHeader;		// // //

		std::vector<uint8_t> tempsfc = openFile("asm/SNES/temp.sfc");		// // //
		final.insert(final.cend(), tempsfc.cbegin(), tempsfc.cend());		// // //

		removeFile(ROMName.string() + "~");		// // //
		fs::rename(ROMName, ROMName.string() + "~");

		writeFile(ROMName, final);
	}
}

void generateMSC() {
	writeTextFile(ROMName.replace_extension(".msc"), [] {		// // //
		std::stringstream text;
		for (const auto &x : musics)
			if (x.exists) {
				text << hex2 << x.index << "\t" << 0 << "\t" << x.title << "\n";		// // //
				text << hex2 << x.index << "\t" << 1 << "\t" << x.title << "\n";
			}
		return text.str();
	});
}

void cleanUpTempFiles() {
	if (doNotPatch)		// If this is specified, then the user might need these temp files.  Keep them.
		return;

	removeFile("asm/tempmain.asm");
	removeFile("asm/main.bin");
	removeFile("asm/SFX1DF9Table.bin");
	removeFile("asm/SFX1DFCTable.bin");
	removeFile("asm/SFXData.bin");

	removeFile("asm/SNES/temppatch.asm");
	removeFile("asm/SNES/temp.sfc");
	removeFile("temp.log");
	removeFile("temp.txt");
}

void tryToCleanSampleToolData() {
	const char HEADER_STR[] = "New Super Mario World Sample Utility 2.0 by smkdan";		// // //
	auto it = std::search(rom.cbegin(), rom.cend(), std::cbegin(HEADER_STR), std::cend(HEADER_STR));
	if (it == rom.cend())
		return;

	std::cout << "Sample Tool detected.  Erasing data...\n";

	unsigned int i = std::distance(rom.cbegin(), it);
	int hackPos = i - 8;

	i += 0x36;

	int sizeOfErasedData = 0;

	bool removed[0x100] = { };		// // //
	for (int j = 0; j < 0x207; j++) {
		if (removed[rom[j + i]]) continue;
		sizeOfErasedData += clearRATS(rom, SNESToPC(rom[j + i] * 0x10000 + 0x8000));		// // //
		removed[rom[j + i]] = true;
	}

	int sampleDataSize = sizeOfErasedData;

	sizeOfErasedData += clearRATS(rom, hackPos);		// // //

	std::cout << "Erased 0x" << hex6 << sizeOfErasedData << " bytes, of which 0x" << sampleDataSize << " were sample data.";
}

void tryToCleanAM4Data() {
	if ((rom.size() % 0x8000 != 0 && rom[0x1940] == 0x22) || (rom.size() % 0x8000 == 0 && rom[0x1740] == 0x22)) {
		if (rom.size() % 0x8000 == 0)
			fatalError("Addmusic 4.05 ROMs can only be cleaned if they have a header. This does not\n"
					   "apply to any other aspect of the program.");		// // //

		std::cout << "Attempting to erase data from Addmusic 4.05:\n";
		std::string ROMstr = ROMName.string();		// // //
		char blank = '\0';
		char *am405argv[] = {&blank, const_cast<char *>(ROMstr.c_str())};
		removeAM405Data(2, am405argv);

		rom = openFile(ROMName);		// // // Reopen the file.
		if (rom[0x255] == 0x5C) {
			int moreASMData = ((rom[0x255 + 3] << 16) | (rom[0x255 + 2] << 8) | (rom[0x255 + 1])) - 8;
			clearRATS(rom, SNESToPC(moreASMData));		// // //
		}
		int romiSPCProgramAddress = (unsigned char)rom[0x2E9] | ((unsigned char)rom[0x2EE] << 8) | ((unsigned char)rom[0x2F3] << 16);
		clearRATS(rom, SNESToPC(romiSPCProgramAddress) - 12 + 0x200);		// // //
	}
}

void tryToCleanAMMData() {
	if ((rom.size() % 0x8000 != 0 && rom[0x078200] == 0x53) || (rom.size() % 0x8000 == 0 && rom[0x078000] == 0x53)) {		// Since RATS tags only need to be in banks 0x10+, a tag here signals AMM usage.
		if (rom.size() % 0x8000 == 0)
			fatalError("AddmusicM ROMs can only be cleaned if they have a header. This does not\n"
					   "apply to any other aspect of the program.");		// // //

		if (!fs::exists("INIT.asm"))		// // //
			fatalError("AddmusicM was detected.  In order to remove it from this ROM, you must put\n"
					   "AddmusicM's INIT.asm as well as xkasAnti and a clean ROM (named clean.smc) in\n"
					   "the same folder as this program. Then attempt to run this program once more.");

		std::cout << "AddmusicM detected.  Attempting to remove...\n";
		execute("perl addmusicMRemover.pl " + ROMName.string(), false);		// // //
		execute("xkasAnti clean.smc " + ROMName.string() + " INIT.asm");
	}
}

// // // moved
int SNESToPC(int addr) {			// Thanks to alcaro.
	if (addr < 0 || addr > 0xFFFFFF ||		// not 24bit 
		(addr & 0xFE0000) == 0x7E0000 ||	// wram 
		(addr & 0x408000) == 0x000000)		// hardware regs 
		return -1;
	if (usingSA1 && addr >= 0x808000)
		addr -= 0x400000;
	return ((addr & 0x7F0000) >> 1 | (addr & 0x7FFF));
}

// // // moved
int PCToSNES(int addr) {
	if (addr < 0 || addr >= 0x400000)
		return -1;
	addr = ((addr << 1) & 0x7F0000) | (addr & 0x7FFF) | 0x8000;
	if ((addr & 0xF00000) == 0x700000)
		addr |= 0x800000;
	if (usingSA1 && addr >= 0x400000)
		addr += 0x400000;
	return addr;
}

void checkMainTimeStamps()			// Disabled for now, as this only works if the ROM is linked to the program (so it wouldn't work if the program was used on multiple ROMs)
{						// It didn't save much time anyway...
	recompileMain = true;
	return;

/*
	if (!fs::exists("asm/SNES/bin/main.bin") || 0 != ::strncmp((char *)(rom.data() + 0x70000), "@AMK", 4)) {		// // //
		recompileMain = true;
		return;
	}

	mostRecentMainModification = std::max(mostRecentMainModification, getLastModifiedTime());		// // //
	mostRecentMainModification = std::max(mostRecentMainModification, getTimeStamp("asm/SNES/patch.asm"));
	mostRecentMainModification = std::max(mostRecentMainModification, getTimeStamp("asm/SNES/patch2.asm"));
	mostRecentMainModification = std::max(mostRecentMainModification, getTimeStamp("asm/SNES/tweaks.asm"));
	mostRecentMainModification = std::max(mostRecentMainModification, getTimeStamp("Addmusic_list.txt"));

	if (recompileMain = (mostRecentMainModification > getTimeStamp("asm/SNES/bin/main.bin")))
		std::cout << "Changes have been made to the global program.  Recompiling...\n\n";
*/
}

// // //
/*
std::time_t getLastModifiedTime() {
	std::time_t recentMod = 0;			// If any main program modifications were made, we need to update all SPCs.
	for (int i = 1; i <= highestGlobalSong; i++)
		recentMod = std::max(recentMod, getTimeStamp(fs::path("music") / musics[i].getFileName()));		// // //

	recentMod = std::max(recentMod, getTimeStamp("asm/main.asm"));
	recentMod = std::max(recentMod, getTimeStamp("asm/commands.asm"));
	recentMod = std::max(recentMod, getTimeStamp("asm/InstrumentData.asm"));
	recentMod = std::max(recentMod, getTimeStamp("asm/CommandTable.asm"));
	recentMod = std::max(recentMod, getTimeStamp("Addmusic_sound effects.txt"));
	recentMod = std::max(recentMod, getTimeStamp("Addmusic_sample groups.txt"));
	recentMod = std::max(recentMod, getTimeStamp("AddmusicK.exe"));

	for (int i = 1; i < 256; i++) {		// // //
		if (soundEffects[0][i].exists)
			recentMod = std::max(recentMod, getTimeStamp(fs::path("1DF9") / soundEffects[0][i].getEffectiveName()));
		if (soundEffects[1][i].exists)
			recentMod = std::max(recentMod, getTimeStamp(fs::path("1DFC") / soundEffects[1][i].getEffectiveName()));
	}

	return recentMod;
}
*/

void generatePNGs()
{
	/*
	const int width = 1024;
	const int height = 64;

	for (const auto &current : musics) if (current.index > highestGlobalSong && current.exists) {
		const auto &info = current.spaceInfo;

		uint32_t bitmap[height * width];		// // //
		std::fill(bitmap + info.echoBufferEndPos, std::end(bitmap), 0xFF3F3F3F); // grey
		std::fill(bitmap + info.echoBufferStartPos, bitmap + info.echoBufferEndPos, 0xFFA000A0); // purple
		int currentSampleIndex = 0;
		int sampleCount = info.individualSamples.size();
		for (const auto &samp : info.individualSamples) {
			auto intensity = static_cast<uint8_t>(128. + 127. * currentSampleIndex / sampleCount);
			std::fill(bitmap + samp.startPosition, bitmap + samp.endPosition,
					  0xFF000000 | intensity * (samp.important ? 0x00010100 : 0x00010000); // cyan or blue
			++currentSampleIndex;
		}
		std::fill(bitmap + info.sampleTableStartPos, bitmap + info.sampleTableEndPos, 0xFF00FF00); // lime
		std::fill(bitmap + info.songStartPos, bitmap + info.songEndPos, 0xFF008000); // green
		std::fill(bitmap + programUploadPos, bitmap + programPos + programSize, 0XFF00FFFF); // yellow
		std::fill(bitmap, bitmap + programUploadPos, 0XFF0000FF); // red

		lodepng::encode("Visualizations" / fs::path {current.getFileName()}.stem().replace_extension(".png"), bitmap, width, height);
	}
	*/
}
