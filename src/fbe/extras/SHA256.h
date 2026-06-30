#pragma once

#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

class FCCrypt
{
public:
	static CString Get_SHA256(const void* data, int length)
	{
		if (!data || length < 0)
			return CString();

		BCRYPT_ALG_HANDLE algorithm = NULL;
		BCRYPT_HASH_HANDLE hash = NULL;
		BYTE digest[32] = {};
		CString result;

		if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm,
			BCRYPT_SHA256_ALGORITHM, NULL, 0)))
		{
			return result;
		}

		if (BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0)) &&
			BCRYPT_SUCCESS(BCryptHashData(hash,
				static_cast<PUCHAR>(const_cast<void*>(data)),
				static_cast<ULONG>(length), 0)) &&
			BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0)))
		{
			for (BYTE value : digest)
				result.AppendFormat(L"%02x", value);
		}

		if (hash)
			BCryptDestroyHash(hash);
		BCryptCloseAlgorithmProvider(algorithm, 0);
		return result;
	}
};
