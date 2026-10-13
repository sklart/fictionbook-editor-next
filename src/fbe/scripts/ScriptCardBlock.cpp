#include "stdafx.h"
#include "ScriptCardBlock.h"
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace ScriptCardBlock {
namespace {
constexpr char kOpen[] = "/*<FBE-SCRIPT-CARD version=\"1\">";
constexpr char kClose[] = "</FBE-SCRIPT-CARD>*/";
constexpr char kLegacyOpen[] = "/*<FBE-NEXT-CARD version=\"1\">";
constexpr char kLegacyClose[] = "</FBE-NEXT-CARD>*/";
constexpr size_t kMaximumCardBytes = 64 * 1024;

struct Encoding {
	size_t bom = 0;
	size_t unit = 1;
	bool bigEndian = false;
};

Encoding Detect(const BYTE* bytes, size_t length)
{
	if(length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
		return { 3, 1, false };
	if(length >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
		return { 2, 2, false };
	if(length >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
		return { 2, 2, true };
	if(length >= 2 && bytes[0] == '/' && bytes[1] == 0)
		return { 0, 2, false };
	if(length >= 2 && bytes[0] == 0 && bytes[1] == '/')
		return { 0, 2, true };
	return {};
}

bool Matches(const BYTE* bytes, size_t length, size_t position, const char* marker, const Encoding& encoding)
{
	const size_t characters = strlen(marker);
	if(position > length || characters > (length - position) / encoding.unit) return false;
	for(size_t index = 0; index < characters; ++index)
	{
		const size_t offset = position + index * encoding.unit;
		if(encoding.unit == 1)
		{
			if(bytes[offset] != static_cast<BYTE>(marker[index])) return false;
		}
		else if(encoding.bigEndian)
		{
			if(bytes[offset] != 0 || bytes[offset + 1] != static_cast<BYTE>(marker[index])) return false;
		}
		else if(bytes[offset] != static_cast<BYTE>(marker[index]) || bytes[offset + 1] != 0)
			return false;
	}
	return true;
}

bool ContainsMarker(const BYTE* bytes, size_t length, size_t start, const Encoding& encoding)
{
	for(size_t position = start; position < length; position += encoding.unit)
		if(Matches(bytes, length, position, kOpen, encoding) ||
			Matches(bytes, length, position, kLegacyOpen, encoding)) return true;
	return false;
}
}

Status Find(const BYTE* bytes, size_t length, Span& span)
{
	span = {};
	if(bytes == NULL) return Status::Malformed;
	const Encoding encoding = Detect(bytes, length);
	const size_t start = encoding.bom;
	const bool current = Matches(bytes, length, start, kOpen, encoding);
	const bool legacy = !current && Matches(bytes, length, start, kLegacyOpen, encoding);
	if(!current && !legacy)
		return ContainsMarker(bytes, length, start, encoding) ? Status::Malformed : Status::Missing;
	const char* closeMarker = current ? kClose : kLegacyClose;
	const size_t openLength = strlen(current ? kOpen : kLegacyOpen) * encoding.unit;
	const size_t limit = (std::min)(length, start + kMaximumCardBytes);
	const size_t closeLength = strlen(closeMarker) * encoding.unit;
	for(size_t cursor = start + openLength;
		cursor <= limit && closeLength <= limit - cursor; cursor += encoding.unit)
	{
		if(!Matches(bytes, length, cursor, closeMarker, encoding)) continue;
		size_t end = cursor + closeLength;
		if(Matches(bytes, length, end, "\r\n", encoding)) end += 2 * encoding.unit;
		else if(Matches(bytes, length, end, "\n", encoding) || Matches(bytes, length, end, "\r", encoding))
			end += encoding.unit;
		else return Status::Malformed;
		if(end - start > kMaximumCardBytes) return Status::Malformed;
		if(ContainsMarker(bytes, length, end, encoding)) return Status::Malformed;
		span = { start, end };
		return Status::Present;
	}
	return Status::Malformed;
}

CString Sha256WithoutCard(const BYTE* bytes, size_t length)
{
	Span span;
	const Status status = Find(bytes, length, span);
	if(status == Status::Malformed || length > ULONG_MAX) return CString();
	BCRYPT_ALG_HANDLE algorithm = NULL;
	BCRYPT_HASH_HANDLE hash = NULL;
	BYTE digest[32] = {};
	CString result;
	if(!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0)))
		return result;
	bool good = BCRYPT_SUCCESS(::BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0));
	if(good && status == Status::Present)
		good = BCRYPT_SUCCESS(::BCryptHashData(hash, const_cast<PUCHAR>(bytes), static_cast<ULONG>(span.begin), 0)) &&
			BCRYPT_SUCCESS(::BCryptHashData(hash, const_cast<PUCHAR>(bytes + span.end), static_cast<ULONG>(length - span.end), 0));
	else if(good)
		good = BCRYPT_SUCCESS(::BCryptHashData(hash, const_cast<PUCHAR>(bytes), static_cast<ULONG>(length), 0));
	if(good && BCRYPT_SUCCESS(::BCryptFinishHash(hash, digest, sizeof(digest), 0)))
		for(BYTE value : digest) result.AppendFormat(L"%02x", value);
	if(hash != NULL) ::BCryptDestroyHash(hash);
	::BCryptCloseAlgorithmProvider(algorithm, 0);
	return result;
}
}
