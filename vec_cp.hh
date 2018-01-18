/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#ifndef bqtCharPtrVecHH
#define bqtCharPtrVecHH

/* Vector of vectors of unsigned char */

#include "vec_c.hh"

#define UsePlacementNew
#define o(x) x(CharVecType,CharPtrVecType)
#include "vecbase.hh"
#undef o
#undef  UsePlacementNew

#endif
