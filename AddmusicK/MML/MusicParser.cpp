#include "MusicParser.h"
#include "../Music.h"
#include "Lexer.h"
#include "../Utility/Exception.h"
#include "../globals.h"
#include "../Utility/StaticTrie.h"

using namespace AMKd::MML;
using namespace AMKd::MML::Lexer;

namespace {
template <typename T>
struct TrieAdaptor
{
	void operator()(const std::string_view &, std::size_t, SourceView &file, ::Music &music) {
		return T()(file, music);
	}
};
template <typename T, char... Cs>
using Entry = AMKd::Utility::TrieEntry<TrieAdaptor<T>, Cs...>;
} // namespace

void MusicParser::operator()(SourceView &file, ::Music &music) {
	constexpr AMKd::Utility::StaticTrie<
		Entry<Parser::ExMark      , '!'>,
		Entry<Parser::Comment     , ';'>,
		Entry<Parser::Bar         , '|'>,
		Entry<Parser::Replacement , '"'>,
		Entry<Parser::RaiseOctave , '>'>,
		Entry<Parser::LowerOctave , '<'>,
		Entry<Parser::LoopBegin   , '['>,
		Entry<Parser::SubloopBegin, '[', '['>,
		Entry<Parser::ErrorBegin  , '[', '[', '['>,
		Entry<Parser::ErrorEnd    , ']', ']', ']'>,
		Entry<Parser::Intro       , '/'>,
		Entry<Parser::TripletOpen , '{'>,
		Entry<Parser::TripletClose, '}'>
	> TRIE;

	const auto doParse = [&] (std::string_view &sv) {
		return AMKd::Utility::ParseTrie(TRIE, sv, file, music);
	};

	while (file.HasNextToken()) {		// // // TODO: also call this for selected lexers
		try {
			if (!file.TryProcess(doParse) && !music.compileStep())		// // // TODO: remove
				throw AMKd::Utility::SyntaxException {"Unexpected character \"" + *file.Trim(".") + "\" found."};
		}
		catch (AMKd::Utility::MMLException &e) {
			::printError(e.what(), music.name, file.GetLineNumber());
		}
	}
}

void Parser::Comment::operator()(SourceView &file, ::Music &music) {
	(void)GetParameters<Row>(file);
	return music.doComment();
}

void Parser::ExMark::operator()(SourceView &, ::Music &music) {
	return music.doExMark();
}

void Parser::Bar::operator()(SourceView &, ::Music &music) {
	return music.doBar();
}

void Parser::Replacement::operator()(SourceView &file, ::Music &) {
	file.Unput();
	auto param = GetParameters<QString>(file);
	if (!param)
		throw AMKd::Utility::SyntaxException {"Unexpected end of replacement directive."};
	std::string s = param.get<0>();
	size_t i = s.find('=');
	if (i == std::string::npos)
		throw AMKd::Utility::SyntaxException {"Error parsing replacement directive; could not find '='"};		// // //

	std::string findStr = s.substr(0, i);
	std::string replStr = s.substr(i + 1);

	while (!findStr.empty() && isspace(findStr.back()))
		findStr.pop_back();
	if (findStr.empty())
		throw AMKd::Utility::ParamException {"Replacement string to find cannot be empty."};

	while (!replStr.empty() && isspace(replStr.front()))
		replStr.erase(0, 1);

	file.AddMacro(findStr, replStr);		// // //
}

void Parser::RaiseOctave::operator()(SourceView &, ::Music &music) {
	return music.doRaiseOctave();
}

void Parser::LowerOctave::operator()(SourceView &, ::Music &music) {
	return music.doLowerOctave();
}

void Parser::LoopBegin::operator()(SourceView &, ::Music &music) {
	return music.doLoopEnter();
}

void Parser::SubloopBegin::operator()(SourceView &, ::Music &music) {
	return music.doSubloopEnter();
}

void Parser::ErrorBegin::operator()(SourceView &, ::Music &) {
	throw AMKd::Utility::SyntaxException {"An ambiguous use of the [ and [[ loop delimiters was found (\"[[[\").  Separate\n"
										  "the \"[[\" and \"[\" to clarify your intention."};
}

void Parser::ErrorEnd::operator()(SourceView &file, ::Music &) {
	GetParameters<Int>(file);
	throw AMKd::Utility::SyntaxException {"An ambiguous use of the ] and ]] loop delimiters was found (\"]]]\").  Separate\n"
										  "the \"]]\" and \"]\" to clarify your intention."};
}

void Parser::Intro::operator()(SourceView &, ::Music &music) {
	return music.doIntro();
}

void Parser::TripletOpen::operator()(SourceView &, ::Music &music) {
	return music.doTriplet(true);
}

void Parser::TripletClose::operator()(SourceView &, ::Music &music) {
	return music.doTriplet(false);
}