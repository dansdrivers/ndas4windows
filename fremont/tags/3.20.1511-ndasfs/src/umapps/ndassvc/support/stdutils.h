#pragma once

/*
template <typename T, size_t BLOCK_SIZE = sizeof(T) >
struct mem_block_less :
	public std::binary_function<T, T, bool>
{
	bool operator()(const T& x, const T& y) const {
		return (::memcmp(&x,&y, BLOCK_SIZE) < 0);
	}
};
*/

struct less_GUID :
	public std::binary_function<GUID, GUID, bool>
{
	bool operator()(const GUID& x, const GUID& y) const {
		return (::memcmp(&x,&y, sizeof(GUID)) < 0);
	}
};

