#ifndef bqtPtrArrayHH
#define bqtPtrArrayHH

#include <bitset>

template<typename T, unsigned Dimension>
class pointer_array
{
    static constexpr unsigned num_distinct = 4;
    T** distinct_options;
    std::bitset<Dimension * (num_distinct-1)>* bitset;
public:
    T*    Get(std::size_t index) const;
    void  Set(std::size_t index, T* ptr);

    pointer_array();
    ~pointer_array();

    pointer_array(pointer_array&& b);
    pointer_array& operator=(pointer_array&& b);

    pointer_array& operator=(const pointer_array&) = delete;
    pointer_array(const pointer_array&) = delete;
private:
    void Clean();
};

#include "ptr_array.tcc"

#endif
