/************************************************************************
* marc_xcheck ()                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Check a marcctl structure for validity.                         *
*                                                                       *
*       We can add anything we want here.                               *
*                                                                       *
*   PASS                                                                *
*       Pointer to marcctl structure.                                   *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcxchk.c,v 1.1 1998/12/28 21:15:03 meyer Exp meyer $
 * $Log: marcxchk.c,v $
 * Revision 1.1  1998/12/28 21:15:03  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_xcheck (
    MARCP mp        /* marcctl to check */
) {

    /* Minimal test to be sure this is a marcctl structure */
    if (mp->start_tag != MARC_START_TAG)
        return MARC_ERR_START_TAG;
    if (mp->end_tag != MARC_END_TAG)
        return MARC_ERR_END_TAG;

    return 0;

} /* marc_xcheck */
