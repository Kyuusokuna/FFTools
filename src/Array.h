#pragma once
#include <assert.h>
#include <stdlib.h>
#include <initializer_list>
#include <algorithm>

template<typename T, size_t capacity_increase = 16> struct Array {
	static_assert(capacity_increase > 0, "Array capacity_increase needs to be > 0");

	size_t count = 0;
	size_t capacity = 0;
	T *values = 0;

	Array() { }
	Array(std::initializer_list<T> arr) : Array() { for (auto t : arr) add(t); }

	T *begin() {
		return values;
	}

	T *end() {
		return values + count;
	}

	T &operator[](size_t index) {
		assert(index < count);
		return values[index];
	}

	void ensure_capacity(size_t space) {
		if (capacity < space) {
			if (!values) {
				values = (T *)malloc(sizeof(T) * space);
				capacity = space;
			} else {
				values = (T *)realloc(values, sizeof(T) * space);
				capacity = space;
			}
		}
	}

	T &add(T to_add) {
		if (count == capacity) {
			if (!values) {
				values = (T *)malloc(sizeof(T) * capacity_increase);
				capacity = capacity_increase;
			} else {
				values = (T *)realloc(values, sizeof(T) * (capacity + capacity_increase));
				capacity += capacity_increase;
			}
		}

		values[count] = to_add;
		count++;
		return values[count - 1];
	}

	void remove(size_t index) {
		assert(index < count);

		count--;

		if (count)
			values[index] = values[count];
	}

	void remove_ordered(size_t index) {
		assert(index < count);

		count--;

		if (count) {
			for (int i = index; i < count; i++)
				values[i] = values[i + 1];
		}
	}

	void clear() {
		count = 0;
	}

	void reset() {
		count = 0;
		capacity = 0;

		if (values)
			free(values);
		values = 0;
	}

	T *first() {
		return values;
	}

	T *last() {
		if(count)
			return values + count - 1;
		return 0;
	}

	Array<T> copy() {
		Array<T> result = {};
		result.ensure_capacity(count);
		for (size_t i = 0; i < count; i++)
			result.add(values[i]);
		return result;
	}

	void sort(bool comparison_function(const T &a, const T &b)) {
		std::stable_sort(begin(), end(), comparison_function);
	}

	template<typename Predicate>
	bool Any(Predicate &&predicate) {
		for (auto &e : *this)
			if (predicate(e))
				return true;

		return false;
	}
};