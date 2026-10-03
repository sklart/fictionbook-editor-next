#include "stdafx.h"
#include "search/SearchPresetCatalog.h"
#include "search/ReplacementParser.h"
#include "search/RegexBackend.h"
#include "RuntimeLocalization.h"
#include <map>
#include <iostream>
#define PCRE2_CODE_UNIT_WIDTH 16
#define PCRE2_STATIC
#include "pcre2.h"
CString FbeLoadRuntimeStringByKey(LPCWSTR, LPCWSTR fallback) { return fallback ? CString(fallback) : CString(); }
namespace {
using FbeSearchPresets::SearchPreset;
struct Fixture { LPCWSTR positive; LPCWSTR negative; LPCWSTR output; };
const std::map<std::wstring, Fixture> kFixtures = {
 {L"design.normalize-spaces", {L"one   two", L"one two", L"one two"}}, {L"design.trim-before-punctuation", {L"word ,", L"word,", L"word,"}},
 {L"design.trim-leading", {L"  word", L"word", L"word"}}, {L"design.trim-trailing", {L"word  ", L"word", L"word"}}, {L"design.tabs-to-spaces", {L"a\tb", L"a b", L"a b"}},
 {L"design.nbsp-to-space", {L"a\x00A0" L"b", L"a b", L"a b"}}, {L"design_trim_after_opening", {L"( word", L"(word", L"(word"}}, {L"design_trim_before_closing", {L"word )", L"word)", L"word)"}},
 {L"design_trim_before_period", {L"word .", L"word.", L"word."}}, {L"design_ellipsis_three_dots", {L"wait...", L"wait..", L"wait…"}}, {L"design_ellipsis_spaced_dots", {L"wait. . .", L"wait..", L"wait…"}},
 {L"design_number_nbsp", {L"№ 12", L"№12", L"№\x00A0" L"12"}}, {L"design_section_nbsp", {L"§ 12", L"§12", L"§\x00A0" L"12"}},
 {L"design.duplicate-word", {L"тест тест", L"тест другой", NULL}}, {L"design.repeated-punctuation", {L"What??", L"What?", NULL}}, {L"design_hidden_characters", {L"a\x200B" L"b", L"a b", NULL}},
 {L"design_mixed_alphabets", {L"Тeст", L"Тест", NULL}}, {L"design_lower_to_upper_inside_word", {L"abC", L"abc", NULL}}, {L"design_digit_inside_word", {L"a1b", L"a1", NULL}},
 {L"design_punctuation_inside_word", {L"a,b", L"a, b", NULL}}, {L"design_uppercase_before_lowercase", {L"OCRword", L"Word", NULL}}, {L"design_apostrophe_cyrillic", {L"'тест", L"'Test", NULL}},
 {L"design_paragraph_starts_lowercase", {L"lower start", L"Upper start", NULL}}, {L"design_paragraph_missing_final_punctuation", {L"текст]", L"текст.", NULL}}, {L"design_lowercase_after_sentence", {L"One. next", L"One. Next", NULL}},
 {L"design_possible_missing_period", {L"word Next", L"word next", NULL}}, {L"design_repeated_quotes", {L"\"\"", L"\"one\"", NULL}}, {L"design_repeated_terminal_punctuation", {L"word,,", L"word,", NULL}},
 {L"design_straight_double_quotes", {L"\"word\"", L"«word»", NULL}}, {L"design_spaced_hyphen", {L"word - word", L"word-word", NULL}}, {L"design_number_ranges", {L"1 - 2", L"1–2", NULL}},
 {L"design_thousands_space", {L"1 000", L"1000", NULL}}, {L"design_initials_before_name", {L"И. И. Иванов", L"Иванов И.И.", NULL}}, {L"design_initials_after_name", {L"Иванов И.И.", L"Иванов Иван", NULL}},
 {L"design_roman_cyrillic_ha", {L"IХ", L"IX", NULL}}
};
struct FbeMatch
{
    size_t start;
    size_t length;
    CString value;
    std::vector<CString> submatches;
};

bool Match(const SearchPreset& preset, LPCWSTR subject, CString* replacementResult = NULL) {
 int error=0; PCRE2_SIZE offset=0;
 CString pattern(preset.findText);
 if (preset.wholeWord) pattern = AU::RegexBackend::BuildWholeWordRegexPattern(pattern);
 uint32_t flags=PCRE2_UTF|PCRE2_MULTILINE|(preset.matchCase?0:PCRE2_CASELESS);
 if (preset.unicodeProperties) flags|=PCRE2_UCP;
 pcre2_code* code=pcre2_compile((PCRE2_SPTR)(LPCWSTR)pattern,pattern.GetLength(),flags,&error,&offset,NULL); if(!code)return false;
 pcre2_match_data* data=pcre2_match_data_create_from_pattern(code,NULL); int result=data?pcre2_match(code,(PCRE2_SPTR)subject,wcslen(subject),0,0,data,NULL):PCRE2_ERROR_NOMEMORY;
 if (result >= 0 && replacementResult)
 {
     std::vector<FbeMatch> matches;
     PCRE2_SIZE searchOffset = 0;
     while (true)
     {
         const int count = pcre2_match(code, (PCRE2_SPTR)subject, wcslen(subject), searchOffset, 0, data, NULL);
         if (count < 0) break;
         PCRE2_SIZE* vector = pcre2_get_ovector_pointer(data);
         FbeMatch match;
         match.start = static_cast<size_t>(vector[0]);
         match.length = static_cast<size_t>(vector[1] - vector[0]);
         match.value = CString(subject + vector[0], static_cast<int>(match.length));
         for (int group = 1; group < count; ++group)
         {
             const PCRE2_SIZE groupStart = vector[group * 2];
             const PCRE2_SIZE groupEnd = vector[group * 2 + 1];
             match.submatches.push_back(groupStart == PCRE2_UNSET ? CString() : CString(subject + groupStart, static_cast<int>(groupEnd - groupStart)));
         }
         matches.push_back(match);
         searchOffset = vector[1];
         if (searchOffset >= wcslen(subject)) break;
     }
     CString output(subject);
     for (std::vector<FbeMatch>::reverse_iterator match = matches.rbegin(); match != matches.rend(); ++match)
     {
         output.Delete(static_cast<int>(match->start), static_cast<int>(match->length));
         AU::Search::ReplacementMatch replacementMatch;
         replacementMatch.Value = match->value;
         replacementMatch.SubMatches = match->submatches;
         std::vector<AU::Search::ReplacementFormattingRun> formatting;
         output.Insert(static_cast<int>(match->start), AU::Search::ExpandRegexReplacement(preset.replacementText, replacementMatch, formatting));
     }
     *replacementResult = output;
 }
 if(data)pcre2_match_data_free(data); pcre2_code_free(code); return result>=0;
}
const wchar_t* const kFinalPunctuationPositive[] = { L"Текст", L"Текст»", L"Текст)", L"Текст]", L"Текст}", NULL };
const wchar_t* const kFinalPunctuationNegative[] = { L"Текст.", L"Текст!", L"Текст?", L"Текст…", L"«Текст!»", L"«Текст?»", L"(Текст.)", L"[Текст!]", NULL };
}
int wmain(){std::vector<SearchPreset> presets;FbeSearchPresets::GetBuiltInPresets(FbeSearchPresets::SearchUiContext::Design,false,presets);for(size_t i=0;i<presets.size();++i){std::map<std::wstring,Fixture>::const_iterator it=kFixtures.find((LPCWSTR)presets[i].id);if(it==kFixtures.end()||!Match(presets[i],it->second.positive)||Match(presets[i],it->second.negative)){std::wcerr<<L"Design fixture failed: "<<(LPCWSTR)presets[i].id<<std::endl;return 1;} if(presets[i].id==L"design_paragraph_missing_final_punctuation"){for(size_t n=0;kFinalPunctuationPositive[n];++n)if(!Match(presets[i],kFinalPunctuationPositive[n]))return 4;for(size_t n=0;kFinalPunctuationNegative[n];++n)if(Match(presets[i],kFinalPunctuationNegative[n]))return 5;}if(presets[i].hasReplacement){CString output;if(!it->second.output||!Match(presets[i],it->second.positive,&output)||output!=it->second.output){std::wcerr<<L"Design replacement failed: "<<(LPCWSTR)presets[i].id<<std::endl;return 2;}}}return kFixtures.size()==presets.size()?0:3;}