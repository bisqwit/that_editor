#ifndef bqtPtrArrayHH
#define bqtPtrArrayHH

#include <bitset>
#include <vector>
#include <array>

template<typename T, unsigned Dimension>
class pointer_array
{
    static_assert(Dimension != 1, "pointer_array size must be greater than 1.");

    static constexpr unsigned num_distinct = NUM_DISTINCT;
    static constexpr unsigned max_distinct = MAX_DISTINCT;
    std::vector< T >                     distinct_options;
    std::vector< std::bitset<Dimension> > bitsets;
public:
    T    Get(std::size_t index) const;
    void Set(std::size_t index, T ptr);

    pointer_array();
    ~pointer_array();

    pointer_array(pointer_array&& ) = default;
    pointer_array& operator=(pointer_array&& ) = default;

    pointer_array& operator=(const pointer_array&) = delete;
    pointer_array(const pointer_array&) = delete;
};

#include "ptr_array.tcc"

#endif
