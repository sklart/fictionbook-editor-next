#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "SpellParagraphTraversal.h"

struct Node
{
	std::string tag;
	Node* parent = NULL;
	Node* first = NULL;
	Node* next = NULL;
};

struct Accessor
{
	Node* FirstChild(Node* node) const { return node->first; }
	Node* NextSibling(Node* node) const { return node->next; }
	Node* Parent(Node* node) const { return node->parent; }
	bool IsParagraph(Node* node) const { return node->tag == "P"; }
};

static void Need(bool value, const char* message)
{
	if (!value) { std::cerr << message << std::endl; std::exit(1); }
}

static void Append(Node& parent, Node& child)
{
	child.parent = &parent;
	if (!parent.first) { parent.first = &child; return; }
	Node* last = parent.first;
	while (last->next) last = last->next;
	last->next = &child;
}

int main()
{
	Accessor accessor;
	Node body = { "DIV" };
	std::vector<Node> paragraphs(25);
	for (size_t index = 0; index < paragraphs.size(); ++index)
	{
		paragraphs[index].tag = "P";
		Append(body, paragraphs[index]);
	}
	Node* current = &paragraphs[0];
	for (size_t index = 1; index < paragraphs.size(); ++index)
	{
		current = FbeNextParagraphInDocumentOrder(current, &body, accessor);
		Need(current == &paragraphs[index], "more than twenty visible paragraphs");
	}
	Need(FbeNextParagraphInDocumentOrder(current, &body, accessor) == NULL, "body boundary");

	Node nestedBody = { "DIV" }, section = { "DIV" }, cite = { "DIV" }, poem = { "DIV" }, stanza = { "DIV" };
	Node first = { "P" }, second = { "P" }, third = { "P" }, fourth = { "P" };
	Append(nestedBody, section); Append(nestedBody, cite);
	Append(section, first); Append(section, poem); Append(poem, stanza); Append(stanza, second);
	Append(cite, third); Append(cite, fourth);
	Need(FbeNextParagraphInDocumentOrder(&first, &nestedBody, accessor) == &second, "section poem stanza descent");
	Need(FbeNextParagraphInDocumentOrder(&second, &nestedBody, accessor) == &third, "neighboring nested div containers");
	Need(FbeNextParagraphInDocumentOrder(&third, &nestedBody, accessor) == &fourth, "cite paragraphs");
	return 0;
}
