#ifndef bqtPicTypesHH
#define bqtPicTypesHH

// Bitfield utilities
template<unsigned bitno, unsigned nbits, typename T,
         unsigned dim=1, unsigned index=0>
struct RegBitSet
{
    T data[dim];

    enum { mask = ((1ull << (nbits/2)) << (nbits-(nbits/2))) - 1ull };

    operator T() const { return (data[index] >> bitno) & mask; }

    typedef RegBitSet<bitno,nbits,T,1,0> SingleElem;

    SingleElem& operator[] (unsigned i)
    {
        T* d = reinterpret_cast<T*>(this) + index+i;
        return *reinterpret_cast<SingleElem*>(d);
    }

    T operator[] (unsigned i) const
    {
        return (data[index+i] >> bitno) & mask;
    }

    template<typename T2>
    RegBitSet& operator=(T2 val)
    {
        data[index] = (data[index] & ~(mask << bitno))
                    | ((nbits > 1 ? val & mask : !!val) << bitno);
        return *this;
    }

    T& ref() { return data[index]; }

    template<typename T2> RegBitSet& operator^= (T2 val) { return *this = (T)*this ^ (T)val; }
    template<typename T2> RegBitSet& operator|= (T2 val) { return *this = (T)*this | (T)val; }
    template<typename T2> RegBitSet& operator&= (T2 val) { return *this = (T)*this & (T)val; }
    template<typename T2> RegBitSet& operator+= (T2 val) { return *this = (T)*this + (T)val; }
    template<typename T2> RegBitSet& operator-= (T2 val) { return *this = (T)*this - (T)val; }
/*
    T* operator &()      { return &data[index]; }

    template<typename T2> decltype(T()+(T2()+0)) operator+ (T2 val) const { return (T)*this + val; }
    template<typename T2> decltype(T()-(T2()+0)) operator- (T2 val) const { return (T)*this - val; }
    template<typename T2> decltype(T()|(T2()+0)) operator| (T2 val) const { return (T)*this | val; }
    template<typename T2> decltype(T()&(T2()+0)) operator& (T2 val) const { return (T)*this & val; }
    template<typename T2> decltype(T()^(T2()+0)) operator^ (T2 val) const { return (T)*this ^ val; }
    template<typename T2> decltype(T()*(T2()+0)) operator* (T2 val) const { return (T)*this * val; }
    template<typename T2> decltype(T()/(T2()+0)) operator/ (T2 val) const { return (T)*this / val; }
    template<typename T2> decltype(T()%(T2()+0)) operator% (T2 val) const { return (T)*this % val; }
    template<typename T2> decltype(T()> (T2()+0)) operator> (T2 val) const { return (T)*this > val; }
    template<typename T2> decltype(T()< (T2()+0)) operator< (T2 val) const { return (T)*this < val; }
    template<typename T2> decltype(T()==(T2()+0)) operator== (T2 val) const { return (T)*this == val; }
    template<typename T2> decltype(T()!=(T2()+0)) operator!= (T2 val) const { return (T)*this != val; }
    template<typename T2> decltype(T()<=(T2()+0)) operator<= (T2 val) const { return (T)*this <= val; }
    template<typename T2> decltype(T()>=(T2()+0)) operator>= (T2 val) const { return (T)*this >= val; }
*/
    RegBitSet& operator++ ()  { return *this = (T)*this + T(1); }
    RegBitSet& operator-- ()  { return *this = (T)*this - T(1); }
    T operator++ (int) { T r(*this); ++*this; return r; }
    T operator-- (int) { T r(*this); --*this; return r; }
    bool operator! () const { return !operator T(); }
};

#endif
