#include "ptr_array.hh"
#include <algorithm>

template<typename T, unsigned Dimension>
T* pointer_array<T,Dimension>::Get(std::size_t index) const
{
    if(bitset)
    {
        for(unsigned n=1; n<num_distinct; ++n)
            if(bitset->test(index + (n-1)*Dimension))
                return distinct_options[n];
        return distinct_options[0];
    }
    return distinct_options[index];
}

template<typename T, unsigned Dimension>
void pointer_array<T,Dimension>::Set(std::size_t index, T* ptr)
{
    if(bitset)
    {
        unsigned first_null = ~0u;
        for(unsigned n=0; n<num_distinct; ++n)
        {
            if(ptr == distinct_options[n])
            {
                for(unsigned m=1; m<num_distinct; ++m)
                    if(m == n)
                        bitset->set(   index + (m-1)*Dimension );
                    else
                        bitset->reset( index + (m-1)*Dimension );
                return;
            }
            if(first_null == ~0u && !distinct_options[n])
                first_null = n;
        }
        if(first_null != ~0u)
        {
            distinct_options[first_null] = ptr;
            if(first_null > 0)
                bitset->set(   index + (first_null-1)*Dimension );
            return;
        }
        // We already have a full set of distinct options.
        // Convert the distinct_options array into an array of Dimension.
        T** p = distinct_options;
        distinct_options = new T* [Dimension];
        for(unsigned n=0; n<Dimension; ++n)
            distinct_options[n] = p[0];
        for(unsigned n=0; n<Dimension * (num_distinct-1); ++n)
            if(bitset->test(n))
                distinct_options[n%Dimension] = p[ (n/Dimension) + 1];
        delete[] p;
        delete bitset;
        bitset = nullptr;
    }
    distinct_options[index] = ptr;
}

template<typename T, unsigned Dimension>
pointer_array<T,Dimension>::pointer_array()
    : distinct_options{ new T* [num_distinct] },
      bitset{ new std::bitset<Dimension * (num_distinct-1)> }
{
    for(unsigned n=0; n<num_distinct; ++n)
        distinct_options[n] = 0;
}

template<typename T, unsigned Dimension>
pointer_array<T,Dimension>::~pointer_array()
{
    Clean();
}


template<typename T, unsigned Dimension>
pointer_array<T,Dimension>::pointer_array(pointer_array&& b)
    : distinct_options(b.distinct_options),
      bitset(b.bitset)
{
    b.distinct_options = nullptr;
    b.bitset           = nullptr;
}

template<typename T, unsigned Dimension>
auto pointer_array<T,Dimension>::operator=(pointer_array&& b) -> pointer_array&
{
    Clean();

    distinct_options = b.distinct_options;
    bitset           = b.bitset;
    b.distinct_options = nullptr;
    b.bitset           = nullptr;
    return *this;
}

template<typename T, unsigned Dimension>
void pointer_array<T,Dimension>::Clean()
{
    T** end = distinct_options + (bitset ? num_distinct : Dimension);
    std::sort(distinct_options, end);
    for(T *prev = nullptr, **p = distinct_options; p != end; ++p)
        if(*p != prev)
        {
            delete *p;
            prev = *p;
        }
    delete bitset;
    delete[] distinct_options;
}
