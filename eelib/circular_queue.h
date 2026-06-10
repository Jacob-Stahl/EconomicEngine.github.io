#pragma once

#include <vector>

template <typename T>
class circbuffer{
    size_t start = 0;
    size_t end = 0;
    std::vector<T> elements;

    public:
        circbuffer(size_t capacity_ = 256) : elements(capacity_){}
        void reserve(size_t capacity){
            elements.reserve(capacity);
        }

        void push(T&& newElement){
            elements[end]
        }

};
