/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#ifndef bqtWordVecHH
#define bqtWordVecHH

/* Vector of unsigned short */

#ifdef __GNUC__
 #include <vector>
 using WordVecType = std::vector<unsigned short>;
#else

#define T       unsigned short
#define VecType WordVecType
#include "vecbase.hh"
#undef VecType
#undef T

#endif

#endif
