/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#ifndef bqtCharPtrVecHH
#define bqtCharPtrVecHH

/* Vector of vectors of unsigned char */

#include "vec_c.hh"

#define UsePlacementNew

#define T       CharVecType
#define VecType CharPtrVecType

#include "vecbase.hh"

#undef VecType
#undef T
#undef  UsePlacementNew

#endif
