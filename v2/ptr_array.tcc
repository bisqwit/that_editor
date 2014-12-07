#include "ptr_array.hh"
#include <algorithm>
#include <type_traits>

template<typename T, unsigned Dimension>
T pointer_array<T,Dimension>::Get(std::size_t index) const
{
    if(bitsets.empty() && distinct_options.size() > 1)
        return distinct_options[index];

    for(std::size_t b=bitsets.size(), a=0; a<b; ++a)
        if(bitsets[a].test(index))
            return distinct_options[a+1];
    return distinct_options[0];
}

template<typename T, unsigned Dimension>
void pointer_array<T,Dimension>::Set(std::size_t index, T ptr)
{
    if(bitsets.empty() && distinct_options.size() > 1)
    {
        distinct_options[index] = (ptr);
        return;
    }

    std::size_t found_pos = distinct_options.size();
    for(std::size_t b=distinct_options.size(), a=0; a<b; ++a)
        if(distinct_options[a] == ptr)
        {
            found_pos = a;
            break;
        }
    if(found_pos == distinct_options.size())
    {
        if(found_pos >= max_distinct)
        {
            // Convert into a flat array
            std::vector<T> o(Dimension, distinct_options[0]);
            for(std::size_t b=bitsets.size(), a=0; a<b; ++a)
                for(unsigned c=0; c<Dimension; ++c)
                    if(bitsets[a].test(c))
                        o[c] = distinct_options[a+1];

            distinct_options.swap(o);
            bitsets.clear();

            distinct_options[index] = (ptr);
            return;
        }
        if(!distinct_options.empty())
            bitsets.emplace_back(  );
        distinct_options.emplace_back( (ptr) );
    }

    for(std::size_t b=bitsets.size(), a=0; a<b; ++a)
        if(found_pos == a+1)
            bitsets[a].set(index);
        else
            bitsets[a].reset(index);
}

template<typename T, unsigned Dimension>
pointer_array<T,Dimension>::pointer_array()
{
    distinct_options.reserve(num_distinct);
    bitsets.reserve(num_distinct-1);
}

template<typename T, bool is_ptr=std::is_pointer<T>::value>
struct DeletePointerArray
{
    template<typename T2>
    static void DealWith(T2& )
    {
    }
};
template<typename T>
struct DeletePointerArray<T,true>
{
    template<typename T2>
    static void DealWith(T2& array)
    {
        std::sort( std::begin(array), std::end(array) );
        T prev = nullptr;
        for(auto p: array)
            if(p != prev)
            {
                delete p;
                prev = p;
            }
    }
};

template<typename T, unsigned Dimension>
pointer_array<T,Dimension>::~pointer_array()
{
    DeletePointerArray<T>::DealWith(distinct_options);
}
