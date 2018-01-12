/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#ifndef bqtLongVecHH
#define bqtLongVecHH

/* Vector of unsigned long */

#ifdef __GNUC__
 #include <vector>
 using LongVecType = std::vector<unsigned long>;
#else


#define T       unsigned long
#define VecType LongVecType
#include "vecbase.hh"
#undef VecType
#undef T


#endif

#endif
