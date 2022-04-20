#pragma once
#include "types.h"
#include "string.h"
#include "Array.h"

struct Byte_View {
	u8 *data;
	u64 length;

	Byte_View() { }
	Byte_View(u8 *data, u64 length) : data(data), length(length) { }
	Byte_View(String str) : data((u8 *)str.data), length(str.length) { }

	Byte_View(Byte_View source, u64 offset) :
		data(source.data + min(offset, source.length)),
		length(source.length - min(offset, source.length)) {
	}

	Byte_View(Byte_View source, u64 offset, u64 new_length) :
		data(source.data + min(offset, source.length)),
		length(min(source.length - min(offset, source.length), new_length)) {
	}

	u8 operator[](u64 index) {
		if (index >= length) {
			assert(index < length);
			return 0;
		}

		return data[index];
	}
};

template<typename T>
struct Type_View {
	T *data;

	Type_View(Byte_View source) : data(source.length >= sizeof(T) ? (T *)source.data : 0) { }
	Type_View(Byte_View source, u64 offset) : Type_View(Byte_View(source, offset)) { }

	T *operator->() {
		return data;
	}

	T &operator*() {
		return *data;
	}

	explicit operator bool() {
		return !!data;
	}
};

template<typename T>
struct Array_View {
	T *data;
	u64 count;

	Array_View() : data(0), count(0) { }
	Array_View(Byte_View source, u64 count) : data((T *)source.data), count(min(count, source.length / sizeof(T))) { }
	Array_View(Byte_View source, u64 count, u64 offset) : Array_View(Byte_View(source, offset), count) { }
	Array_View(Array<T> source) : data(source.values), count(source.count) { }

	T &operator[](u64 index) {
		return data[index];
	}

	T *begin() {
		return data;
	}

	T *end() {
		return data + count;
	}

	operator Byte_View() {
		return Byte_View((u8 *)data, count * sizeof(T));
	}
};

template<typename T>
struct Array_View2D {
	T *data;
	u64 num_elements_per_row;
	u64 num_rows;

	Array_View2D(Byte_View source, u64 num_elements_per_row, u64 num_rows) : 
		data((T *)source.data), 
		num_elements_per_row(num_elements_per_row), 
		num_rows(min(num_rows, source.length / (sizeof(T) * num_elements_per_row))) { }

	Array_View<T> operator[](u64 row_index) {
		return Array_View<T>(Byte_View(data + num_elements_per_row * row_index, num_elements_per_row * sizeof(T)), num_elements_per_row);
	}
};