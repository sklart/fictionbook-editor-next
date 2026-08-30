#pragma once

// Walks a DOM subtree in document order without materialising a collection of
// every paragraph. Accessor supplies the DOM-specific navigation primitives.
template <typename Node, typename Accessor>
Node FbeNextParagraphInDocumentOrder(Node current, Node boundary, Accessor accessor)
{
	Node node = current;
	while (node && node != boundary)
	{
		Node child = accessor.FirstChild(node);
		if (child)
		{
			node = child;
		}
		else
		{
			Node sibling = accessor.NextSibling(node);
			while (!sibling && node && node != boundary)
			{
				node = accessor.Parent(node);
				if (node && node != boundary)
					sibling = accessor.NextSibling(node);
			}
			if (!node || node == boundary || !sibling)
				return Node();
			node = sibling;
		}
		if (node != boundary && accessor.IsParagraph(node))
			return node;
	}
	return Node();
}
