/************************************************************************
* marc_get_field ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Retrieve information about a field using it's field tag         *
*       and occurrence number.                                          *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Field number to find.                                           *
*           0 = Leader.                                                 *
*       Occurrence number, origin 0, for this field number.             *
*           Negative occurrence numbers count backwards from end, e.g.  *
*               -1 = last occ.                                          *
*               -2 = next to last occ.                                  *
*               etc.                                                    *
*       Ptr to place to put pointer to field data.                      *
*           For non-control fields, returned pointer points to first    *
*           indicator for field.                                        *
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

/* $Id: marcgetf.c,v 1.2 1999/06/14 21:43:03 meyer Exp $
 * $Log: marcgetf.c,v $
 * Revision 1.2  1999/06/14 21:43:03  meyer
 * Added support for negative occurrence numbers.
 *
 * Revision 1.1  1998/12/28 21:01:14  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_get_field (
    MARCP         mp,           /* Pointer to structure     */
    int           field_id,     /* Field tag/identifier     */
    int           field_occ,    /* Occurrence number        */
    unsigned char **datap,      /* Put data pointer here    */
    size_t        *datalen      /* Put data only length here*/
) {
    FLDDIR        *dp;          /* Ptr to directory entry   */
    int           count,        /* Field directory counter  */
                  occ,          /* Occurrence counter       */
                  retcode;      /* Return code              */


    /* Cast to mp and test, return if failed */
    MARC_XCHECK(mp)

    /* Haven't found the field yet */
    retcode = MARC_RET_NO_FIELD;

    /* Look for positive field occs */
    if (field_occ >= 0) {

        /* Walk field directory looking for id and occurrence number */
        dp  = mp->fdirp;
        occ = 0;
        for (count=0; count<mp->field_count; count++) {

            /* If found requested field */
            if (dp->tag == field_id) {

                /* If requested occurrence */
                if (occ == field_occ) {

                    /* Got it */
                    retcode = 0;
                    break;
                }

                /* Didn't get it, but count occurrence and change retcode */
                ++occ;
                retcode = MARC_RET_NO_FLDOCC;
            }

            ++dp;
        }
    }

    /* Negative occs are searched in reverse */
    else {

        /* field_occ < 0 */
        count = mp->field_count - 1;
        dp    = mp->fdirp + count;
        occ   = -1;
        for (; count>=0; count--) {

            /* If found requested field */
            if (dp->tag == field_id) {

                /* If requested occurrence */
                if (occ == field_occ) {

                    /* Got it */
                    retcode = 0;
                    break;
                }

                /* Didn't get it, but count occurrence and change retcode */
                --occ;
                retcode = MARC_RET_NO_FLDOCC;
            }

            --dp;
        }
    }


    /* If data found */
    if (!retcode) {

        /* Return requested information */
        *datap   = mp->rawbufp + dp->offset;
        *datalen = dp->len;

        /* Set the current field cursor to the found field */
        mp->cur_field = count;
    }

    return (retcode);

} /* marc_get_field */
