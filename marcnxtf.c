/************************************************************************
* marc_next_field ()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Position to the next field after the current one.               *
*                                                                       *
*       To process a record sequentially, call marc_get_field()         *
*       for the leader (field 0), and then call marc_next_field()       *
*       repeatedly until it returns MARC_RET_END_REC.                   *
*                                                                       *
*       This is just a repackage of marc_pos_field()/                   *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Direction to move.                                              *
*           True (non-zero) = Forward, next field.                      *
*           False (zero)    = Backward, previous field.                 *
*       Pointer to place to put field tag/id number.                    *
*           0 = Leader.                                                 *
*       Ptr to place to put pointer to field data.                      *
*       Ptr to place to put length of field.                            *
*           Length does NOT include indicators or field terminator.     *
*       Ptr to place to put field position.                             *
*           In case the caller wants to know that this is the Nth field.*
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*           Negative number indicates serious, structural error.        *
*           Positive number indicates data not found.                   *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcnxtf.c,v 1.1 1998/12/28 21:05:18 meyer Exp meyer $
 * $Log: marcnxtf.c,v $
 * Revision 1.1  1998/12/28 21:05:18  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_next_field (
    MARCP         mp,           /* Pointer to structure     */
    int           forward,      /* True=next field else prev*/
    int           *field_idp,   /* Put Field tag/id here    */
    unsigned char **datap,      /* Put data pointer here    */
    size_t        *datalenp,    /* Put data only length here*/
    int           *posp         /* Put position here        */
) {
    int           get_pos,      /* Position to retreive     */
                  retcode;


    /* Set correct field id */
    get_pos = mp->cur_field + ( (forward) ? 1 : -1);

    if ((retcode = marc_pos_field (mp, get_pos, field_idp, datap,
                                   datalenp)) == 0)

        /* We've got everything for caller but the field position */
        *posp = mp->cur_field;

    return (retcode);

} /* marc_next_field */
