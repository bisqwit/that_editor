/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#ifndef bqtWordPtrVecHH
#define bqtWordPtrVecHH

/* Vector of vectors of unsigned short */

#include "vec_s.hh"

#ifdef __GNUC__
 #include <vector>
 using WordPtrVecType = std::vector<std::vector<unsigned short>>;
#else

#define UsePlacementNew

#define T       WordVecType
#define VecType WordPtrVecType

#include "vecbase.hh"

#undef VecType
#undef T

#undef UsePlacementNew

#endif

#endif
