/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#ifndef bqtLongPtrVecHH
#define bqtLongPtrVecHH

/* Vector of vectors of unsigned long */

#include "vec_l.hh"

#define UsePlacementNew
#define o(x) x(LongVecType,LongPtrVecType)
#include "vecbase.hh"
#undef o
#undef UsePlacementNew

#endif
