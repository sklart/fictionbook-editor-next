#pragma once

#include "WordsItem.h"

namespace FbeSettings { namespace Words {
	void Load(std::vector<WordsItem>& words);
	void Save(const std::vector<WordsItem>& words);
} }
