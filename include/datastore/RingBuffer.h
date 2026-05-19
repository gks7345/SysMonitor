#pragma once
#include <iostream>
#include <queue>
#include <cstring>
#include <stdexcept>

template <typename Q>
class RingBuffer {
private:
	size_t size, capacity, frontIndex, backIndex;
	Q* data;
public:
	RingBuffer(size_t capacity) : size(0), frontIndex(0), backIndex(0), capacity(capacity) {
		data = new Q * [capacity];
	}
	~RingBuffer() {
		delete[] data;
	}

	void enQueue(const Q* q, size_t block) {
		if (block == 0) {
			return;
		}
		size_t ableCapacity = capacity - size;

		//새로 추가될 q의 크기가 사용 가능한 capacity를 넘어가지 않을때
		if (block < ableCapacity) {
			for (size_t i = 0; i <= block; i++) {
				data[backIndex] = *q[i];

				back_index++;
				//만약 back_index가 capacity를 넘어가면 back_index는 맨 앞으로 회귀
				if (backIndex > capacity) {
					backIndex = 0;
				}
			}
			size = size + block;
			return;
		}
		//새로 추가될 q의 크기가 사용 가능한 capacity를 넘어갈때 에러 발생
		if (block > ableCapacity) {
			throw std::exception();
		}
	}

	Q deQueue() {
		//front_index 뒤에 back_index가 있을때
		if (frontIndex < backIndex) {
			Q data_to_pop[size] = 0;
			size_t i = 0;

			for (frontIndex; frontIndex <= backIndex; frontIndex++) {
				data_to_pop[i] = data[frontIndex];
				data[frontIndex] = NULL;
			}
			size = 0;
			return data_to_pop;
		}

		//front_index 앞에 back_index가 있을때
		if (frontIndex > backIndex) {
			Q data_to_pop[size] = 0;
			size_t i = 0;
			for (frontIndex; frontIndex <= capacity; frontIndex++) {
				data_to_pop[i] = data[frontIndex];
				data[frontIndex] = NULL;
				i++
			}
			for (frontIndex = 0; frontIndex <= backIndex; frontIndex++) {
				data_to_pop[i] = data[frontIndex];
				data[frontIndex] = NULL;
				i++
			}
			size = 0;
			return data_to_pop;
		}
		return NULL;
	}

	std::vector<std::unique_ptr<Q>> popQueue() {
		auto queue_ = std::make_unique
			if (backIndex)
	}

	bool isEmpty() {
		if (size = 0)
			return true;
		return false;
	}
};