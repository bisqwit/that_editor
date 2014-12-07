#ifndef bqtPtrArrayHH
#define bqtPtrArrayHH

#include <bitset>
#include <vector>

template<typename T, unsigned Dimension>
class pointer_array
{
    static_assert(Dimension != 1, "pointer_array size must be greater than 1.");

    static constexpr unsigned num_distinct = 1;//NUM_DISTINCT;
    static constexpr unsigned max_distinct = 32;//MAX_DISTINCT;
    std::vector< T >                      distinct_options;
    std::vector< std::bitset<Dimension> > bitsets;

    /* bitsets[n][m] tells whether array[m] == distinct_options[n+1].
     * If none is set, distinct_options[0] is assumed.
     */

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
