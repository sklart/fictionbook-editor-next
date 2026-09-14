#include "stdafx.h"
#include "SelectionRangeMapper.h"

namespace FbeDom {

int SelectionRangeMapper::GetRangePos(const MSHTML::IHTMLTxtRangePtr& range, MSHTML::IHTMLElementPtr& element, int& pos)
{
	MSHTML::IHTMLTxtRangePtr tr(range->duplicate());
	tr->collapse(VARIANT_TRUE);
	element = tr->parentElement();
	MSHTML::IHTMLTxtRangePtr btr(range->duplicate());
	btr->moveToElementText(element);
	btr->collapse(VARIANT_TRUE);
	pos = 0;
	MSHTML::IHTMLDOMNodePtr node(element);
	if(!(bool)node) return 0;
	int count = 0;
	int cuttedchars = 0;
	node = node->firstChild;
	while(node)
	{
		MSHTML::IHTMLDOMTextNodePtr textNode(node);
		if(!(bool)textNode)
		{
			int skip = CountNodeChars(node);
			cuttedchars += skip;
			btr->move(L"character", skip);
		}
		else
		{
			int skip = count + textNode->length;
			btr->move(L"character", skip);
			if(btr->compareEndPoints(L"StartToStart", tr) != -1)
			{
				btr->move(L"character", -skip);
				break;
			}
			pos += skip;
		}
		node = node->nextSibling;
	}
	int k = btr->compareEndPoints(L"StartToStart", tr);
	if(k == -1)
	{
		int res = btr->move(L"character", 1);
		if (res != 1) return 0;
		if(btr->compareEndPoints(L"StartToStart", tr) != 1) ++pos;
	}
	while(btr->compareEndPoints(L"StartToStart", tr) == -1)
	{
		++pos;
		int res = btr->move(L"character", 1);
		if (res != 1) return 0;
	}
	return pos;
}

bool SelectionRangeMapper::GetSelectionInfo(MSHTML::IHTMLTxtRangePtr range, MSHTML::IHTMLElementPtr* begin, MSHTML::IHTMLElementPtr* end, int* beginChar, int* endChar)
{
	*beginChar = 0;
	*endChar = 0;
	if(!range) return false;
	int b = 0;
	int e = 0;
	bstr_t text = range->text;
	MSHTML::IHTMLTxtRangePtr tr(range->duplicate());
	tr->collapse(VARIANT_TRUE);
	*begin = tr->parentElement();
	if (!(bool)(*begin)) return false;
	GetRangePos(tr, *begin, b);
	tr = range->duplicate();
	tr->collapse(VARIANT_FALSE);
	*end = tr->parentElement();
	GetRangePos(tr, *end, e);
	MSHTML::IHTMLDOMNodePtr nodeb(*begin);
	MSHTML::IHTMLDOMNodePtr nodee(*end);
	if(!(bool)nodeb || !(bool)nodee) return false;
	*beginChar = b;
	*endChar = e;
	return true;
}

bool SelectionRangeMapper::GetSelectionInfo(MSHTML::IHTMLControlRangePtr range, MSHTML::IHTMLElementPtr* begin, MSHTML::IHTMLElementPtr* end, int* beginChar, int* endChar)
{
	*beginChar = 0;
	*endChar = 0;
	if(!range || range->length <= 0) return false;
	*begin = range->item(0);
	*end = range->item(range->length - 1);
	return true;
}

MSHTML::IHTMLTxtRangePtr SelectionRangeMapper::SetSelection(MSHTML::IHTMLDocument2Ptr document, MSHTML::IHTMLElementPtr begin, MSHTML::IHTMLElementPtr end, int beginPos, int endPos)
{
	if(!(bool)begin) return 0;
	if(!(bool)end) { end = begin; endPos = beginPos; }
	beginPos = GetRealCharPos(begin, beginPos);
	endPos = GetRealCharPos(end, endPos);
	MSHTML::IHTMLTxtRangePtr rng(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
	if(!(bool)rng) return 0;
	MSHTML::IHTMLTxtRangePtr rngBegin(rng->duplicate());
	rngBegin->moveToElementText(begin);
	rngBegin->collapse(VARIANT_TRUE);
	rngBegin->moveStart(L"character", beginPos);
	if(begin == end)
	{
		rngBegin->moveEnd(L"character", endPos - beginPos);
		rngBegin->select();
		return rngBegin;
	}
	MSHTML::IHTMLTxtRangePtr rngEnd(rng->duplicate());
	rngEnd->moveToElementText(end);
	rngEnd->moveStart(L"character", endPos);
	rngBegin->setEndPoint(L"EndToStart", rngEnd);
	rngBegin->select();
	return rngBegin;
}

int SelectionRangeMapper::GetRelationalCharPos(MSHTML::IHTMLDOMNodePtr node, int pos)
{
	if(!(bool)node) return 0;
	int relpos = 0;
	int cuttedchars = 0;
	node = node->firstChild;
	while(node)
	{
		MSHTML::IHTMLDOMTextNodePtr textNode(node);
		if(!(bool)textNode) cuttedchars += CountNodeChars(node);
		else
		{
			if(relpos + cuttedchars + textNode->length >= pos) return pos - cuttedchars;
			relpos += textNode->length;
		}
		node = node->nextSibling;
	}
	return 0;
}

int SelectionRangeMapper::GetRealCharPos(MSHTML::IHTMLDOMNodePtr node, int pos)
{
	if(!(bool)node) return 0;
	int realpos = 0;
	int cuttedchars = 0;
	node = node->firstChild;
	while(node)
	{
		MSHTML::IHTMLDOMTextNodePtr textNode(node);
		if(!(bool)textNode) cuttedchars += CountNodeChars(node);
		else
		{
			if((realpos + textNode->length) >= pos) return pos + cuttedchars;
			realpos += textNode->length;
		}
		node = node->nextSibling;
	}
	return 0;
}

int SelectionRangeMapper::CountNodeChars(MSHTML::IHTMLDOMNodePtr node)
{
	if(!(bool)node) return 0;
	int count = 0;
	node = node->firstChild;
	while(node)
	{
		MSHTML::IHTMLDOMTextNodePtr textNode(node);
		if(!(bool)textNode) count += CountNodeChars(node);
		else count += textNode->length;
		node = node->nextSibling;
	}
	return count;
}

} // namespace FbeDom
