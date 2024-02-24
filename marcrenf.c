/************************************************************************
* marc_rename_field ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       Rename the current field by changing its field tag.             *
*                                                                       *
*       This admittedly exotic function is used in a marc->marc         *
*       conversion program and could be used in editors or other        *
*       software.                                                       *
*                                                                       *
*   PASS                                                                *
*       Pointer to marcctl structure.                                   *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else no position to save, or error.                             *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcrenf.c,v 1.1 1998/12/28 21:10:37 meyer Exp meyer $
 * $Log: marcrenf.c,v $
 * Revision 1.1  1998/12/28 21:10:37  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_rename_field (
    MARCP mp,       /* Ptr to marcctl       */
    int   new_id    /* New field id/tag     */
) {
    int   retcode;  /* Return code          */


    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* If there is no current position, we can't rename it */
    if (mp->cur_field == MARC_NO_FIELD)
        retcode = MARC_ERR_REN_FLDPOS;

    else if (new_id < 1 || new_id > MARC_MAX_FIELDID)
        retcode = MARC_ERR_REN_FLDTAG;

    else {
        mp->fdirp[mp->cur_field].tag = new_id;
        retcode = 0;
    }

    return retcode;

} /* marc_rename_field */


