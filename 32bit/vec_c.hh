/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#ifndef bqtCharVecHH
#define bqtCharVecHH

/* Vector of unsigned char */

#ifdef __GNUC__
 #include <vector>
 using CharVecType = std::vector<unsigned char>;
#else

#define T       unsigned char
#define VecType CharVecType
#include "vecbase.hh"
#undef VecType
#undef T

#endif

#endif
