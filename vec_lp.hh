/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#ifndef bqtLongPtrVecHH
#define bqtLongPtrVecHH

/* Vector of vectors of unsigned long */

#include "vec_l.hh"

#define UsePlacementNew

#define T       LongVecType
#define VecType LongPtrVecType

#include "vecbase.hh"

#undef VecType
#undef T

#undef UsePlacementNew

#endif
