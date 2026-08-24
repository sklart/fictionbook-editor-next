#include "stdafx.h"
#include "UpdateVersion.h"

namespace
{
	bool IsDigits(const CString& value)
	{
		if (value.IsEmpty()) return false;
		for (int i = 0; i < value.GetLength(); ++i)
			if (value[i] < L'0' || value[i] > L'9') return false;
		return true;
	}

	bool IsIdentifier(const CString& value)
	{
		if (value.IsEmpty()) return false;
		for (int i = 0; i < value.GetLength(); ++i)
			if (!((value[i] >= L'0' && value[i] <= L'9') ||
				(value[i] >= L'a' && value[i] <= L'z') ||
				(value[i] >= L'A' && value[i] <= L'Z') || value[i] == L'-')) return false;
		return true;
	}

	bool Parse(const CString& input, CString& base, CString& prerelease)
	{
		CString value(input); value.Trim();
		if (value.IsEmpty()) return false;
		if (value.Find(L"..") >= 0 || value[0] == L'.' || value[value.GetLength() - 1] == L'.') return false;
		const int plus = value.Find(L'+');
		if (plus >= 0) {
			CString metadata = value.Mid(plus + 1);
			if (metadata.IsEmpty()) return false;
			int pos = 0; CString item;
			while (!(item = metadata.Tokenize(L".", pos)).IsEmpty()) if (!IsIdentifier(item)) return false;
			value = value.Left(plus);
		}
		const int dash = value.Find(L'-');
		base = dash < 0 ? value : value.Left(dash);
		prerelease = dash < 0 ? CString() : value.Mid(dash + 1);
		int pos = 0; CString part; int count = 0;
		while (!(part = base.Tokenize(L".", pos)).IsEmpty()) {
			if (!IsDigits(part) || (part.GetLength() > 1 && part[0] == L'0')) return false;
			++count;
		}
		if (count != 3 || (dash >= 0 && prerelease.IsEmpty())) return false;
		if (!prerelease.IsEmpty()) {
			pos = 0;
			while (!(part = prerelease.Tokenize(L".", pos)).IsEmpty())
				if (!IsIdentifier(part) || (IsDigits(part) && part.GetLength() > 1 && part[0] == L'0')) return false;
		}
		return true;
	}

	int CompareNumeric(const CString& left, const CString& right)
	{
		if (left.GetLength() != right.GetLength()) return left.GetLength() < right.GetLength() ? -1 : 1;
		const int result = left.Compare(right); return result < 0 ? -1 : result > 0 ? 1 : 0;
	}
}

bool IsValidUpdateVersion(const CString& value) { CString base, prerelease; return Parse(value, base, prerelease); }
bool IsValidReleaseTag(const CString& value) { return value.GetLength() > 1 && value[0] == L'v' && IsValidUpdateVersion(value.Mid(1)); }
bool IsPrereleaseUpdateVersion(const CString& value) { CString base, prerelease; return Parse(value, base, prerelease) && !prerelease.IsEmpty(); }
CString GetUpdateBaseVersion(const CString& value) { CString base, prerelease; return Parse(value, base, prerelease) ? base : CString(); }

int CompareUpdateVersions(const CString& left, const CString& right)
{
	CString leftBase, leftPre, rightBase, rightPre;
	if (!Parse(left, leftBase, leftPre) || !Parse(right, rightBase, rightPre)) return 0;
	int lp = 0, rp = 0; CString lpart, rpart;
	while (!(lpart = leftBase.Tokenize(L".", lp)).IsEmpty() && !(rpart = rightBase.Tokenize(L".", rp)).IsEmpty()) {
		int result = CompareNumeric(lpart, rpart); if (result) return result;
	}
	if (leftPre.IsEmpty() || rightPre.IsEmpty()) return leftPre.IsEmpty() == rightPre.IsEmpty() ? 0 : (leftPre.IsEmpty() ? 1 : -1);
	lp = rp = 0;
	for (;;) {
		lpart = leftPre.Tokenize(L".", lp); rpart = rightPre.Tokenize(L".", rp);
		if (lpart.IsEmpty() || rpart.IsEmpty()) return lpart.IsEmpty() == rpart.IsEmpty() ? 0 : (lpart.IsEmpty() ? -1 : 1);
		const bool ln = IsDigits(lpart), rn = IsDigits(rpart);
		int result = ln && rn ? CompareNumeric(lpart, rpart) : (ln != rn ? (ln ? -1 : 1) : lpart.Compare(rpart));
		if (result) return result < 0 ? -1 : 1;
	}
}
