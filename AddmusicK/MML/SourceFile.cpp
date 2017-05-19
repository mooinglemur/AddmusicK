#include "SourceFile.h"
#include <regex>

#include <unordered_map>

const std::regex &get_re(std::string_view re, bool ignoreCase) {
	static std::unordered_map<std::string_view, std::regex> regex_cache;
	auto flag = std::regex::ECMAScript | std::regex::optimize | (ignoreCase ? std::regex::icase : (std::regex_constants::syntax_option_type)0);

	auto it = regex_cache.find(re);
	if (it != regex_cache.cend())
		return it->second;
	std::regex cache {re.data(), flag};
	auto result = regex_cache.insert(std::make_pair(re, cache));
	return result.first->second;
}

using namespace AMKd::MML;

SourceFile::SourceFile() : sv_(mml_), prev_(sv_)
{
}

SourceFile::SourceFile(std::string_view data) :
	mml_(data), sv_(mml_), prev_(sv_)
{
}

SourceFile::SourceFile(const SourceFile &other) :
	mml_(other.mml_)
{
	SetInitReadCount(other.GetReadCount());
}

SourceFile::SourceFile(SourceFile &&other) noexcept :
	mml_(std::move(other.mml_))
{
	SetInitReadCount(mml_.size() - other.sv_.size());
}

SourceFile &SourceFile::operator=(const SourceFile &other) {
	mml_ = other.mml_;
	SetInitReadCount(other.GetReadCount());
	return *this;
}

SourceFile &SourceFile::operator=(SourceFile &&other) {
	size_t len = other.GetReadCount();
	mml_.swap(other.mml_);
	SetInitReadCount(len);
	return *this;
}

std::optional<std::string> SourceFile::Trim(std::string_view re, bool ignoreCase) {
	std::cmatch match;
	std::optional<std::string> z;

	prev_ = sv_;
	if (std::regex_search(sv_.data(), match, get_re(re, ignoreCase),
						  std::regex_constants::match_continuous)) {
		size_t len = match[0].length();
		z = sv_.substr(0, len);
		sv_.remove_prefix(len);
	}

	return z;
}

std::optional<std::string> SourceFile::Trim(char re) {
	std::optional<std::string> z;

	prev_ = sv_;
	if (!sv_.empty() && sv_.front() == re) {
		z = std::string(1, re);
		sv_.remove_prefix(1);
	}

	return z;
}

int SourceFile::Peek() const {
	if (sv_.empty())
		return std::string_view::npos;
	return sv_.front();
}

void SourceFile::SkipSpaces() {
	do {
		Trim(R"(\s*)");
	} while (sv_.empty() && PopMacro());
}

void SourceFile::Clear() {
	if (!macros_.empty())
		mml_ = std::move(macros_.front().mml);
	macros_.clear();
	SetInitReadCount(mml_.size());
}

void SourceFile::Unput() {
	sv_ = prev_;
}

bool SourceFile::PushMacro(std::string_view key, std::string_view repl) {
	for (const auto &x : macros_)
		if (x.key == key)
			return false;
	std::size_t len = GetReadCount() + key.size();
	macros_.push_back(MacroState {key, std::move(mml_), len});
	prev_ = sv_ = mml_ = repl;
	return true;
}

bool SourceFile::PopMacro() {
	if (macros_.empty())
		return false;
	mml_ = std::move(macros_.back().mml);
	SetInitReadCount(macros_.back().charCount);
	macros_.pop_back();
	return true;
}

std::size_t SourceFile::GetLineNumber() const {
	return std::count(mml_.cbegin(), mml_.cend(), '\n') -
		std::count(sv_.cbegin(), sv_.cend(), '\n') + 1;
}

std::size_t SourceFile::GetReadCount() const {
	return mml_.size() - sv_.size();
}

void SourceFile::SetReadCount(std::size_t count) {
	prev_ = sv_;
	sv_ = mml_;
	sv_.remove_prefix(count);
}

void SourceFile::SetInitReadCount(std::size_t count) {
	sv_ = mml_;
	sv_.remove_prefix(count);
	prev_ = sv_;
}

SourceFile::operator bool() const {
	return !sv_.empty();
}
