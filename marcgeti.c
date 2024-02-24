/************************************************************************
* marc_get_item ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Package marc_get_field() and marc_get_subfield() into a single  *
*       function.                                                       *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Field number to find.                                           *
*           0 = Leader.                                                 *
*       Occurrence number, origin 0, for this field number.             *
*       Subfield code to find.                                          *
*       Occurrence number, origin 0, for this subfield code.            *
*       Ptr to place to put pointer to subfield data.                   *
*           Will point to start of data, after subfield delimiter       *
*           and code.                                                   *
*       Ptr to place to put length of subfield.                         *
*           Length does not include subfield delimiter or code.         *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*           Negative number indicates serious, structural error.        *
*           Positive number indicates data not found.                   *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcgeti.c,v 1.1 1998/12/28 21:01:32 meyer Exp meyer $
 * $Log: marcgeti.c,v $
 * Revision 1.1  1998/12/28 21:01:32  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_get_item (
    MARCP         mp,           /* Pointer to structure     */
    int           field_id,     /* Field tag/identifier     */
    int           field_occ,    /* Occurrence number        */
    int           sf_id,        /* Subfield code/identifier */
    int           sf_occ,       /* Occurrence number        */
    unsigned char **datap,      /* Put data pointer here    */
    size_t        *datalen      /* Put data only length here*/
) {
    int           retcode;      /* Return code              */


    /* If we can position to the field */
    if ((retcode = marc_get_field (mp, field_id, field_occ,
                                   datap, datalen)) == 0)

        /* Try the subfield */
        retcode = marc_get_subfield (mp, sf_id, sf_occ, datap, datalen);

    return (retcode);

} /* marc_get_item */
