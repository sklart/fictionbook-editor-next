#pragma once

namespace FBESearchViewport
{
	struct Rect
	{
		long left;
		long top;
		long right;
		long bottom;
	};

	struct PlacementInput
	{
		long scrollTop;
		long scrollHeight;
		long clientHeight;
		Rect match;
		long preferredTop;
		long obstructionMargin;
		const Rect* obstructions;
		unsigned obstructionCount;
	};

	inline long Clamp(long value, long minimum, long maximum)
	{
		if(value < minimum) return minimum;
		if(value > maximum) return maximum;
		return value;
	}

	inline long MinimumContextTopForDpi(unsigned dpi)
	{
		return static_cast<long>((64u * (dpi ? dpi : 96u) + 48u) / 96u);
	}

	inline long PreferredMatchTop(long clientHeight, unsigned dpi)
	{
		const long maximum = clientHeight > 0 ? clientHeight - 1 : 0;
		const long relative = clientHeight * 18 / 100;
		return Clamp(relative > MinimumContextTopForDpi(dpi) ? relative : MinimumContextTopForDpi(dpi), 0, maximum);
	}

	inline long ScaleToViewport(long screenPixels, long viewportPixels, long viewportCssPixels)
	{
		return viewportPixels > 0 ? screenPixels * viewportCssPixels / viewportPixels : 0;
	}

	inline bool Intersects(const Rect& first, const Rect& second)
	{
		return first.right > second.left && first.left < second.right && first.bottom > second.top && first.top < second.bottom;
	}

	// Move only when the match is obscured. Coordinates are screen pixels in
	// the same DPI context; preserve the user's other axis whenever possible.
	inline bool DialogPositionForMatch(const Rect& dialog, const Rect& viewport,
		const Rect& work, const Rect& match, Rect& result)
	{
		if (!Intersects(dialog, match)) return false;
		const long width = dialog.right - dialog.left, height = dialog.bottom - dialog.top;
		if (width <= 0 || height <= 0 || width > work.right - work.left || height > work.bottom - work.top) return false;
		const long left = Clamp(dialog.left, work.left, work.right - width);
		const long top = Clamp(dialog.top, work.top, work.bottom - height);
		const bool matchAbove = match.top + (match.bottom - match.top) / 2 < viewport.top + (viewport.bottom - viewport.top) / 2;
		const bool matchLeft = match.left + (match.right - match.left) / 2 < viewport.left + (viewport.right - viewport.left) / 2;
		const long upper = Clamp(viewport.top, work.top, work.bottom - height);
		const long lower = Clamp(viewport.bottom - height, work.top, work.bottom - height);
		const long west = Clamp(viewport.left, work.left, work.right - width);
		const long east = Clamp(viewport.right - width, work.left, work.right - width);
		const Rect candidates[] = {
			{ left, matchAbove ? lower : upper, left + width, (matchAbove ? lower : upper) + height },
			{ left, matchAbove ? upper : lower, left + width, (matchAbove ? upper : lower) + height },
			{ matchLeft ? east : west, top, (matchLeft ? east : west) + width, top + height },
			{ matchLeft ? west : east, top, (matchLeft ? west : east) + width, top + height }
		};
		for (const Rect& candidate : candidates) {
			if (!Intersects(candidate, match)) { result = candidate; return true; }
		}
		return false;
	}

	inline long ScrollTopForMatch(const PlacementInput& input)
	{
		const long maximumScroll = input.scrollHeight > input.clientHeight ? input.scrollHeight - input.clientHeight : 0;
		const long matchHeight = input.match.bottom > input.match.top ? input.match.bottom - input.match.top : 1;
		const long maximumTop = input.clientHeight > matchHeight ? input.clientHeight - matchHeight : 0;
		long targetTop = Clamp(input.preferredTop, 0, maximumTop);
		for(unsigned pass = 0; pass <= input.obstructionCount; ++pass)
		{
			Rect target = { input.match.left, targetTop, input.match.right, targetTop + matchHeight };
			bool moved = false;
			for(unsigned index = 0; index < input.obstructionCount; ++index)
				if(Intersects(target, input.obstructions[index])) { targetTop = Clamp(input.obstructions[index].bottom + input.obstructionMargin, 0, maximumTop); moved = true; break; }
			if(!moved) break;
		}
		return Clamp(input.scrollTop + input.match.top - targetTop, 0, maximumScroll);
	}
}
