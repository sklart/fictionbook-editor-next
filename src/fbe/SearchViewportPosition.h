#pragma once

namespace FBESearchViewport
{
	struct PlacementInput
	{
		long scrollTop;
		long scrollHeight;
		long clientHeight;
		long matchTop;
		long preferredTop;
		long obstructionTop;
		long obstructionBottom;
		long obstructionMargin;
		bool obstructionOverlapsViewport;
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

	inline long ScrollTopForMatch(const PlacementInput& input)
	{
		const long maximumScroll = input.scrollHeight > input.clientHeight ? input.scrollHeight - input.clientHeight : 0;
		long targetTop = Clamp(input.preferredTop, 0, input.clientHeight > 0 ? input.clientHeight - 1 : 0);
		if(input.obstructionOverlapsViewport && targetTop >= input.obstructionTop && targetTop < input.obstructionBottom)
			targetTop = Clamp(input.obstructionBottom + input.obstructionMargin, 0, input.clientHeight > 0 ? input.clientHeight - 1 : 0);
		return Clamp(input.scrollTop + input.matchTop - targetTop, 0, maximumScroll);
	}
}
