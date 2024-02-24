/************************************************************************
* marc_pos_field ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Position to a field by it's ordinal number, i.e., its           *
*       order in the directory.                                         *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Position to retrieve, 0 to field_count-1.                       *
*           Negative numbers count from end.                            *
*           -1 = last field.                                            *
*           -N = Nth from last field.                                   *
*       Pointer to place to put field tag/id number.                    *
*           0 = Leader.                                                 *
*       Ptr to place to put pointer to field data.                      *
*       Ptr to place to put length of field.                            *
*           Length does NOT include field terminator.                   *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*           Negative number indicates serious, structural error.        *
*           Positive number indicates data not found.                   *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcposf.c,v 1.1 1998/12/28 21:09:48 meyer Exp meyer $
 * $Log: marcposf.c,v $
 * Revision 1.1  1998/12/28 21:09:48  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_pos_field (
    MARCP         mp,           /* Pointer to structure     */
    int           pos,          /* Position to move to      */
    int           *field_idp,   /* Put Field tag/id here    */
    unsigned char **datap,      /* Put data pointer here    */
    size_t        *datalenp     /* Put data only length here*/
) {
    FLDDIR        *dp;          /* Ptr to directory entry   */


    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Normalize requests for negative positions */
    if (pos < 0)
        pos = mp->field_count - pos;

    /* Can't go beyond ends of record */
    if (pos < 0 || pos > mp->field_count - 1)
        return MARC_RET_END_REC;

    /* Move to next or pevious field */
    mp->cur_field = pos;

    /* Position to field directory */
    dp = mp->fdirp + pos;

    /* Return requested information */
    *field_idp = dp->tag;
    *datap     = mp->rawbufp + dp->offset;
    *datalenp  = dp->len;

    return 0;

} /* marc_pos_field */
