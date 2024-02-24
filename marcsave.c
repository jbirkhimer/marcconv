/************************************************************************
* marc_save_pos ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Remember a field position by setting a bit on in the            *
*       pos_bits bit flags field in the field directory entry           *
*       for that field.                                                 *
*                                                                       *
*       Use marc_restore_pos() to make the field current again.         *
*                                                                       *
*       marc_save_pos() may be called repeatedly, effectively           *
*       establishing a stack of saved positions, up to the              *
*       maximum number of bit flags we support (31).                    *
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

/* $Id: marcsave.c,v 1.2 1999/06/14 21:43:47 meyer Exp $
 * $Log: marcsave.c,v $
 * Revision 1.2  1999/06/14 21:43:47  meyer
 * Fixed bug in marc_restore_pos(), was not really restoring position,
 *   it only removed the position marking flag.
 *
 * Revision 1.1  1998/12/28 21:12:53  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_save_pos (
    MARCP mp        /* Ptr to marcctl       */
) {
    int   retcode;  /* Return code          */


    /* If there is no current position, we can't save it
     * We'll treat this as a non-serious error.
     */
    if (mp->cur_field == MARC_NO_FIELD)
        retcode = MARC_RET_NO_POS;

    /* Only allow as many saves as there are bits in the field struct */
    else if (mp->last_pos >= MARC_MAX_SAVE_POS)
        retcode = MARC_ERR_POS_STACK;

    else {
        /* Set the bit on for the current count of saves */
        mp->fdirp[mp->cur_field].pos_bits |= (1 << mp->last_pos);
        ++mp->last_pos;
        retcode = 0;
    }

    return retcode;

} /* marc_save_pos */


/************************************************************************
* marc_restore_pos ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Return to the last field position saved via a call to           *
*       marc_save_pos().                                                *
*                                                                       *
*       The marked field might have been deleted since the last         *
*       marc_save_pos(), leaving nothing to restore.  In that case      *
*       we set the current position to MARC_NO_FIELD and return         *
*       a non-zero return code (MARC_RET_NO_SAVE_POS).                  *
*                                                                       *
*   PASS                                                                *
*       Pointer to marcctl structure.                                   *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else no position to restore, or error.                          *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

#include "marcdefs.h"

int marc_restore_pos (
    MARCP    mp         /* Ptr to marcctl       */
) {
    FLDDIR   *fdp;      /* Ptr into directory   */
    unsigned mask;      /* Bit mask for flags   */
    int      i,         /* Loop counter         */
             retcode;   /* Return code          */



    /* If marc_save_pos() not called, this is an unbalanced call
     * Probably a serious logic error in the calling program
     */
    if (mp->last_pos == 0)
        retcode = MARC_ERR_SAVE_POS;

    else {
        /* Decrement stack and set bit mask for current position */
        mask = (1 << --mp->last_pos);

        /* Default return code if position not found */
        retcode = MARC_RET_NO_SAVE_POS;

        /* Search for our position */
        fdp = mp->fdirp;
        for (i=0; i<mp->field_count; i++) {
            if (fdp->pos_bits & mask) {

                /* Turn bit off in field dir entry */
                mask = ~mask;
                fdp->pos_bits &= mask;

                /* Set current position in marc control */
                mp->cur_field = i;

                /* Success */
                retcode = 0;
                break;
            }
            ++fdp;
        }
    }

    return retcode;

} /* marc_restore_pos */
