#pragma once

// State owned by one BODY <-> SOURCE representation transition.  The DOM
// ranges themselves remain owned by view orchestration because they also participate
// in BODY and DESC view lifecycle.
struct BodySourceSelectionState
{
	bool bodyToSourceTransferred = false;
	bool sourceToBodyTransferred = false;
	int sourceStart = 0;
	int sourceEnd = 0;

	void Reset()
	{
		bodyToSourceTransferred = false;
		sourceToBodyTransferred = false;
		sourceStart = 0;
		sourceEnd = 0;
	}
};
