#ifdef __GNUC__
# define cdecl
# define register
#else
typedef signed char Schar;
# define bool  Schar
# define false 0
# define true  1
# define nullptr ((void*)0)
# define _far
#endif
