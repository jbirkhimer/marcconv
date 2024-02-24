/************************************************************************
* marc_pos_subfield ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       Position to a particular ordinal subfield within the            *
*       current field.                                                  *
*                                                                       *
*       For non-control fields, the first "subfield" is defined         *
*       in this API as MARC_INDIC1.  The first variable length          *
*       subfield is the third one, ordinal position 2 (origin 0).       *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Position to retrieve, 0 to sf_count - 1.                        *
*           Negative numbers count from end.                            *
*           -1 = last subfield.                                         *
*           -N = Nth from last subfield.                                *
*       Pointer to place to put subfield code/id.                       *
*       Ptr to place to put pointer to subfield data.                   *
*           Returned pointer points past subfield delimiter and         *
*           code to first byte of actual data.                          *
*       Ptr to place to put length of subfield.                         *
*           Length does NOT include the subfield delimiter or code.     *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*           Negative number indicates serious, structural error.        *
*           Positive number indicates data not found.                   *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcposs.c,v 1.2 2000/03/13 23:04:54 meyer Exp $
 * $Log: marcposs.c,v $
 * Revision 1.2  2000/03/13 23:04:54  meyer
 * Fixed comment.
 *
 * Revision 1.1  1998/12/28 21:10:08  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_pos_subfield (
    MARCP         mp,           /* Pointer to structure     */
    int           pos,          /* Subfield position        */
    int           *sf_idp,      /* Put subfield code/id here*/
    unsigned char **datap,      /* Put data pointer here    */
    size_t        *datalenp     /* Put data only length here*/
) {
    SFDIR         *sp;          /* Ptr to directory entry   */
    int           stat;         /* Return from marc_xsfdir()*/


    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Can't find subfield position if not positioned on a field */
    if (mp->cur_field == MARC_NO_FIELD)
        return MARC_ERR_SF_FLDPOS;

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

    /* Normalize requests for negative positions */
    if (pos < 0)
        pos = mp->sf_count - pos;

    /* Can't go beyond beginning or end of field */
    if (pos < 0 || pos > mp->sf_count - 1)
        return MARC_RET_END_FIELD;

    /* Set index to requested subfield */
    mp->cur_sf = pos;

    /* Simplify reference to subfield directory */
    sp = mp->sdirp + pos;

    /* Return requested information.
     * +/- 2 used to skip subfield delimiter and code.
     */
    *sf_idp   = sp->id;
    *datap    = sp->startp;
    *datalenp = sp->len;
    if (pos > 1) {
        *datap    += 2;
        *datalenp -= 2;
    }

    return 0;

} /* marc_pos_subfield */
