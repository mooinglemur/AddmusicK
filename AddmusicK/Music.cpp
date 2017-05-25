#include "Music.h"
//#include "globals.h"
//#include "Sample.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <locale>		// // //

#include "globals.h"		// // //
#include "Utility/Exception.h"		// // //
#include "Binary/Defines.h"		// // //
#include "MML/Lexer.h"		// // //
#include "MML/Preprocessor.h"		// // //
#include <functional>

//#include "Preprocessor.h"


// // //

#define CMD_ERROR(name, abbr) "Error parsing " name " (\"" abbr "\") command."
#define CMD_ILLEGAL(name, abbr) "Illegal value for " name " (\"" abbr "\") command."
#define DIR_ERROR(name) "Error parsing " name " directive."
#define DIR_ILLEGAL(name) "Illegal value for " name " directive."

template <typename T, typename U, typename V>
decltype(auto) apply_this(T &&fn, U pObj, V &&tup) {
	return std::apply(std::forward<T>(fn), std::tuple_cat(std::make_tuple(pObj), std::forward<V>(tup)));
}

template <typename T, typename U>
T requires(T x, T min, T max, U&& err) {
	return (x >= min && x <= max) ? x :
		throw AMKd::Utility::ParamException {std::forward<U>(err)};
}

#define warn(str) if (false); else \
	::printWarning(str, name, mml_.GetLineNumber())

#define error(str) do { \
		::printError(str, name, mml_.GetLineNumber()); \
		return; \
	} while (false)

// // //
void Music::fatalError(const std::string &str) {
	::fatalError(str, name, mml_.GetLineNumber());		// // //
}

static int channel, prevChannel, octave, prevNoteLength, defaultNoteLength;
static bool inDefineBlock;
static bool inNormalLoop;		// // //

static unsigned int prevLoop;
static bool doesntLoop;
static bool triplet;
static bool inPitchSlide;

using AMKd::MML::Target;		// // //
static Target songTargetProgram = Target::Unknown;
static int targetAMKVersion;

static int loopLabel = 0;

static const int tmpTrans[19] = { 0, 0, 5, 0, 0, 0, 0, 0, 0, -5, 6, 0, -5, 0, 0, 8, 0, 0, 0 };
static const int instrToSample[30] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x07, 0x08, 0x09, 0x05, 0x0A,	// \ Instruments
0x0B, 0x01, 0x10, 0x0C, 0x0D, 0x12, 0x0C, 0x11, 0x01,		// /
0x00, 0x00,							// Nothing
0x0F, 0x06, 0x06, 0x0E, 0x0E, 0x0B, 0x0B, 0x0B, 0x0E };		// Percussion

static const int hexLengths[] = {
	                              2, 2, 3, 4, 4, 1,
	2, 3, 2, 3, 2, 4, 2, 2, 3, 4, 2, 4, 4, 3, 2, 4,
	1, 4, 4, 3, 2, 9, 3, 4, 2, 3, 3, 2, 5
};
static int transposeMap[256];
//static bool htranspose[256];
static int hTranspose;
static bool usingHTranspose;
static int hexLeft = 0;
static int currentHex = 0;

//static int tempLoopLength;		// How long the current [ ] loop is.
//static int e6LoopLength;		// How long the current $E6 loop is.
//static int previousLoopLength;	// How long the last encountered loop was.
static bool inE6Loop;			// Whether or not we're in an $E6 loop.
static int loopNestLevel;		// How deep we're "loop nested".
// If this is 0, then any type of loop is allowed.
// If this is 1 and we're in a normal loop, then normal loops are disallowed and $E6 loops are allowed.
// If this is 1 and we're in an $E6 loop, then $E6 loops are disallowed and normal loops are allowed.
// If this is 2, then no new loops are allowed.

//static unsigned int lengths[CHANNELS];		// How long each channel is.

static unsigned int tempo;
//static bool onlyHadOneTempo;
static bool tempoDefined;

static bool manualNoteWarning;

static bool channelDefined;
//static int am4silence;			// Used to keep track of the brief silence at the start of am4 songs.

//static bool normalLoopInsideE6Loop;
//static bool e6LoopInsideNormalLoop;

static std::string basepath;

static bool usingSMWVTable;

//static bool songVersionIdentified;
//static int hTranspose = 0;00

// // // vs2017 does not have fold-expressions
#define SWALLOW(x) do { \
		using _swallow = int[]; \
		(void)_swallow {0, ((x), 0)...}; \
	} while (false)



// // //
template <typename... Args>
void Music::append(Args&&... value) {
#if 0
	(tracks[::channel].data.push_back(static_cast<uint8_t>(value)), ...);
#else
	SWALLOW(tracks[::channel].data.push_back(static_cast<uint8_t>(value)));
#endif
}

// // //
bool Music::trimChar(char c) {
	return mml_.Trim(c).has_value();
}

// // //
bool Music::trimDirective(std::string_view str) {
	return mml_.Trim(str, true).has_value();
}

// // //
void Music::skipSpaces() {
	mml_.SkipSpaces();
}

Music::Music() {
	knowsLength = false;
	totalSize = 0;
	spaceForPointersAndInstrs = 0;
	echoBufferSize = 0;
}

void Music::init() {
	basepath = "./";
	prevChannel = 0;
	manualNoteWarning = true;
	tempoDefined = false;
	//am4silence = 0;
	//songVersionIdentified = false;
	// // //
	hasYoshiDrums = false;
	//onlyHadOneTempo = true;
	tempo = 0x36;
	guessLength = true;

	channelDefined = false;
	tempoRatio = 1;

	// // //
	//remoteDefinitionArg = 0;
	inRemoteDefinition = false;
	inNormalLoop = false;		// // //

	superLoopLength = normalLoopLength = 0;

	baseLoopIsNormal = baseLoopIsSuper = extraLoopIsNormal = extraLoopIsSuper = false;
	// // //

	inE6Loop = false;
	seconds = 0;

	hTranspose = 0;
	usingHTranspose = false;
	// // //
	octave = 4;
	prevNoteLength = -1;
	defaultNoteLength = 8;

	prevLoop = -1;
	// // //

	inDefineBlock = false;

	hasIntro = false;
	doesntLoop = false;
	triplet = false;
	inPitchSlide = false;

	loopLabel = 0;
	currentHex = 0;
	hexLeft = 0;

	for (int z = 0; z < 256; z++) {
		transposeMap[z] = 0;
		usedSamples[z] = false;
	}

	for (int z = 0; z < 19; z++)
		transposeMap[z] = tmpTrans[z];

	title = name.substr(0, name.find_last_of('.'));		// // //
	size_t p = name.find_last_of('/');
	if (p != std::string::npos)
		title = name.substr(p + 1);
	p = name.find_last_of('\\');
	if (p != std::string::npos)
		title = name.substr(p);

	//std::string backup = text;

	const auto stat = AMKd::MML::Preprocessor {openTextFile(fs::path {"music"} / name), name};		// // //
	mml_ = AMKd::MML::SourceFile {stat.result};
	songTargetProgram = stat.target;		// // //
	targetAMKVersion = stat.version;
	if (!stat.title.empty())
		title = stat.title;

	switch (stat.target) {		// // //
	case Target::AMM:
		//songVersionIdentified = true;
		break;
	case Target::AM4:
		//songVersionIdentified = true;
		for (auto &t : tracks)		// // //
			t.ignoreTuning = true; // AM4 fix for tuning[] related stuff.
		break;
	case Target::AMK:
		/*
		targetAMKVersion = 0;
		if (backup.find('\r') != -1)
			backup = backup.insert(backup.length(), "\r\n\r\n#amk=1\r\n");		// Automatically assume that this is a song written for AMK.
		else
			backup = backup.insert(backup.length(), "\n\n#amk=1\n");
		writeTextFile(fs::path {"music"} / name, backup);
		*/
		if (targetAMKVersion > PARSER_VERSION)
			error("This song was made for a newer version of AddmusicK.  You must update to use\nthis song.");
		break;
	default:
		error("Song did not specify target program with #amk, #am4, or #amm.");
	}

	usingSMWVTable = (targetAMKVersion < 2);		// // //
	
	// // // We can't just insert this at the end due to looping complications and such.
	if (validateHex && index > highestGlobalSong && stat.firstChannel != CHANNELS) {
		channel = stat.firstChannel;
		if (targetAMKVersion > 1)
			doVolumeTable(true);
		append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::EchoBuffer, echoBufferSize);
	}

	channel = 0;
}

void Music::compile() {
	static const auto CMDS = [] {		// // //
		AMKd::Utility::Trie<void (Music::*)()> cmd;

		cmd.Insert("p", &Music::parseVibratoCommand);
		cmd.Insert("tuning", &Music::parseTransposeDirective);
		cmd.Insert("t", &Music::parseTempoCommand);
		cmd.Insert("[", &Music::parseLoopCommand);
		cmd.Insert("[[", &Music::parseSubloopCommand);
		cmd.Insert("[[[", &Music::parseErrorLoopCommand);
		cmd.Insert("]", &Music::parseLoopEndCommand);
		cmd.Insert("]]", &Music::parseSubloopEndCommand);
		cmd.Insert("]]]", &Music::parseErrorLoopEndCommand);

		return cmd;
	}();

	init();

	try {
		while (hasNextToken()) {		// // //
			if (auto token = mml_.ExtractToken(CMDS)) {		// // //
				if (hexLeft != 0)
					error("Unknown hex command.");
				(this->*(*token))();
				continue;
			}

			char ch = ::tolower((*mml_.Trim("."))[0]);		// // //

			if (hexLeft != 0 && ch != '$')
				error("Unknown hex command.");

			switch (ch) {
			case '?': parseQMarkDirective();		break;
//			case '!': parseExMarkDirective();		break;
			case '#': parseChannelDirective();		break;
			case 'l': parseLDirective();			break;
			case 'w': parseGlobalVolumeCommand();	break;
			case 'v': parseVolumeCommand();			break;
			case 'q': parseQuantizationCommand();	break;
			case 'y': parsePanCommand();			break;
			case '/': parseIntroDirective();		break;
			case 'o': parseOctaveDirective();		break;
			case '@': parseInstrumentCommand();		break;
			case '(': parseOpenParenCommand();		break;
			case '*': parseStarLoopCommand();		break;
			case '{': parseTripletOpenDirective();	break;
			case '}': parseTripletCloseDirective();	break;
			case '>': parseRaiseOctaveDirective();	break;
			case '<': parseLowerOctaveDirective();	break;
			case '&': parsePitchSlideCommand();		break;
			case '$': parseHexCommand();			break;
			case 'h': parseHDirective();			break;
			case 'n': parseNCommand();				break;
			case '"': parseReplacementDirective();	break;
			case '|':								break;		// // // no-op
			case 'c': case 'd': case 'e': case 'f': case 'g': case 'a': case 'b': case 'r': case '^':
				parseNote(ch);						break;
			case ';': parseComment();				break;		// Needed for comments in quotes
			default:
				throw AMKd::Utility::SyntaxException {"Unexpected character \"" + std::string(1, ch) + "\" found."};
			}
		}
	}
	catch (AMKd::Utility::SyntaxException &e) {
		fatalError(e.what());
	}
	catch (AMKd::Utility::MMLException &e) {
		error(e.what());
	}

	pointersFirstPass();
}

// // //
size_t Music::getDataSize() const {
	size_t x = 0;
	for (int i = 0; i < CHANNELS; ++i)
		x += tracks[i].data.size();
	return x;
}

// // //
void Music::FlushSongData(std::vector<uint8_t> &buf) const {
	buf.reserve(buf.size() + getDataSize() + tracks[CHANNELS].data.size() + allPointersAndInstrs.size());
	buf.insert(buf.end(), allPointersAndInstrs.cbegin(), allPointersAndInstrs.cend());
	for (const auto &x : tracks)
		buf.insert(buf.cend(), x.data.cbegin(), x.data.cend());
}

void Music::parseComment() {
	if (songTargetProgram == Target::AMM)		// // //
		mml_.Trim(".*?\\n");
	else
		error("Illegal use of comments. Sorry about that. Should be fixed in AddmusicK 2.");		// // //
}

void Music::printChannelDataNonVerbose(int totalSize) {
	std::cout << name << ": ";		// // //
	for (int i = 0, n = 58 - name.size(); i < n; ++i)
		std::cout.put('.');
	std::cout.put(' ');

	if (knowsLength) {
		// int s = (unsigned int)std::floor((mainLength + introLength) / (2.0 * tempo) + 0.5);
		auto seconds = static_cast<int>(std::floor((introSeconds + mainSeconds + 0.5) / 60));		// // //
		std::cout << seconds / 60 << ':' << std::setfill('0') << std::setw(2) << seconds % 60;
	}
	else
		std::cout << "?:??";
	std::cout << ", 0x" << hex4 << totalSize << std::dec << " bytes\n";
}

void Music::parseQMarkDirective() {
	using namespace AMKd::MML::Lexer;
	if (auto param = GetParameters<Int>(mml_)) {
		switch (param.get<0>()) {
		case 0: doesntLoop = true; break;
		case 1: tracks[channel].noMusic[0] = true; break;		// // //
		case 2: tracks[channel].noMusic[1] = true; break;
		default:
			error(DIR_ERROR("\"?\""));
		}
	}
	else
		doesntLoop = true;
}

void Music::parseExMarkDirective() {
	mml_.Clear();
}

void Music::parseChannelDirective() {
	if (isalpha(peek())) {		// // //
		parseSpecialDirective();
		return;
	}

	int i = getInt();		// // //
	if (i != -1) {
		channel = requires(i, 0, static_cast<int>(CHANNELS) - 1, DIR_ILLEGAL("channel"));
		tracks[CHANNELS].q = tracks[channel].q;		// // //
		tracks[CHANNELS].updateQ = tracks[channel].updateQ;
		prevNoteLength = -1;

		hTranspose = 0;
		usingHTranspose = false;
		channelDefined = true;
		/*for (int u = 0; u < CHANNELS * 2; u++)
		{
		if (htranspose[u])			// Undo what the h directive did.
		transposeMap[u] = tmpTrans[u];
		}*/
		//hTranspose = 0;
		return;
	}

	error(DIR_ERROR("channel"));
}

void Music::parseLDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		defaultNoteLength = requires(param.get<0>(), 1u, 192u, DIR_ILLEGAL("note length (\"l\")"));		// // //
	else
		error(DIR_ERROR("note length (\"l\")"));
}

void Music::parseGlobalVolumeCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doGlobalVolume(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("global volume", "w")));		// // //
	error(CMD_ERROR("global volume", "w"));		// // //
}

void Music::parseVolumeCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doVolume(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("volume", "v")));
	error(CMD_ERROR("volume", "v"));		// // //
}

void Music::parseQuantizationCommand() {
	int i = getHex();		// // //
	if (i != -1) {
		int q = requires(i, 0x00, 0x7F, CMD_ILLEGAL("quantization", "q"));		// // //

		if (inNormalLoop) {		// // //
			tracks[prevChannel].q = q;		// // //
			tracks[prevChannel].updateQ = true;
		}
		else {
			tracks[channel].q = q;
			tracks[channel].updateQ = true;
		}

		tracks[CHANNELS].q = q;
		tracks[CHANNELS].updateQ = true;
		return;
	}

	error(CMD_ERROR("quantization", "q"));		// // //
}

void Music::parsePanCommand() {
	using namespace AMKd::MML::Lexer;
	if (auto param = GetParameters<Int>(mml_)) {
		unsigned pan = requires(param.get<0>(), 0u, 20u, CMD_ILLEGAL("pan", "y"));
		if (auto panParam = GetParameters<Comma, Int, Comma, Int>(mml_)) {
			if (requires(panParam.get<0>(), 0u, 1u, CMD_ILLEGAL("pan", "y")))
				pan |= 1 << 7;
			if (requires(panParam.get<1>(), 0u, 1u, CMD_ILLEGAL("pan", "y")))
				pan |= 1 << 6;
		}
		return append(AMKd::Binary::CmdType::Pan, pan);		// // //
	}

	error(CMD_ERROR("pan", "y"));		// // //
}

void Music::parseIntroDirective() {
	if (inNormalLoop)		// // //
		error("Intro directive found within a loop.");		// // //

	if (!hasIntro)
		tempoChanges.emplace_back(tracks[channel].channelLength, -static_cast<int>(tempo));		// // //
	else		// // //
		for (auto &x : tempoChanges)
			if (x.second < 0)
				x.second = -static_cast<int>(tempo);

	hasIntro = true;
	tracks[channel].phrasePointers[1] = static_cast<uint16_t>(tracks[channel].data.size());		// // //
	prevNoteLength = -1;
	hasIntro = true;
	introLength = static_cast<unsigned>(tracks[channel].channelLength);		// // //
}

void Music::parseTempoCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doTempo(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("tempo", "t")));
	error(CMD_ERROR("tempo", "t"));
}

void Music::parseTransposeDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Sep<'['>, Int, Sep<']'>, Sep<'='>, SInt>(mml_)) {
		unsigned inst; int trsp;
		std::tie(inst, trsp) = *param;
		while (true) {
			transposeMap[requires(inst++, 0u, 0xFFu, DIR_ILLEGAL("tuning"))] = trsp;
			auto ext = GetParameters<Comma, SInt>(mml_);
			if (!ext)
				return;
			std::tie(trsp) = *ext;
		}
	}

	error(DIR_ERROR("tuning"));
}

void Music::parseOctaveDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		octave = requires(param.get<0>(), 1u, 6u, DIR_ILLEGAL("octave (\"o\")"));
	else
		error(DIR_ERROR("octave (\"o\")"));
}

void Music::parseInstrumentCommand() {
	bool direct = trimChar('@');		// // //

	int i = getInt();		// // //
	if (i != -1) {
		i = requires(i, 0x00, 0xFF, CMD_ILLEGAL("instrument", "@"));

		if ((i <= 18 || direct) || i >= 30) {
			if (convert)
				if (i >= 0x13 && i < 30)	// Change it to an HFD custom instrument.
					i = i - 0x13 + 30;

			if (optimizeSampleUsage)
				if (i < 30)
					usedSamples[instrToSample[i]] = true;
				else if ((i - 30) * 6 < static_cast<int>(instrumentData.size()))		// // //
					usedSamples[instrumentData[(i - 30) * 6]] = true;
				else
					error("This custom instrument has not been defined yet.");		// // //

			if (songTargetProgram == Target::AM4)		// // //
				tracks[channel].ignoreTuning = false;

			append(AMKd::Binary::CmdType::Inst, i);		// // //
		}

		if (i < 30)
			if (optimizeSampleUsage)
				usedSamples[instrToSample[i]] = true;

		//hTranspose = 0;
		//usingHTranspose = false;
		tracks[channel].instrument = i;		// // //
		//if (htranspose[i] == true)
		//	transposeMap[tracks[channe].instrument] = ::tmpTrans[tracks[channe].instrument];
		return;
	}

	error(CMD_ERROR("instrument", "@"));		// // //
}

void Music::parseOpenParenCommand() {
	int sampID;
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Sep<'@'>, Int>(mml_))
		sampID = instrToSample[requires(param.get<0>(), 0u, 29u, "Illegal instrument number for sample load command.")];
	else if (auto param = GetParameters<Sep<'\"'>, QString>(mml_)) {
		std::string s = basepath + param.get<0>();		// // //
		auto it = std::find(mySamples.cbegin(), mySamples.cend(), getSample(s, this));		// // //
		if (it == mySamples.cend())
			error("The specified sample was not included in this song.");
		sampID = std::distance(mySamples.cbegin(), it);
	}
	else
		return parseLabelLoopCommand();

	if (auto ext = GetParameters<Comma, Byte, Sep<')'>>(mml_))
		return doSampleLoad(sampID, ext.get<0>());
	error("Error parsing sample load command.");
}

void Music::parseLabelLoopCommand() {
	using namespace AMKd::MML::Lexer;
	if (trimChar('!')) {		// // //
		if (targetAMKVersion < 2)
			error("Unrecognized character '!'.");

		if (channelDefined) {						// A channel's been defined, we're parsing a remote 
			auto param = GetParameters<Int, Comma, SInt>(mml_);		// // //
			if (!param)
				error("Error parsing remote code setup.");

			int i = param.get<0>();
			int j = param.get<1>();
			int k = 0;
			if (j == AMKd::Binary::CmdOptionFC::Sustain || j == AMKd::Binary::CmdOptionFC::Release) {
				if (!GetParameters<Comma>(mml_))
					error("Error parsing remote code setup. Missing the third argument.");
				if (auto len = GetParameters<Byte>(mml_))
					k = len.get<0>();
				else {
					k = getNoteLength();		// // //
					if (k > 0x100)
						error("Note length specified was too large.");		// // //
					else if (k == 0x100)
						k = 0;
				}
			}

			if (!GetParameters<Sep<')'>, Sep<'['>>(mml_))
				error("Error parsing remote code setup.");

			tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
			if (loopPointers.find(i) == loopPointers.cend())		// // //
				loopPointers.insert({i, -1});
			append(AMKd::Binary::CmdType::Callback, loopPointers[i] & 0xFF, loopPointers[i] >> 8, j, k);
			return;
		}

		int i = getInt();		// // // We're outside of a channel, this is a remote call definition.
		if (i == -1)
			error("Error parsing remote code definition.");
		if (skipSpaces(), !trimChar(')'))		// // //
			error("Error parsing remote code definition.");
		if (trimChar('['))		// // //
			error("Error parsing remote code definition; the definition was missing.");

		loopLabel = i;		// // //
		inRemoteDefinition = true;
		return parseLoopCommand();
	}

	if (inNormalLoop)		// // //
		error("Nested loops are not allowed.");		// // //

	int i = getInt();		// // //
	if (i == -1)
		error("Error parsing label loop.");		// // //
	if (++i >= 0x10000)		// Needed to allow for songs that used label 0.
		error("Illegal value for loop label.");		// // //

	if (!trimChar(')'))		// // //
		error("Error parsing label loop.");

	synchronizeQ();		// // //
	loopLabel = i;

	if (trimChar('[')) {				// If this is a loop definition...
		tracks[channel].isDefiningLabelLoop = true;		// // // The rest of the code is handled in the respective function.
		return parseLoopCommand();
	}

	auto it = loopPointers.find(loopLabel);		// // // Otherwise, if this is a loop call...
	if (it == loopPointers.cend())
		error("Label not yet defined.");
	int j = getInt();		// // //
	if (j == -1)
		j = 1;
	if (j < 1 || j > 255)
		error("Invalid loop count.");		// // //

	doLoopRemoteCall(j);

	tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
	append(AMKd::Binary::CmdType::Loop, it->second & 0xFF, it->second >> 8, j);

	loopLabel = 0;
}

void Music::parseLoopCommand() {
	if (inNormalLoop)		// // //
		error("You cannot nest standard [ ] loops.");
	synchronizeQ();		// // //

	prevLoop = tracks[CHANNELS].data.size();		// // //

	prevChannel = channel;				// We're in a loop now, which is represented as channel 8.
	channel = CHANNELS;					// So we have to back up the current channel.
	inNormalLoop = true;		// // //
	tracks[CHANNELS].instrument = tracks[prevChannel].instrument;		// // //
	if (songTargetProgram == Target::AM4)
		tracks[CHANNELS].ignoreTuning = tracks[prevChannel].ignoreTuning; // More AM4 tuning stuff.  Related to the line above it.

	if (loopLabel > 0 && loopPointers.find(loopLabel) != loopPointers.cend())		// // //
		error("Label redefinition.");

	if (loopLabel > 0)
		loopPointers[loopLabel] = prevLoop;

	doLoopEnter();
}

// // //
void Music::parseSubloopCommand() {
	if (loopLabel > 0 && tracks[channel].isDefiningLabelLoop)		// // //
		error("A label loop cannot define a subloop.  Use a standard or remote loop instead.");
	return doSubloopEnter();
}

// // //
void Music::parseErrorLoopCommand() {
	fatalError("An ambiguous use of the [ and [[ loop delimiters was found (\"[[[\").  Separate\n"
	           "the \"[[\" and \"[\" to clarify your intention.");
}

void Music::parseLoopEndCommand() {
	if (!inNormalLoop)		// // //
		error("Loop end found outside of a loop.");
	synchronizeQ();		// // //

	int i = getInt();		// // //
	if (i == -1)
		i = 1;
	else if (inRemoteDefinition)
		error("Remote code definitions cannot repeat.");		// // //
	if (i < 1 || i > 255)
		error("Invalid loop count.");

	append(0);
	channel = prevChannel;
	inNormalLoop = false;		// // //

	doLoopExit(i);

	if (!inRemoteDefinition) {
		tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
		append(AMKd::Binary::CmdType::Loop, prevLoop & 0xFF, prevLoop >> 8, i);
	}
	inRemoteDefinition = false;
	loopLabel = 0;
	tracks[channel].isDefiningLabelLoop = false;		// // //
}

// // //
void Music::parseSubloopEndCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doSubloopExit(requires(param.get<0>(), 2u, 256u, CMD_ILLEGAL("subloop", "[[ ]]")));
	error(CMD_ERROR("subloop", "[[ ]]"));
}

// // //
void Music::parseErrorLoopEndCommand() {
	fatalError("An ambiguous use of the ] and ]] loop delimiters was found (\"]]]\").  Separate\n"
	           "the \"]]\" and \"]\" to clarify your intention.");
}

void Music::parseStarLoopCommand() {
	if (inNormalLoop)		// // //
		error("Nested loops are not allowed.");		// // //
	synchronizeQ();		// // //
	
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Opt<Int>>(mml_)) {
		int count = 1;
		if (auto l = param.get<0>())
			count = requires(*l, 0x01u, 0xFFu, CMD_ILLEGAL("star loop", "*"));
		doLoopRemoteCall(count);
		tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
		append(AMKd::Binary::CmdType::Loop, prevLoop & 0xFF, prevLoop >> 8, count);
		return;
	}
	error(CMD_ERROR("star loop", "*"));
}

void Music::parseVibratoCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int, Comma, Int>(mml_)) {
		unsigned delay, rate, depth;
		if (auto param2 = GetParameters<Comma, Int>(mml_)) {
			std::tie(delay, rate) = *param;
			std::tie(depth) = *param2;
		}
		else {
			delay = 0;
			std::tie(rate, depth) = *param;
		}
		return doVibrato(requires(delay, 0x00u, 0xFFu, "Illegal value for vibrato delay."),
						 requires(rate, 0x00u, 0xFFu, "Illegal value for vibrato rate."),
						 requires(depth, 0x00u, 0xFFu, "Illegal value for vibrato extent."));		// // //
	}

	error(CMD_ERROR("vibrato", "p"));		// // //
}

void Music::parseTripletOpenDirective() {
	if (triplet)		// // //
		error("Triplet on directive found within a triplet block.");
	triplet = true;
}

void Music::parseTripletCloseDirective() {
	if (!triplet)		// // //
		error("Triplet off directive found outside of a triplet block.");
	triplet = false;
}

void Music::parseRaiseOctaveDirective() {
	octave = requires(octave + 1, 1, 6, "The octave has been raised too high.");		// // //
}

void Music::parseLowerOctaveDirective() {
	octave = requires(octave - 1, 1, 6, "The octave has been dropped too low.");		// // //
}

void Music::parsePitchSlideCommand() {
	if (inPitchSlide)		// // //
		error("Pitch slide directive specified multiple times in a row.");
	inPitchSlide = true;
}

void Music::parseHFDInstrumentHack(int addr, int bytes) {
	int byteNum = 0;
	do {
		int i;		// // //
		if (!getHexByte(i))		// // //
			error("Unknown HFD hex command.");
		instrumentData.push_back(i);
		byteNum++;
		addr++;
		if (byteNum == 1) {
			if (optimizeSampleUsage)
				usedSamples[i] = true;
		}
		if (byteNum == 5) {
			instrumentData.push_back(0);	// On the 5th byte, we need to add a 0 as the new sub-multiplier.
			byteNum = 0;
		}
		bytes--;
	} while (bytes >= 0);
}

void Music::parseHFDHex() {
	int i;		// // //
	if (!getHexByte(i))		// // //
		error("Unknown HFD hex command.");

	if (convert) {
		switch (i) {
		case 0x80:
		{
			int reg, val;
			if (!getHexByte(reg) || !getHexByte(val))		// // //
				error("Error while parsing HFD hex command.");

			if (!(reg == 0x6D || reg == 0x7D))	// Do not write the HFD header hex bytes.
				if (reg == 0x6C)			// Noise command gets special treatment.
					append(AMKd::Binary::CmdType::Noise, val);		// // //
				else
					append(AMKd::Binary::CmdType::DSP, reg, val);		// // //
			else
				songTargetProgram = Target::AM4;		// // // The HFD header bytes indicate this as being an AM4 song, so it gets AM4 treatment.
		} break;
		case 0x81:
			if (!getHexByte(i))		// // //
				error("Error while parsing HFD hex command.");
			append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Transpose, i);		// // //
			break;
		case 0x82:
		{
			int addrHi, addrLo;
			int bytesHi, bytesLo;
			if (!getHexByte(addrHi) || !getHexByte(addrLo) || !getHexByte(bytesHi) || !getHexByte(bytesLo))		// // //
				error("Error while parsing HFD hex command.");
			int addr = (addrHi << 8) | addrLo;
			int bytes = (bytesHi << 8) | bytesLo;

			if (addr == 0x6136) {
				parseHFDInstrumentHack(addr, bytes);
				return;
			}

			for (++bytes; bytes > 0; --bytes) {		// // //
				if (!getHexByte(i))
					error("Error while parsing HFD hex command.");
				// append(AMKd::Binary::CmdType::ARAM, i, addr >> 8, addr & 0xFF);		// // // Don't do this stuff; we won't know what we're overwriting.
				++addr;
			}
		} break;
		case 0x83:
			error("HFD hex command $ED$83 not implemented.");
		}

		hexLeft = 0;
		return;
	}

	currentHex = AMKd::Binary::CmdType::Envelope;
//	hexLeft = hexLengths[currentHex - AMKd::Binary::CmdType::Inst] - 1 - 1;
	hexLeft = 1;
	append(AMKd::Binary::CmdType::Envelope, i);		// // //
}

// // //
void Music::insertRemoteConversion(std::vector<uint8_t> &&cmd, std::vector<uint8_t> &&conv) {
	auto index = static_cast<uint16_t>(tracks[channel].data.size() + 1);
	tracks[channel].remoteGainInfo.emplace_back(index, std::move(cmd));		// // //
	tracks[channel].data.insert(tracks[channel].data.cend(), conv.cbegin(), conv.cend());
}

// // //
void Music::removeRemoteConversion() {
	tracks[channel].remoteGainInfo.pop_back();		// // //
}

static bool nextNoteIsForDD;

void Music::parseHexCommand() {
	int i = getHex();		// // //
	if (i < 0 || i > 0xFF)
		error("Error parsing hex command.");

	if (!validateHex) {
		append(i);
		return;
	}

	if (hexLeft == 0) {
		currentHex = i;

		using namespace AMKd::MML::Lexer;		// // //
		switch (currentHex) {		// // //
		case AMKd::Binary::CmdType::Vibrato:
			if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
				return apply_this(&Music::doVibrato, this, *param);
			error("Unknown hex command.");
		case AMKd::Binary::CmdType::VolGlobal:
			if (auto param = GetParameters<Byte>(mml_))
				return apply_this(&Music::doGlobalVolume, this, *param);
			error("Unknown hex command.");
		case AMKd::Binary::CmdType::Tempo:
			if (auto param = GetParameters<Byte>(mml_))
				return apply_this(&Music::doTempo, this, *param);
			error("Unknown hex command.");
		case AMKd::Binary::CmdType::TempoFade:
			guessLength = false; // NOPE.  Nope nope nope nope nope nope nope nope nope nope.
			break;
		case AMKd::Binary::CmdType::Tremolo:
			if (auto first = GetParameters<Byte>(mml_)) {		// // //
				if (songTargetProgram == Target::AM4 && first.get<0>() >= 0x80) {
					if (mySamples.empty() && (i & 0x7F) > 0x13)
						error("This song uses custom samples, but has not yet defined its samples with the #samples command.");		// // //
					auto param = GetParameters<Byte>(mml_);
					return doSampleLoad(i - 0x80, param.get<0>());
				}
				if (auto remain = GetParameters<Byte, Byte>(mml_))
					return apply_this(&Music::doTremolo, this, std::tuple_cat(*first, *remain));
			}
			error("Unknown hex command.");
		case AMKd::Binary::CmdType::Subloop:
			if (songTargetProgram == Target::AM4)		// // // N-SPC tremolo off
				return doTremoloOff();		// // //
			if (auto param = GetParameters<Byte>(mml_)) {
				int repeats = param.get<0>();
				return repeats ? doSubloopExit(repeats + 1) : doSubloopEnter();
			}
			error("Unknown hex command.");
		case AMKd::Binary::CmdType::Vol:
			if (auto param = GetParameters<Byte>(mml_))
				return apply_this(&Music::doVolume, this, *param);
			error("Unknown hex command.");
		case AMKd::Binary::CmdType::Envelope:
			if (songTargetProgram == Target::AM4) {
				parseHFDHex();
				return;
			}
			break;
		case AMKd::Binary::CmdType::SampleLoad:
			if (auto param = GetParameters<Byte, Byte>(mml_))
				return apply_this(&Music::doSampleLoad, this, *param);
			error("Unknown hex command.");
		case AMKd::Binary::CmdType::Arpeggio:
			if (auto param = GetParameters<Byte, Byte>(mml_)) {
				unsigned count, len;
				std::tie(count, len) = *param;
				append(AMKd::Binary::CmdType::Arpeggio, count, divideByTempoRatio(len, false));
				if (count >= 0x80) {
					if (count != 0x80 && count != 0x81)
						error("Illegal value for arpeggio command.");
					count = 2;
				}
				for (unsigned i = 0; i < count; ++i) {
					auto note = GetParameters<Byte>(mml_);
					if (!note)
						error("Error parsing arpeggio command.");
					append(note.get<0>());
				}
				return;
			}
			error("Unknown hex command.");
		case AMKd::Binary::CmdType::Callback:
			if (targetAMKVersion > 1)
				error("$FC has been replaced with remote code in #amk 2 and above.");		// // //
			else if (targetAMKVersion == 1) {
				//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")
					// Add in a "restore instrument" remote call.
				int channelToCheck = inNormalLoop ? prevChannel : channel;		// // //
				tracks[channelToCheck].usingFC = true;		// // //

				// If we're just using the FC command and not the FA command as well,
				if (!tracks[channelToCheck].usingFA) {
					// Then add the "restore instrument command"
					insertRemoteConversion({AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::RestoreInst, 0x00},
										   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::KeyOn, 0x00});

					// Then add in the first part of a "apply gain before a note ends" call.
					hexLeft = 2;
					insertRemoteConversion({AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain},
										   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::Release});

					// We won't know the gain and delays until later.
				}
				else {
					// Then add in the first part of a "2 3 combination" call.  
					// Theoretically we could go back and change the previous type to 5.
					// But that's annoying if the commands are separated, so maybe some other time.
					// Shh.  Don't tell anyone.

					hexLeft = 2;
					insertRemoteConversion({AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain},
										   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, 0x05});
					//append(lastFAGainValue[channelToCheck]);
				}
				return;
			}
		case 0xFD: case 0xFE: case 0xFF:
			error("Unknown hex command.");
		default:
			if (currentHex < AMKd::Binary::CmdType::Inst && manualNoteWarning) {
				if (targetAMKVersion == 0) {
					warn("Warning: A hex command was found that will act as a note instead of a special\n"
						 "effect. If this is a song you're using from someone else, you can most likely\n"
						 "ignore this message, though it may indicate that a necessary #amm or #am4 is\n"
						 "missing.");		// // //
					manualNoteWarning = false;
					return;
				}
				error("Unknown hex command.");
			}
		}

		hexLeft = hexLengths[currentHex - AMKd::Binary::CmdType::Inst] - 1;
		append(currentHex);
	}
	else {
		--hexLeft;
		switch (currentHex) {
		case AMKd::Binary::CmdType::Inst:
			if (songTargetProgram == Target::AM4) {		// // // If this was the instrument command
				if (i >= 0x13)					// And it was >= 0x13
					i = i - 0x13 + 30;		// Then change it to a custom instrument.
				if (optimizeSampleUsage)
					usedSamples[instrumentData[(i - 30) * 5]] = true;
			}
			break;
		case AMKd::Binary::CmdType::PanFade:
			if (hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::Portamento:
			if (hexLeft == 2)
				i = divideByTempoRatio(i, false);
			else if (hexLeft == 1) {		// Hack allowing the $DD command to accept a note as a parameter.
				AMKd::MML::SourceFile checkNote {mml_};		// // //
				while (true) {
					checkNote.SkipSpaces();
					if (checkNote.Trim("[A-Ga-g]")) {
						if (tracks[channel].updateQ)		// // //
							error("You cannot use a note as the last parameter of the $DD command if you've also\n"
								  "used the qXX command just before it.");		// // //
						hexLeft = 0;
						nextNoteIsForDD = true;
						break;
					}
					else if (checkNote.Trim('o'))
						AMKd::MML::Lexer::GetParameters<AMKd::MML::Lexer::Int>(checkNote);
					else if (!checkNote.Trim('<') && !checkNote.Trim('>'))
						break;
				}
				i = divideByTempoRatio(i, false);
			}
			break;
		case AMKd::Binary::CmdType::VolGlobalFade:
			if (hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::TempoFade:
			if (hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::TrspGlobal:
			if (hexLeft == 0 && songTargetProgram == Target::AM4) {	// // // AM4 seems to do something strange with $E4?
				++i;
				i &= 0xFF;
			}
			break;
		case AMKd::Binary::CmdType::VolFade:
			if (hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::VibratoFade:
			if (hexLeft == 0)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::BendAway:
			if (hexLeft == 2 || hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::BendTo:
			if (hexLeft == 2 || hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::Echo2:
			if (hexLeft == 2)
				echoBufferSize = std::max(echoBufferSize, i);
			// Print error for AM4 songs that attempt to use an invalid FIR filter. They both
			// A) won't sound like their originals and
			// B) may crash the DSP (or for whatever reason that causes SPCPlayer to go silent with them).
			else if (hexLeft == 0 && songTargetProgram == Target::AM4) {		// // //
				if (i > 1) {
					std::stringstream ss;		// // //
					ss << '$' << hex2 << i;
					error(ss.str() + " is not a valid FIR filter for the $F1 command. Must be either $00 or $01.");
				}
			}
			break;
		case AMKd::Binary::CmdType::EchoFade:
			if (hexLeft == 2)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::ExtF4:
			if (hexLeft == 0)
				if (i == AMKd::Binary::CmdOptionF4::YoshiCh5 || i == AMKd::Binary::CmdOptionF4::Yoshi)
					hasYoshiDrums = true;
			break;
		case AMKd::Binary::CmdType::ExtFA:
			if (hexLeft == 1 && targetAMKVersion > 1 && i == 0x05)
				error("$FA $05 in #amk 2 or above has been replaced with remote code.");		// // //
			// More code conversion.
			if (hexLeft == 0 && targetAMKVersion == 1 && tracks[channel].data.back() == 0x05) {
				//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")
				tracks[channel].data.pop_back();					// // // Remove the last two bytes
				tracks[channel].data.pop_back();					// (i.e. the $FA $05)

				int channelToCheck = inNormalLoop ? prevChannel : channel;

				if (i != 0) {
					// Check if this channel is using FA and FC combined...
					if (!tracks[channelToCheck].usingFC) {		// // //
						// Then add in a "restore instrument" remote call.
						insertRemoteConversion({AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::RestoreInst, 0x00},
											   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::KeyOn, 0x00});

						// Then add the "set gain" remote call.
						insertRemoteConversion({AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00},
											   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::KeyOff, 0x00});
					}
					else {
						// Otherwise, add in a "2 5 combination" command.

						// Then add the "set gain" remote call.
						insertRemoteConversion({AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00},
											   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, 0x05, tracks[channelToCheck].lastFCDelayValue});
						//append(0x00);
					}

					// Either way, we're using FA now.
					tracks[channelToCheck].usingFA = true;
				}
				else {
					insertRemoteConversion({},
										   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::Disable, 0x00});
					tracks[channelToCheck].usingFA = false;
				}
				return;
			}
			break;
		case AMKd::Binary::CmdType::Callback:
			// Do conversion for the old remote gain command.
			if (hexLeft == 1 && targetAMKVersion == 1) {
				//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")

				int channelToCheck = inNormalLoop ? prevChannel : channel;		// // //

				if (i == 0) {							// If i is zero, we have to undo a bunch of stuff.
					if (!tracks[channelToCheck].usingFA) {		// // // But only if this is a "pure" FC command.
						removeRemoteConversion();
						removeRemoteConversion();

						tracks[channel].data.pop_back();		// // //
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();

						insertRemoteConversion({},
											   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::Disable, 0x00});
					}
					else {
						// If we're using FA and FC, then we need to "restore" the FA data.

						// Same as the other "get rid of stuff", but without the "restore instrument" call.
						removeRemoteConversion();

						tracks[channel].data.pop_back();		// // //
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();

						// Then add the "set gain" remote call.
						insertRemoteConversion({AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00},
											   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::KeyOff, 0x00});
					}

					// Either way, FC gets turned off.
					tracks[channelToCheck].usingFC = false;

					//remoteGainConversion.back().push_back(AMKd::Binary::CmdType::ExtFA);
					//remoteGainConversion.back().push_back(AMKd::Binary::CmdOptionFA::Gain);
				}
				else {
					i = divideByTempoRatio(i, false);
					tracks[channelToCheck].lastFCDelayValue = static_cast<uint8_t>(i);		// // //
					append(i);
				}
				return;
			}
			else if (hexLeft == 0 && targetAMKVersion == 1) {		// // //
				//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")
				// // //
				std::map<std::vector<uint8_t>, uint16_t> a;
				if (!tracks[channel].remoteGainInfo.empty()) {
					auto &conv = tracks[channel].remoteGainInfo.back().second;		// // //
					if (!conv.empty()) {			// If the size was zero, then it has no data anyway.  Used for the 0 event type.
	//					int channelToCheck = inNormalLoop ? prevChannel : channel;			// Only saves two bytes, though.

	//					tracks[channelToCheck].lastFCGainValue = static_cast<uint8_t>(i);		// // //
						conv.push_back(i);
						conv.push_back(0x00);
					}
				}
				return;
			}
			break;
		}

		if (i == -1)
			error("Error parsing hex command.");
		if (i < 0 || i > 255)
			error("Illegal value for hex command.");
		append(i);
	}
}

void Music::parseNote(int ch) {		// // //
	if (inRemoteDefinition)
		error("Remote definitions cannot contain note data!");

	// // //
	if (songTargetProgram == Target::AMK && channelDefined == false && inRemoteDefinition == false)
		error("Note data must be inside a channel!");

	int i;		// // //
	if (ch == 'r')
		i = AMKd::Binary::CmdType::Rest;		// // //
	else if (ch == '^')
		i = AMKd::Binary::CmdType::Tie;
	else {
		//am4silence++;
		i = getPitch(ch);

		if (usingHTranspose)
			i += hTranspose;
		else {
			if (!tracks[channel].ignoreTuning)		// // // More AM4 tuning stuff
				i -= transposeMap[tracks[channel].instrument];
		}

		if (i < 0)
			error("Note's pitch was too low.");		// // //
		else if (i >= AMKd::Binary::CmdType::Tie)
			error("Note's pitch was too high.");
		else if (tracks[channel].instrument >= 21 && tracks[channel].instrument < 30)		// // //
		{
			i = 0xD0 + (tracks[channel].instrument - 21);

			if ((channel == 6 || channel == 7 || (inNormalLoop && (prevChannel == 6 || prevChannel == 7))) == false)	// If this is not a SFX channel,
				tracks[channel].instrument = 0xFF;										// Then don't force the drum pitch on every note.
		}
	}

	if (inPitchSlide) {
		inPitchSlide = false;
		append(AMKd::Binary::CmdType::Portamento, 0x00, prevNoteLength, i);		// // //
	}

	if (nextNoteIsForDD) {
		append(i);
		nextNoteIsForDD = false;
		return;
	}

	int j = getNoteLength();		// // //
	skipSpaces();

	while (true) {
		AMKd::MML::SourceFile temptext {mml_};		// // //
		if (!trimChar('^') && !(i == AMKd::Binary::CmdType::Rest && trimChar('r')))
			break;

		int tempsize = j;	// If there's a pitch bend up ahead, we need to not optimize the last tie.
		j += getNoteLength();
		skipSpaces();

		if (mml_.Trim("$DD", true)) {
			j = tempsize;		//
			mml_ = temptext;		// // // "Rewind" so we forcibly place a tie before the bend.
			break;			//
		}
	}

	/*if (normalLoopInsideE6Loop)
	tempLoopLength += j;
	else if (normalLoopInsideE6Loop)
	e6LoopLength += j;
	else if (::inE6Loop)
	e6LoopLength += j;
	else if (inNormalLoop)
	tempLoopLength += j;
	else
	lengths[channel] += j;*/

	j = divideByTempoRatio(j, true);

	addNoteLength(j);

	if (j >= divideByTempoRatio(0x80, true)) {		// Note length must be less than 0x80
		append(divideByTempoRatio(0x60, true));

		if (tracks[channel].updateQ) {		// // //
			append(tracks[channel].q);
			tracks[channel].updateQ = false;
			tracks[CHANNELS].updateQ = false;
			// // //
		}
		append(i); j -= divideByTempoRatio(0x60, true);

		while (j > divideByTempoRatio(0x60, true)) {
			append(AMKd::Binary::CmdType::Tie);		// // //
			j -= divideByTempoRatio(0x60, true);
		}

		if (j > 0) {
			if (j != divideByTempoRatio(0x60, true))
				append(j);
			append(AMKd::Binary::CmdType::Tie);		// // //
		}
		prevNoteLength = j;
		return;
	}
	else if (j > 0) {
		if (j != prevNoteLength || tracks[channel].updateQ)		// // //
			append(j);
		prevNoteLength = j;
		if (tracks[channel].updateQ) {
			append(tracks[channel].q);
			tracks[channel].updateQ = false;
			tracks[CHANNELS].updateQ = false;		// // //
			// // //
		}
		append(i);
	}
	//append(i);
}

void Music::parseHDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<SInt>(mml_)) {
		hTranspose = param.get<0>();
		usingHTranspose = true;
		//transposeMap[tracks[channel].instrument] = -param.get<0>();		// // //
		//htranspose[tracks[channel].instrument] = true;
		return;
	}
	error(DIR_ERROR("transpose (\"h\")"));
}

void Music::parseNCommand() {
	int i = getHex();		// // //
	if (i < 0 || i > 0x1F)
		error("Invlid value for the n command.  Value must be in hex and between 0 and 1F.");		// // //

	append(AMKd::Binary::CmdType::Noise, i);		// // //
}

void Music::parseOptionDirective() {
	if (targetAMKVersion == 1)
		error("Unknown command.");

	if (channelDefined == true)
		error("#option directives must be used before any and all channels.");		// // //

	skipSpaces();

	if (trimDirective("smwvtable"))		// // //
		if (!usingSMWVTable) {
			doVolumeTable(false);
			usingSMWVTable = true;
		}
		else
			warn("This song is already using the SMW V Table. This command is just wasting three bytes...");		// // //
	else if (trimDirective("nspcvtable")) {		// // //
		doVolumeTable(true);
		usingSMWVTable = false;
		warn("This song uses the N-SPC V by default. This command is just wasting two bytes...");		// // //
	}
	else if (trimDirective("tempoimmunity"))		// // //
		append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::TempoImmunity);		// // //
	else if (trimDirective("noloop"))		// // //
		doesntLoop = true;
	else if (trimDirective("dividetempo")) {		// // //
		int i = getInt();		// // //
		if (i == -1)
			error("Missing integer argument for #option dividetempo.");		// // //
		if (i == 0)
			error("Argument for #option dividetempo cannot be 0.");		// // //
		if (i == 1)
			warn("#option dividetempo 1 has no effect besides printing this warning.");		// // //
		if (i < 0)
			error("#halvetempo has been used too many times...what are you even doing?");		// // //

		tempoRatio = i;
	}
	else
		error("#option directive missing its first argument");		// // //
}

void Music::parseSpecialDirective() {
	if (trimDirective("instruments"))		// // //
		parseInstrumentDefinitions();
	else if (trimDirective("samples"))		// // //
		parseSampleDefinitions();
	else if (trimDirective("pad"))		// // //
		parsePadDefinition();
	else if (trimDirective("spc"))		// // //
		parseSPCInfo();
	else if (trimDirective("louder")) {		// // //
		if (targetAMKVersion > 1)
			printWarning("#louder is redundant in #amk 2 and above.");
		parseLouderCommand();
	}
	else if (trimDirective("tempoimmunity"))		// // //
		append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::TempoImmunity);		// // //
	else if (trimDirective("path"))		// // //
		parsePath();
	else if (trimDirective("halvetempo")) {		// // //
		if (channelDefined)
			error("#halvetempo must be used before any and all channels.");		// // //
		tempoRatio *= 2;

		if (tempoRatio < 0)
			error("#halvetempo has been used too many times...what are you even doing?");		// // //
	}
	else if (trimDirective("option"))		// // //
		parseOptionDirective();
	else
		error("Unknown option type for '#' directive.");		// // //
}

void Music::parseReplacementDirective() {
	std::string s = getEscapedString();		// // //
	int i = s.find('=');
	if (i == std::string::npos)
		fatalError("Error parsing replacement directive; could not find '='");		// // //

	std::string findStr = s.substr(0, i);
	std::string replStr = s.substr(i + 1);

	// // //
	while (!findStr.empty() && isspace(findStr.back()))
		findStr.pop_back();
	if (findStr.empty())
		fatalError("Error parsing replacement directive; string to find was of zero length.");

	while (!replStr.empty() && isspace(replStr.front()))
		replStr.erase(0, 1);

	mml_.AddMacro(findStr, replStr);		// // //
}

void Music::parseInstrumentDefinitions() {
	if (skipSpaces(), !trimChar('{'))		// // //
		fatalError("Could not find opening curly brace in instrument definition.");

	while (skipSpaces(), !trimChar('}')) {
		int i;		// // //
		if (trimChar('\"')) {		// // //
			std::string brrName = getEscapedString();
			if (brrName.empty())
				fatalError("Error parsing sample portion of the instrument definition.");
			brrName = basepath + brrName;
			int gs = getSample(brrName, this);
			auto it = std::find(mySamples.cbegin(), mySamples.cend(), getSample(brrName, this));		// // //
			if (it == mySamples.cend())
				fatalError("The specified sample was not included in this song.");		// // //
			i = std::distance(mySamples.cbegin(), it);
		}
		else if (trimChar('n')) {		// // //
			i = getHex();
			if (i == -1 || i > 0xFF)
				fatalError("Error parsing the noise portion of the instrument command.");
			if (i >= 0x20)
				fatalError("Invalid noise pitch.  The value must be a hex value from 0 - 1F.");		// // //

			i |= 0x80;
		}
		else if (trimChar('@')) {
			i = getInt();
			if (i == -1)
				fatalError("Error parsing the instrument copy portion of the instrument command.");
			if (i >= 30)
				fatalError("Cannot use a custom instrument's sample as a base for another custom instrument.");		// // //

			i = instrToSample[i];
		}
		else
			fatalError("Error parsing instrument definition.");

		instrumentData.push_back(i);
		if (optimizeSampleUsage)
			usedSamples[i] = true;

		for (int j = 0; j < 5; j++) {
			if (!getHexByte(i))		// // //
				fatalError("Error parsing instrument definition; there were too few bytes following the sample (there must be 6).");
			instrumentData.push_back(i);
		}
	}

	/*
	enum parseState
	{
		lookingForOpenBrace,
		lookingForAnything,
		lookingForDollarSign,
		lookingForOpenQuote,
		gettingName,
		gettingValue,
	};

	parseState state = lookingForOpenBrace;

	//unsigned char temp;
	int count = 0;

	while (pos < text.length()) {
		switch (state) {
		case lookingForOpenBrace:
			if (isspace(peek())) break;
			if (peek() != '{')
				error("Could not find opening curly brace in instrument definition.");
			state = lookingForDollarSign;
			break;
		case lookingForDollarSign:
			if (peek() == '\n')
				count = 0;
			if (isspace(peek())) break;
			if (peek() == '$') {
				if (count == 6) error("Invalid number of arguments for instrument.  Total number of bytes must be a multiple of 6.");
				state = gettingValue;
				break;
			}
			if (peek() == '}') {
				if (count != 0)
					error("Invalid number of arguments for instrument.  Total number of bytes must be a multiple of 6.");
				skipChars(1);
				return;
			}

			error("Error parsing instrument definition.");
			break;
		case gettingValue:
			int val = getHex();
			if (val == -1 || val > 255) error("Error parsing instrument definition.");
			instrumentData.push_back(val);
			state = lookingForDollarSign;
			count++;
			break;
		}
		skipChars(1);
	}
	*/
}

void Music::parseSampleDefinitions() {
	if (skipSpaces(), !trimChar('{'))		// // //
		fatalError("Unexpected character in sample group definition.  Expected \"{\".");

	while (true) {		// // //
		if (skipSpaces(), trimChar('\"')) {		// // //
			std::string tempstr = basepath + getEscapedString();		// // //
			auto tmppos = tempstr.find_last_of(".");
			if (tmppos == -1)
				fatalError("The filename for the sample was missing its extension; is it a .brr or .bnk?");		// // //
			std::string extension = tempstr.substr(tmppos);
			if (extension == ".bnk")
				addSampleBank(tempstr, this);
			else if (extension == ".brr")
				addSample(tempstr, this, false);
			else
				fatalError("The filename for the sample was invalid.  Only \".brr\" and \".bnk\" are allowed.");		// // //
		}
		else if (trimChar('#'))
			addSampleGroup(getIdentifier(), this);		// // //
		else if (trimChar('}'))
			return;
		else
			fatalError("Unexpected end of sample group definition.");
	}
}

void Music::parsePadDefinition() {
	if (hasNextToken() && trimChar('$')) {		// // //
		using namespace AMKd::MML::Lexer;
		if (auto param = HexInt()(mml_)) {
			minSize = *param;
			return;
		}
	}

	error(DIR_ERROR("padding"));		// // //
}

void Music::parseLouderCommand() {
	append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::VelocityTable);		// // //
}

void Music::parsePath() {
	if (skipSpaces(), !trimChar('\"'))
		fatalError("Unexpected symbol found in path command. Expected a quoted string.");
	auto str = getEscapedString();		// // //
	if (str.empty())
		fatalError("Unexpected symbol found in path command. Expected a quoted string.");
	basepath = "./" + str + "/";
}

// // //
bool Music::hasNextToken() {
	return mml_.HasNextToken();
}

// // //
int Music::peek() {
	return mml_.Peek();
}

int Music::getInt() {
	//if (peek() == '$') { skipChars(1); return getHex(); }	// Allow for things such as t$20 instead of t32.
	// Actually, can't do it.
	// Consider this:
	// l8r$ED$00$00
	// Yeah. Oh well.
	// Attempt to do a replacement.  (So things like "ab = 8"    [c1]ab    are valid).
	if (hasNextToken())		// // //
		if (auto param = AMKd::MML::Lexer::Int()(mml_))
			return *param;
	return -1;
}

int Music::getIntWithNegative() {
	if (hasNextToken())		// // //
		if (auto param = AMKd::MML::Lexer::SInt()(mml_))
			return *param;
	throw "Invalid number";
}

int Music::getHex() {		// // //
	if (hasNextToken())		// // //
		if (auto param = AMKd::MML::Lexer::Hex<2>()(mml_))
			return *param;
	return -1;
}

// // //
bool Music::getHexByte(int &out) {
	if (hasNextToken())		// // //
		if (auto param = AMKd::MML::Lexer::Byte()(mml_)) {
			out = *param;
			return true;
		}
	return false;
}

int Music::getPitch(int i) {
	static const int pitches[] = {9, 11, 0, 2, 4, 5, 7};

	i = pitches[i - 'a'] + (octave - 1) * 12 + 0x80;		// // //
	if (trimChar('+'))
		++i;
	else if (trimChar('-'))
		--i;

	/*if (i < 0x80)
	return -1;
	if (i >= 0xC6)
	return -2;*/

	return i;
}

// // //
int Music::getNoteLength() {
	//bool still = true;

	int len = getInt();		// // //

	if (len == -1 && trimChar('=')) {		// // //
		len = getInt();
		if (len == -1 || len == 0)		// // // do not allow zero duration
			[&] { error("Error parsing note"); }();
		return len;
		//if (len < 1) still = false; else return len;
	}

	//if (still)
	//{
	if (len < 1 || len > 192)
		len = defaultNoteLength;
	len = 192 / len;

	int frac = len;

	int times = 0;
	while (trimChar('.')) {		// // //
		frac /= 2;
		len += frac;
		if (++times == 2 && songTargetProgram == Target::AM4)
			break;	// // // AM4 only allows two dots for whatever reason.
	}

	if (triplet)
		len = static_cast<int>(std::floor(len * 2.0 / 3.0 + 0.5));
	return len;
}

// // //
std::string Music::getIdentifier() {
	if (auto param = AMKd::MML::Lexer::Ident()(mml_))
		return *param;
	printError("Error while parsing identifier.");		// // //
	return "";
}

// // //
std::string Music::getEscapedString() {
	if (auto param = AMKd::MML::Lexer::QString()(mml_))
		return *param;
	printError("Unexpected end of file found.");		// // //
	return "";
}

// // //

void Music::pointersFirstPass() {
	if (errorCount)
		fatalError("There were errors when compiling the music file.  Compilation aborted.  Your ROM has not been modified.");

	if (std::all_of(tracks, tracks + CHANNELS, [] (const Track &t) { return t.data.empty(); }))		// // //
		error("This song contained no musical data!");

	if (targetAMKVersion == 1)			// Handle more conversion of the old $FC command to remote call.
		for (auto &track : tracks) for (const auto &x : track.remoteGainInfo) {		// // //
			uint16_t dataIndex = x.first;
			track.loopLocations.push_back(dataIndex);

			track.data[dataIndex] = static_cast<uint8_t>(tracks[CHANNELS].data.size() & 0xFF);
			track.data[dataIndex + 1] = static_cast<uint8_t>(tracks[CHANNELS].data.size() >> 8);

			tracks[CHANNELS].data.insert(tracks[CHANNELS].data.end(), x.second.cbegin(), x.second.cend());
		}

	for (int z = 0; z < CHANNELS; z++) if (!tracks[z].data.empty())		// // //
		tracks[z].data.push_back(AMKd::Binary::CmdType::End);

	if (mySamples.empty())		// // // If no sample groups were provided...
		addSampleGroup("default", this);		// // //

	if (game.empty())
		game = "Super Mario World (custom)";

	if (optimizeSampleUsage) {
		int emptySampleIndex = ::getSample("EMPTY.brr", this);
		if (emptySampleIndex == -1) {
			addSample("EMPTY.brr", this, true);
			emptySampleIndex = getSample("EMPTY.brr", this);
		}

		for (size_t z = 0, n = mySamples.size(); z < n; ++z)		// // //
			if (usedSamples[z] == false && samples[mySamples[z]].important == false)
				mySamples[z] = emptySampleIndex;
	}

	int binpos = 0;		// // //
	for (int i = 0; i < CHANNELS; ++i) {
		if (!tracks[i].data.empty())
			tracks[i].phrasePointers[0] = binpos;
		tracks[i].phrasePointers[1] += tracks[i].phrasePointers[0];
		binpos += tracks[i].data.size();
	}
	// // //
	spaceForPointersAndInstrs = 20;

	if (hasIntro)
		spaceForPointersAndInstrs += 18;
	if (!doesntLoop)
		spaceForPointersAndInstrs += 2;

	spaceForPointersAndInstrs += instrumentData.size();

	allPointersAndInstrs.resize(spaceForPointersAndInstrs);// = alloc(spaceForPointersAndInstrs);
	//for (i = 0; i < spaceForPointers; i++) allPointers[i] = 0x55;

	int add = (hasIntro ? 2 : 0) + (doesntLoop ? 0 : 2) + 4;

	//memcpy(allPointersAndInstrs.data() + add, instrumentData.base, instrumentData.size());
	for (size_t z = 0, n = instrumentData.size(); z < n; ++z)		// // //
		allPointersAndInstrs[add + z] = instrumentData[z];

	allPointersAndInstrs[0] = static_cast<uint8_t>((add + instrumentData.size()) & 0xFF);		// // //
	allPointersAndInstrs[1] = static_cast<uint8_t>((add + instrumentData.size()) >> 8);

	if (doesntLoop) {
		allPointersAndInstrs[add - 2] = 0xFF;	// Will be re-evaluated to 0000 when the pointers are adjusted later.
		allPointersAndInstrs[add - 1] = 0xFF;
	}
	else {
		allPointersAndInstrs[add - 4] = 0xFE;	// Will be re-evaluated to FF00 when the pointers are adjusted later.
		allPointersAndInstrs[add - 3] = 0xFF;
		if (hasIntro) {
			allPointersAndInstrs[add - 2] = 0xFD;	// Will be re-evaluated to 0002 + ARAMPos when the pointers are adjusted later.
			allPointersAndInstrs[add - 1] = 0xFF;
		}
		else {
			allPointersAndInstrs[add - 2] = 0xFC;	// Will be re-evaluated to ARAMPos when the pointers are adjusted later.
			allPointersAndInstrs[add - 1] = 0xFF;
		}
	}
	if (hasIntro) {
		allPointersAndInstrs[2] = static_cast<uint8_t>((add + instrumentData.size() + 16) & 0xFF);		// // //
		allPointersAndInstrs[3] = static_cast<uint8_t>((add + instrumentData.size() + 16) >> 8);
	}

	add += instrumentData.size();
	for (int i = 0; i < CHANNELS; ++i) {		// // //
		unsigned short adr = tracks[i].data.empty() ? 0xFFFB : (tracks[i].phrasePointers[0] + spaceForPointersAndInstrs);
		allPointersAndInstrs[i * 2 + 0 + add] = static_cast<uint8_t>(adr & 0xFF);		// // //
		allPointersAndInstrs[i * 2 + 1 + add] = static_cast<uint8_t>(adr >> 8);
	}

	if (hasIntro) {
		for (int i = 0; i < CHANNELS; ++i) {		// // //
			unsigned short adr = tracks[i].data.empty() ? 0xFFFB : (tracks[i].phrasePointers[1] + spaceForPointersAndInstrs);
			allPointersAndInstrs[i * 2 + 16 + add] = static_cast<uint8_t>(adr & 0xFF);		// // //
			allPointersAndInstrs[i * 2 + 17 + add] = static_cast<uint8_t>(adr >> 8);
		}
	}

	totalSize = spaceForPointersAndInstrs + tracks[CHANNELS].data.size() + getDataSize();		// // //

	//if (tempo == -1) tempo = 0x36;
	unsigned int totalLength;
	mainLength = -1;
	for (int i = 0; i < CHANNELS; i++)
		if (tracks[i].channelLength != 0)		// // //
			mainLength = std::min(mainLength, (unsigned int)tracks[i].channelLength);
	if (mainLength == -1)
		error("This song doesn't seem to have any data.");		// // //

	totalLength = mainLength;

	if (hasIntro)
		mainLength -= introLength;

	if (guessLength) {
		double l1 = 0, l2 = 0;
		bool onL1 = true;

		std::sort(tempoChanges.begin(), tempoChanges.end());		// // //
		if (tempoChanges.empty() || tempoChanges[0].first != 0) {
			tempoChanges.insert(tempoChanges.begin(), std::make_pair(0., 0x36)); // Stick the default tempo at the beginning if necessary.
		}

		tempoChanges.emplace_back(totalLength, 0);		// // // Add in a dummy tempo change at the very end.

		for (size_t z = 0, n = tempoChanges.size() - 1; z < n; ++z)		// // //
		{
			if (tempoChanges[z].first > totalLength) {
				warn("A tempo change was found beyond the end of the song.");		// // //
				break;
			}

			if (tempoChanges[z].second < 0)
				onL1 = false;

			double difference = tempoChanges[z + 1].first - tempoChanges[z].first;
			if (onL1)
				l1 += difference / (2 * std::abs(tempoChanges[z].second));
			else
				l2 += difference / (2 * std::abs(tempoChanges[z].second));
		}

		if (hasIntro) {
			seconds = (unsigned int)std::floor(l1 + l2 * 2 + 0.5);	// Just 2? Not 2.012584 or something?  Wow.
			mainSeconds = l2;
			introSeconds = l1;
		}
		else {
			mainSeconds = l1;
			seconds = (unsigned int)std::floor(l1 * 2 + 0.5);
		}
		knowsLength = true;
	}

	int spaceUsedBySamples = 0;
	for (const uint16_t x : mySamples)		// // //
		spaceUsedBySamples += 4 + samples[x].data.size();	// The 4 accounts for the space used by the SRCN table.

	if (verbose)
		std::cout << name << " total size: 0x" << hex4 << totalSize << std::dec << " bytes\n";
	else
		printChannelDataNonVerbose(totalSize);

	//for (int z = 0; z <= CHANNELS; z++)
	//{
	if (verbose) {
		const hex_formatter hex3 {3};
		std::cout << '\t';		// // //
		for (int i = 0; i < CHANNELS / 2; ++i)
			std::cout << "#" << i << ": 0x" << hex3 << tracks[i].data.size() << ' ';
		std::cout << "Ptrs+Instrs: 0x" << hex3 << spaceForPointersAndInstrs << "\n\t";
		for (int i = CHANNELS / 2; i < CHANNELS; ++i)
			std::cout << "#" << i << ": 0x" << hex3 << tracks[i].data.size() << ' ';
		std::cout << "Loop:        0x" << hex3 << tracks[CHANNELS].data.size();
		std::cout << "\nSpace used by echo: 0x" << hex4 << (echoBufferSize << 11) <<
			" bytes.  Space used by samples: 0x" << hex4 << spaceUsedBySamples << " bytes.\n\n";
	}
	//}
	if (totalSize > minSize && minSize > 0) {
		std::stringstream err;		// // //
		err << "Song was larger than it could pad out by 0x" << hex4 << totalSize - minSize << " bytes.";
		warn(err.str());		// // //
	}

	std::stringstream statStrStream;

	for (int i = 0; i < CHANNELS; ++i)		// // //
		statStrStream << "CHANNEL " << ('0' + i) << " SIZE:				0x" << hex4 << tracks[i].data.size() << "\n";
	statStrStream << "LOOP DATA SIZE:				0x" << hex4 << tracks[CHANNELS].data.size() << "\n";
	statStrStream << "POINTERS AND INSTRUMENTS SIZE:		0x" << hex4 << spaceForPointersAndInstrs << "\n";
	statStrStream << "SAMPLES SIZE:				0x" << hex4 << spaceUsedBySamples << "\n";
	statStrStream << "ECHO SIZE:				0x" << hex4 << (echoBufferSize << 11) << "\n";
	statStrStream << "SONG TOTAL DATA SIZE:			0x" << hex4 << totalSize << "\n";		// // //

	if (index > highestGlobalSong)
		statStrStream << "FREE ARAM (APPROXIMATE):		0x" << hex4 << 0x10000 - (echoBufferSize << 11) - spaceUsedBySamples - totalSize - programUploadPos << "\n\n";
	else
		statStrStream << "FREE ARAM (APPROXIMATE):		UNKNOWN\n\n";

	for (int i = 0; i < CHANNELS; ++i)		// // //
		statStrStream << "CHANNEL " << ('0' + i) << " TICKS:			0x" << hex4 << tracks[i].channelLength << "\n";
	statStrStream << '\n';

	if (knowsLength) {
		statStrStream << "SONG INTRO LENGTH IN SECONDS:		" << std::dec << introSeconds << "\n";
		statStrStream << "SONG MAIN LOOP LENGTH IN SECONDS:	" << mainSeconds << "\n";
		statStrStream << "SONG TOTAL LENGTH IN SECONDS:		" << introSeconds + mainSeconds << "\n";
	}
	else {
		statStrStream << "SONG INTRO LENGTH IN SECONDS:		UNKNOWN\n";
		statStrStream << "SONG MAIN LOOP LENGTH IN SECONDS:	UNKNOWN\n";
		statStrStream << "SONG TOTAL LENGTH IN SECONDS:		UNKNOWN\n";
	}

	statStr = statStrStream.str();

	std::string fname = name;

	int extPos = fname.find_last_of('.');
	if (extPos != -1)
		fname = fname.substr(0, extPos);

	if (fname.find('/') != -1)
		fname = fname.substr(fname.find_last_of('/') + 1);
	else if (fname.find('\\') != -1)
		fname = fname.substr(fname.find_last_of('\\') + 1);
	fname = "stats/" + fname + ".txt";

	writeTextFile(fname, statStr);
}

// // //
void Music::adjustLoopPointers() {
	int normalChannelsSize = getDataSize();		// // //
	for (Track &t : tracks) {		// // //
		for (unsigned short x : t.loopLocations) {
			int temp = (t.data[x] & 0xFF) | (t.data[x + 1] << 8);
			temp += posInARAM + normalChannelsSize + spaceForPointersAndInstrs;
			t.data[x] = temp & 0xFF;
			t.data[x + 1] = temp >> 8;
		}
	}
}

// // //

void Music::parseSPCInfo() {
	if (skipSpaces(), !trimChar('{'))
		error("Could not find opening brace in SPC info command.");

	while (skipSpaces(), !trimChar('}')) {
		if (!trimChar('#'))		// // //
			error("Unexpected symbol found in SPC info command.  Expected \'#\'.");
		std::string typeName = getIdentifier();		// // //

		if (typeName != "author" && typeName != "comment" && typeName != "title" && typeName != "game" && typeName != "length")
			error("Unexpected type name found in SPC info command.  Only \"author\", \"comment\", \"title\", \"game\", and \"length\" are allowed.");

		if (skipSpaces(), !trimChar('\"'))
			error("Error while parsing parameter for SPC info command.");
		std::string parameter = getEscapedString();		// // //

		if (typeName == "author")
			author = parameter;
		else if (typeName == "comment")
			comment = parameter;
		else if (typeName == "title")
			title = parameter;
		else if (typeName == "game")
			game = parameter;
		else if (typeName == "length") {
			if (parameter == "auto")
				guessLength = true;
			else {
				guessLength = false;
				AMKd::MML::SourceFile field {parameter};		// // //
				auto param = AMKd::MML::Lexer::Time()(field);
				if (param && !field)
					seconds = *param;
				else
					error("Error parsing SPC length field.  Format must be m:ss or \"auto\".");		// // //

				if (seconds > 999)
					error("Songs longer than 16:39 are not allowed by the SPC format.");		// // //

				seconds = seconds & 0xFFFFFF;
				knowsLength = true;
			}
		}
	}

	if (author.length() > 32) {
		author = author.substr(0, 32);
		printWarning("\"Author\" field was too long.  Truncating to \"" + author + "\".");		// // //
	}
	if (game.length() > 32) {
		game = game.substr(0, 32);
		printWarning("\"Game\" field was too long.  Truncating to \"" + game + "\".");
	}
	if (comment.length() > 32) {
		comment = comment.substr(0, 32);
		printWarning("\"Comment\" field was too long.  Truncating to \"" + comment + "\".");
	}
	if (title.length() > 32) {
		title = title.substr(0, 32);
		printWarning("\"Title\" field was too long.  Truncating to \"" + title + "\".");
	}
}

void Music::addNoteLength(double ticks) {
	if (extraLoopIsNormal)
		normalLoopLength += ticks;
	else if (extraLoopIsSuper)
		superLoopLength += ticks;
	else if (baseLoopIsNormal)
		normalLoopLength += ticks;
	else if (baseLoopIsSuper)
		superLoopLength += ticks;
	else
		tracks[channel].channelLength += ticks;		// // //
}

// // //
void Music::synchronizeQ() {
	if (!inNormalLoop)		// // //
		tracks[channel].updateQ = true;		// // //
	tracks[CHANNELS].updateQ = true;
	prevNoteLength = -1;
}

int Music::divideByTempoRatio(int value, bool fractionIsError) {
	int temp = value / tempoRatio;
/*
	if (temp * tempoRatio != value)
		if (fractionIsError)
			printError("Using the tempo ratio on this value would result in a fractional value.", false, name, line);
		else
			warn("The tempo ratio resulted in a fractional value.");
*/
	return temp;
}

int Music::multiplyByTempoRatio(int value) {
	int temp = value * tempoRatio;
	if (temp > 0xFF) {		// // //		
		warn("Using the tempo ratio on this value would cause it to overflow.");
		temp = 0xFF;
	}
	return temp;
}

// // //
const std::string &Music::getFileName() const {
	return name;
}



// // //
void Music::doVolume(int vol) {
	append(AMKd::Binary::CmdType::Vol, vol);
}

void Music::doGlobalVolume(int vol) {
	append(AMKd::Binary::CmdType::VolGlobal, vol);
}

void Music::doVibrato(int delay, int rate, int depth) {
	append(AMKd::Binary::CmdType::Vibrato,
		divideByTempoRatio(delay, false), multiplyByTempoRatio(rate), depth);		// // //
}

void Music::doTremolo(int delay, int rate, int depth) {
	append(AMKd::Binary::CmdType::Tremolo,
		divideByTempoRatio(delay, false), multiplyByTempoRatio(rate), depth);		// // //
}

void Music::doTremoloOff() {
	append(AMKd::Binary::CmdType::Tremolo, 0x00, 0x00, 0x00);
}

void Music::doTempo(int speed) {
	tempo = requires(divideByTempoRatio(speed, false), 0x01, 0xFF, "Tempo has been zeroed out by #halvetempo");		// // //
	tempoDefined = true;
	append(AMKd::Binary::CmdType::Tempo, tempo);		// // //

	if (inNormalLoop || inE6Loop)		// // // Not even going to try to figure out tempo changes inside loops.  Maybe in the future.
		guessLength = false;
	else
		tempoChanges.emplace_back(tracks[channel].channelLength, tempo);		// // //
}

void Music::doSampleLoad(int index, int mult) {
	if (optimizeSampleUsage)
		usedSamples[index] = true;
	append(AMKd::Binary::CmdType::SampleLoad, index, mult);		// // //
}

void Music::doLoopEnter() {
	normalLoopLength = 0;
	if (inE6Loop) {					// We're entering a normal loop that's nested in a super loop
		baseLoopIsNormal = false;
		baseLoopIsSuper = true;
		extraLoopIsNormal = true;
		extraLoopIsSuper = false;
	}
	else {						// We're entering a normal loop that's not nested
		baseLoopIsNormal = true;
		baseLoopIsSuper = false;
		extraLoopIsNormal = false;
		extraLoopIsSuper = false;
	}
}

void Music::doLoopExit(int loopCount) {
	if (extraLoopIsNormal) {				// We're leaving a normal loop that's nested in a super loop.
		extraLoopIsNormal = false;
		extraLoopIsSuper = false;
		superLoopLength += normalLoopLength * loopCount;
	}
	else if (baseLoopIsNormal) {			// We're leaving a normal loop that's not nested.
		baseLoopIsNormal = false;
		baseLoopIsSuper = false;
		tracks[channel].channelLength += normalLoopLength * loopCount;		// // //
	}

	if (loopLabel > 0)
		loopLengths[loopLabel] = normalLoopLength;
}

void Music::doLoopRemoteCall(int loopCount) {
	addNoteLength((loopLabel ? loopLengths[loopLabel] : normalLoopLength) * loopCount);		// // //
}

void Music::doSubloopEnter() {
	if (inE6Loop)		// // //
		error("You cannot nest a subloop within another subloop.");
	inE6Loop = true;

	synchronizeQ();		// // //
	superLoopLength = 0;

	if (inNormalLoop) {		// // // We're entering a super loop that's nested in a normal loop
		baseLoopIsNormal = true;
		baseLoopIsSuper = false;
		extraLoopIsNormal = false;
		extraLoopIsSuper = true;
	}
	else {						// We're entering a super loop that's not nested
		baseLoopIsNormal = false;
		baseLoopIsSuper = true;
		extraLoopIsNormal = false;
		extraLoopIsSuper = false;
	}

	append(AMKd::Binary::CmdType::Subloop, 0x00);		// // //
}

void Music::doSubloopExit(int loopCount) {
	if (!inE6Loop)		// // //
		error("A subloop end was found outside of a subloop.");
	inE6Loop = false;

	synchronizeQ();		// // //

	if (extraLoopIsSuper) {				// We're leaving a super loop that's nested in a normal loop.
		extraLoopIsNormal = false;
		extraLoopIsSuper = false;
		normalLoopLength += superLoopLength * loopCount;
	}
	else if (baseLoopIsSuper) {			// We're leaving a super loop that's not nested.
		baseLoopIsNormal = false;
		baseLoopIsSuper = false;
		tracks[channel].channelLength += superLoopLength * loopCount;		// // //
	}

	append(AMKd::Binary::CmdType::Subloop, loopCount - 1);		// // //
}

void Music::doVolumeTable(bool louder) {
	append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::VolTable, louder ? 0x01 : 0x00);
}
