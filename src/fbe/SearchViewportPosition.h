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
