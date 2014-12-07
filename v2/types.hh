#ifndef bqtPicTypesHH
#define bqtPicTypesHH

// Bitfield utilities
template<unsigned bitno, unsigned nbits, typename T>
struct RegBitSet
{
    T data;

    /* Bitmask: A binary number that has nbits 1-bits. pow(2, nbits) - 1 */
    enum { mask = ((1ull << (nbits/2)) << (nbits-(nbits/2))) - 1ull };

    operator T() const { return (data >> bitno) & mask; }

    template<typename T2>
    RegBitSet& operator=(T2 val)
    {
        /* Poke the given value into the data.
         * If nbits=1, cast val as bool before doing the work.
         */
        data = T(data & T(~(mask << bitno)))
             | T((nbits > 1 ? T(val & mask) : T(bool(val))) << bitno);
        return *this;
    }
};

#endif
