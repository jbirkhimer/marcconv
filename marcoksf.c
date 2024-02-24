/************************************************************************
* marc_ok_subfield ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Is the passed subfield code legal in marc?                      *
*                                                                       *
*   PASS                                                                *
*       Subfield code.                                                  *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcoksf.c,v 1.2 1998/12/31 21:15:15 meyer Exp $
 * $Log: marcoksf.c,v $
 * Revision 1.2  1998/12/31 21:15:15  meyer
 * Made all printables after space and before rubout legal.
 * NLM is using some odd ones.
 *
 * Revision 1.1  1998/12/28 21:06:02  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_ok_subfield (
    int sf_id       /* Subfield code to check */
) {

#if WHAT_I_USED_TO_THINK_WAS_VALID
    /* Require subfield code a..z or 1..9 */
    if (!(sf_id >= 'a' && sf_id <= 'z')) {
        if (!(sf_id >= '0' && sf_id <= '9'))
            return MARC_ERR_SFCODE;
    }
#else
    /* Allow any printable character above space, below rubout */
    if (sf_id < 33 || sf_id > 126)
        return MARC_ERR_SFCODE;
#endif

    return 0;

} /* marc_ok_subfield */
