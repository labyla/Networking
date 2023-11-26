#pragma once

struct Buffer {
	const void* Data;
	int DataSize;

	Buffer(const void* _Data, int _DataSize)
		: Data(_Data),
		DataSize(_DataSize) {}
};