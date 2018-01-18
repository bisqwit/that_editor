/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#ifndef bqtWordPtrVecHH
#define bqtWordPtrVecHH

/* Vector of vectors of unsigned short */

#include "vec_s.hh"

#define UsePlacementNew
#define o(x) x(WordVecType,WordPtrVecType)
#include "vecbase.hh"
#undef o
#undef UsePlacementNew

#endif
