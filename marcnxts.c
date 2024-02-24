/************************************************************************
* marc_next_subfield ()                                                 *
*                                                                       *
*   DEFINITION                                                          *
*       Position to the next subfield after the current one.            *
*                                                                       *
*       If no subfields have been accessed in the current field,        *
*       positions to the first subfield.                                *
*                                                                       *
*       In non-control fields, in order to support a simple and         *
*       uniform interface, the first subfield returned when             *
*       starting a field is the first indicator, represented            *
*       as MARC_INDIC1.  Then comes the second indicator, then          *
*       comes the first variable length subfield.                       *
*                                                                       *
*       If the last subfield has already been accessed in the           *
*       current field, returns MARC_RET_END_FIELD.                      *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Direction to move.                                              *
*           True (non-zero) = Forward, next subfield.                   *
*           False (zero)    = Backward, previous subfield.              *
*       Pointer to place to put subfield code/id.                       *
*       Ptr to place to put pointer to subfield data.                   *
*           Returned pointer points past subfield delimiter and         *
*           code to first byte of actual data.                          *
*       Ptr to place to put length of subfield.                         *
*           Length does NOT include subfield code or delimiter.         *
*       Ptr to place to put subfield position.                          *
*           In case the caller wants to know that this is the Nth sf.   *
*           Note: position 0 may be first indicator, position 2 is      *
*           first variable length subfield.                             *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*           Negative number indicates serious, structural error.        *
*           Positive number indicates data not found.                   *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcnxts.c,v 1.2 2002/08/19 15:33:10 meyer Exp $
 * $Log: marcnxts.c,v $
 * Revision 1.2  2002/08/19 15:33:10  meyer
 * Fixed severe bug, pointer to subfield directory always pointed to
 * first subfield, not current subfield, within field.
 *
 * Revision 1.1  1998/12/28 21:05:42  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_next_subfield (
    MARCP         mp,           /* Pointer to structure     */
    int           forward,      /* True=next sf, else prev  */
    int           *sf_id,       /* Put subfield code/id here*/
    unsigned char **datap,      /* Put data pointer here    */
    size_t        *datalenp,    /* Put data only length here*/
    int           *posp         /* Put position here        */
) {
    SFDIR         *sp;          /* Ptr to directory entry   */
    int           stat;         /* Return from marc_xsfdir()*/


    /* Cast to mp and test, return if failed */
    MARC_XCHECK(mp)

    /* Function only works for variable length fields */
    if (MARC_CUR_TAG(mp) < MARC_FIRST_VARFIELD)
        return MARC_ERR_VARONLY;

    /* Create subfield directory if necessary.
     * If we are already positioned at this field, and a subfield
     *   directory already exists for it, nothing happens.
     *   This allows us to get the next subfield by reference
     *   to mp->cur_sf.
     * If no no subfield directory built for it, we build a
     *   subfield directory and set the current position
     *   (mp->cur_sf) to -1, just before the first subfield.
     */
    if ((stat = marc_xsfdir (mp)) != 0)
        return (stat);

    /* Can't go beyond beginning or end of field */
    if (forward) {
        if (mp->cur_sf >= mp->sf_count - 1)
            return MARC_RET_END_FIELD;

        /* Advance to next subfield */
        ++mp->cur_sf;
    }
    else {
        if (mp->cur_sf < 1)
            return MARC_RET_END_FIELD;
        --mp->cur_sf;
    }

    /* Simplify reference to subfield directory */
    sp = mp->sdirp + mp->cur_sf;

    /* Return requested information */
    *sf_id    = sp->id;
    *datap    = sp->startp;
    *datalenp = sp->len;
    *posp     = mp->cur_sf;

    /* For variable len subfields, point past delimiter and code */
    if (mp->cur_sf > 1) {
        *datap    += 2;
        *datalenp -= 2;
    }

    return 0;

} /* marc_next_subfield */
