#pragma once

#include <cstdlib>
#include <stdint.h>
#include <type_traits>

#define CACHELINE_SIZE		  64
#define STACK_SIZE(T)		  (CACHELINE_SIZE - 2 * sizeof(uint16_t) - sizeof(T *)) / sizeof(T)

#define EXP48BP_PTR_SIZE(T)	  (3 * sizeof(T)) / 4
#define EXP48BP_STACK_SIZE(T) (CACHELINE_SIZE - sizeof(uint16_t) - sizeof(int16_t) - sizeof(char *)) / (3 * sizeof(T) / 4)

template <typename T> class Stack {
	static_assert(std::is_trivially_copyable<T>::value, "template T must be trivially copyable");

private:
	uint16_t index;
	T stack_allocated_array[STACK_SIZE(T)];
	uint16_t _n_memb_heap;
	T *heap_allocated_array;

public:
	Stack() noexcept {
		index = 0;
		heap_allocated_array = nullptr;
		_n_memb_heap = 0;
	}

	~Stack() noexcept { free(heap_allocated_array); }

	/*
	appends `t` to the stack. if the stack size is small enough,
	then the value is pushed onto the stack (`stack_allocated_array`)
	otherwise, it is pushed onto a heap allocated array
	(that may be allocated in this function)
	*/
	void push(T t) noexcept {
		const int _n_memb_stack = STACK_SIZE(T);
		if (index < _n_memb_stack) {
			stack_allocated_array[index++] = t;
		} else if (heap_allocated_array != nullptr && index - _n_memb_stack < _n_memb_heap) {
			heap_allocated_array[index++ - _n_memb_stack] = t;
		} else if (heap_allocated_array != nullptr) {
			heap_allocated_array = static_cast<T *>(realloc(heap_allocated_array, _n_memb_heap * 2 * sizeof(T)));
			// note: heap_allocated_array could be NULL now :)
			_n_memb_heap *= 2;
			heap_allocated_array[index++ - _n_memb_stack] = t;
		} else {
			heap_allocated_array = static_cast<T *>(std::aligned_alloc(CACHELINE_SIZE, 64 * sizeof(T)));
			_n_memb_heap = 64;
			heap_allocated_array[index++ - _n_memb_stack] = t;
		}
	}

	T top() noexcept {
		const int _n_memb_stack = STACK_SIZE(T);
		if (index <= _n_memb_stack) {
			return stack_allocated_array[index - 1];
		} else {
			return heap_allocated_array[index - _n_memb_stack - 1];
		}
	}

	/*
	removes and returns the top value of the stack.
	note that this may return from stack allocated
	or heap allocated memory
	*/
	T pop() noexcept {
		T t = top();
		index--;
		return t;
	}

	inline int size() { return index; }
};
