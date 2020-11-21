#include "pspimport.s"


    IMPORT_FUNC "sceDmac", 0x617F3FE6, sceDmacMemcpy

#ifdef F_sceDmac_0002
    IMPORT_FUNC "sceDmac", 0xD97F94D8, sceDmacTryMemcpy
#endif