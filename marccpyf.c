/************************************************************************
* marc_copy_field ()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Copy an entire field from one record to another.                *
*       This version only copies to a different record, not the         *
*       same one.                                                       *
*                                                                       *
*       This is a high level routine that mostly calls regular API      *
*       functions to do all the work.                                   *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init, positioned          *
*           at input field.                                             *
*       Pointer to structure for output record.  Must be a different    *
*           record.                                                     *
*       Field id for new copy of field.  Need not be the same.          *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marccpyf.c,v 1.2 2000/10/02 19:40:08 meyer Exp $
 * $Log: marccpyf.c,v $
 * Revision 1.2  2000/10/02 19:40:08  meyer
 * Fixed bug.
 *
 * Revision 1.1  2000/10/02 18:44:03  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_copy_field (
    MARCP   inmp,           /* Pointer to input recstruc*/
    MARCP   outmp,          /* Pointer to output        */
    int     field_id        /* Field tag/identifier     */
) {
    int     infid,          /* Input field id           */
            sid,            /* Subfield id              */
            sfcount,        /* Num sfs in field         */
            spos,           /* Subfield ordinal position*/
            stat;           /* Return code              */
    unsigned char *datap;   /* Ptr to data in field     */
    size_t  datalen;        /* Ptr to sf length         */


    /* Test that records are different */
    if (inmp->marcbufp == outmp->marcbufp)
        return MARC_ERR_SAME_REC;

    /* Fixed field can only be copied to same fixed field */
    infid = (inmp->fdirp + inmp->cur_field)->tag;
    if (infid < MARC_FIRST_VARFIELD && infid != field_id)
        return MARC_ERR_DIFF_FLD;

    /* Remember where we are in input record */
    if ((stat = marc_save_pos (inmp)) != 0)
        return stat;

    /* Add the output field */
    if ((stat = marc_add_field (outmp, field_id)) != 0)
        return stat;

    /* If it's a fixed field, copy it */
    if (infid < MARC_FIRST_VARFIELD) {

        /* Request all bytes, it will tell us how many there were */
        if ((stat = marc_get_subfield (inmp, 0, 9999, &datap, &datalen))
                 != MARC_RET_FX_LENGTH)
            return stat;

        if ((stat = marc_add_subfield (outmp, 0, datap, datalen)) != 0)
            return stat;
    }

    else {

        /* Process all fields */
        if ((stat = marc_cur_subfield_count (inmp, &sfcount)) != 0)
            return stat;

        for (spos=0; spos<sfcount; spos++) {
            if ((stat = marc_pos_subfield (inmp, spos, &sid,
                                           &datap, &datalen)) != 0)
                return stat;
            if ((stat = marc_add_subfield (outmp, sid, datap, datalen)) != 0)
                return stat;
        }
    }

    /* Back to start */
    stat = marc_restore_pos (inmp);

    return stat;

} /* marc_copy_field */




