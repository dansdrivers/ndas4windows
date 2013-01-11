#pragma once
#include <functional>

template <> struct std::less<GUID>
{
	bool operator()(const GUID& x, const GUID& y) const 
	{
		return (::memcmp(&x,&y, sizeof(GUID)) < 0);
	}
};
