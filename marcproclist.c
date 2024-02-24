/************************************************************************
* marcproclist.c                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Table of custom procedures used in Marc->Marc processing.       *
*                                                                       *
*       This file contains the tables for procedure name/pointer        *
*       conversions.  Actual procedures reside elsewhere.               *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcproclist.c,v 1.3 1999/05/27 13:21:28 vereb Exp vereb $
 * $Log: marcproclist.c,v $
 * Revision 1.3  1999/05/27 13:21:28  vereb
 * modifications to add remarc_035
 *
 * Revision 1.2  1999/03/29 20:27:19  meyer
 * Mark added some prototypes.
 * Modified cmp_log to require at least 2 args.
 *
 * Revision 1.1  1998/12/29 02:41:53  meyer
 * Initial revision
 *
 */

#include "marcproclist.h"

/*********************************************************************
* G_proc_list[]                                                      *
*                                                                    *
*   Global Function external name->internal properties lookup table. *
*                                                                    *
*   Add an entry to this table for each new procedure to be used     *
*   in marc conversions.                                             *
*                                                                    *
*   Place prototypes for general purpose procedures in marcconv.h,   *
*   special purpose procedures in marcproclist.h                     *
*********************************************************************/

CMP_TABLE G_proc_list[] = {
    /* Generic functions */
    {"if",        cmp_if,        CM_CND_IF,    2, 3, CMP_ANY},
    {"else",      cmp_nop,       CM_CND_ELSE,  0, 0, CMP_ANY},
    {"endif",     cmp_nop,       CM_CND_ENDIF, 0, 0, CMP_ANY},
    {"indic",     cmp_indic,     CM_CND_NONE,  2, 2, CMP_FO | CMP_SO},
    {"clear",     cmp_clear,     CM_CND_NONE,  1, 1, CMP_ANY},
    {"append",    cmp_append,    CM_CND_NONE,  2, 6, CMP_ANY},
    {"copy",      cmp_copy,      CM_CND_NONE,  2, 6, CMP_ANY},
    {"normalize", cmp_normalize, CM_CND_NONE,  2, 2, CMP_ANY},
    {"makefld",   cmp_makefld,   CM_CND_NONE,  1, 1, CMP_RE|CMP_RO|CMP_FO|CMP_SO},
    {"makesf",    cmp_makesf,    CM_CND_NONE,  1, 1, CMP_RE|CMP_RO|CMP_FO|CMP_SO},
    {"y2toy4",    cmp_y2toy4,    CM_CND_NONE,  2, 2, CMP_ANY},
    {"killrec",   cmp_killrec,   CM_CND_NONE,  0, 0, CMP_ANY},
    {"killfld",   cmp_killfld,   CM_CND_NONE,  0, 0, CMP_FE|CMP_FO|CMP_SE|CMP_SO},
    {"donerec",   cmp_donerec,   CM_CND_NONE,  0, 0, CMP_ANY},
    {"donefld",   cmp_donefld,   CM_CND_NONE,  0, 0, CMP_FE|CMP_FO|CMP_SE|CMP_SO},
    {"donesf",    cmp_donesf,    CM_CND_NONE,  0, 0, CMP_SE|CMP_SO},
    {"renfld",    cmp_renfld,    CM_CND_NONE,  1, 1, CMP_FE|CMP_FO|CMP_SE|CMP_SO},
    {"rensf",     cmp_rensf,     CM_CND_NONE,  1, 1, CMP_SO},
    {"today",     cmp_today,     CM_CND_NONE,  2, 2, CMP_ANY},
    {"substr",    cmp_substr,    CM_CND_NONE,  3, 4, CMP_ANY},
    {"log",       cmp_log,       CM_CND_NONE,  2,99, CMP_ANY},

    /* Specialized procedures.  Add more here           */
    /* 000 processing must be forced...                 */
    /* 'generic' field processing begins with field 001 */

    /* bibcntl_proc  C_code_proc            if/else/endif/none 
                                                          Min, Max
                                                                Record/fld/sf/pre/post info  */
    {"proc_000",     cmp_proc_000,          CM_CND_NONE,  0, 0, CMP_RE|CMP_RO},
    {"proc_010",     cmp_proc_010,          CM_CND_NONE,  1, 1, CMP_FE},
    {"naco_010",     cmp_naco_010,          CM_CND_NONE,  0, 0, CMP_SE},
    {"proc_022",     cmp_proc_022,          CM_CND_NONE,  0, 0, CMP_FE},
    {"proc_035",     cmp_proc_035,          CM_CND_NONE,  0, 0, CMP_RE},
    //    {"proc_035_plus",cmp_proc_035_plus,     CM_CND_NONE,  0, 0, CMP_RE},
    {"remarc_035",   cmp_proc_035_remarc,   CM_CND_NONE,  0, 0, CMP_RE},
    {"generic_035_processing",cmp_generic_035_processing,CM_CND_NONE,1,1,CMP_RE},
    {"proc_getUI035",cmp_getUIfrom035,      CM_CND_NONE,  0, 0, CMP_RE},
    {"proc_041",     cmp_proc_041,          CM_CND_NONE,  0, 0, CMP_FE},
    {"proc_066",     cmp_proc_066,          CM_CND_NONE,  0, 0, CMP_RE},
    {"punc_245",     cmp_punc_245,          CM_CND_NONE,  0, 0, CMP_RE},
    {"proc_659",     cmp_proc_659,          CM_CND_NONE,  0, 0, CMP_FE},
    {"proc_76X",     cmp_proc_76X,          CM_CND_NONE,  1, 1, CMP_FE},
    {"proc_856",     cmp_proc_856,          CM_CND_NONE,  0, 0, CMP_FE},
    {"proc_880",     cmp_proc_880,          CM_CND_NONE,  0, 0, CMP_RE},
    {"proc_998",     cmp_proc_998,          CM_CND_NONE,  1, 1, CMP_RE},
    {"isbn_13",      cmp_isbn_13,           CM_CND_NONE,  0, 1, CMP_RE},
    {"checkdups",    cmp_checkdups,         CM_CND_NONE,  1, 1, CMP_RE},
    {"mesh",         cmp_Mesh,              CM_CND_NONE,  0, 0, CMP_RE|CMP_FE},
    {"checklen",     cmp_checklen,          CM_CND_NONE,  1, 1, CMP_FE|CMP_FO},
    {"naco_cleanup", cmp_naco_cleanup,      CM_CND_NONE,  0, 0, CMP_FE},
    {0}
};


