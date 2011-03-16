#ifndef bqtWordPtrVecHH
#define bqtWordPtrVecHH

#include "vec_s.hh"

#define UsePlacementNew

#define T       WordVecType
#define VecType WordPtrVecType

#include "vecbase.hh"

#undef VecType
#undef T

#undef UsePlacementNew

#endif
