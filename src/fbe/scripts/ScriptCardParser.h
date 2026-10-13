#pragma once

#include <windows.h>
#include <atlstr.h>
#include <vector>

namespace ScriptCards {

enum class State { Missing, Invalid, Unchecked, Current, Changed };

struct Author {
	CString name;
	CString role;
};

struct Dependency {
	enum class State { Unverified, Current, Changed, Missing, OptionalMissing, Invalid };
	CString path;
	CString expectedSha256;
	CString currentSha256;
	bool optional = false;
	State state = State::Unverified;
};

struct Card {
	State state = State::Missing;
	CString error;
	CString title;
	CString version;
	CString summary;
	CString description;
	CString scope;
	CString expectedSha256;
	CString currentSha256;
	CString reviewStatus;
	CString minFbeVersion;
	CString undo;
	int modifiesDocument = -1;
	int requiresSelection = -1;
	std::vector<Author> authors;
	std::vector<CString> usage;
	std::vector<CString> warnings;
	std::vector<CString> settings;
	std::vector<CString> examples;
	std::vector<CString> tags;
	std::vector<CString> changelog;
	std::vector<CString> links;
	std::vector<Dependency> dependencies;
	bool dependenciesComplete = false;
	enum class DependenciesState { Unverified, Current, Changed, Invalid } dependencyState = DependenciesState::Unverified;
	CString json;
};

// Static parser. It never executes JScript or updates an expected digest.
Card Parse(const BYTE* bytes, size_t length);
Card LoadFile(const CString& path);
// Call only when the card is opened, outside drawing and startup enumeration.
void VerifyDependencies(Card& card, const CString& resourceRoot);

}
