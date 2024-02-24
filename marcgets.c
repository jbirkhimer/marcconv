/************************************************************************
* marc_get_subfield ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       Retrieve information about a subfield in the current field      *
*       using its subfield code and occurrence number.                  *
*                                                                       *
*       As a convenience for applications which like a uniform          *
*       interface to all MARC fields, marc_get_subfield() can be        *
*       used to retrieve data from the leader and fixed length          *
*       fields, or from variable field indicators.  See parameters      *
*       below.                                                          *
*                                                                       *
*       See also marc_get_field(), marc_get_indic(), marc_get_record()  *
*       and marc_get_item().                                            *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Subfield code to find.                                          *
*           This is a one character subfield id, passed as an int.      *
*           For the leader and "fixed" fields, the subfield code        *
*               is the position desired in the field, origin 0.         *
*               For example, to get data in bytes 5-7 of field 8        *
*               (008) pass 4 as the "subfield code", 3 as the           *
*               "occurrence" number.                                    *
*           For "variable" fields, codes defined in marc.h represent    *
*               indicators (MARC_INDIC1 and MARC_INDIC2) or use         *
*               marc_get_indic().                                       *
*       Occurrence number, origin 0, for this subfield code.            *
*           For fixed length fields, the "occurrence" should be the     *
*               number of bytes requested, beginning with the byte      *
*               given in the subfield code.  This number will be        *
*               echoed back in the returned length, if the bytes        *
*               exist.                                                  *
*           Occurrence number is ignored for indicators.                *
*           Negative occurrence numbers count backwards from end, e.g.  *
*               -1 = last occ.                                          *
*               -2 = next to last occ.                                  *
*               etc.                                                    *
*       Ptr to place to put pointer to subfield data.                   *
*           Will point to start of data, after subfield delimiter       *
*           and code.  Returns NULL if data not found.                  *
*       Ptr to place to put length of subfield.                         *
*           Length does not include subfield delimiter or code.         *
*           Returns 0 if data not found, short length if not all        *
*           data available for fixed length segment of fixed field.     *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*           Negative number indicates serious, structural error.        *
*           Positive number indicates data not found (or not all found).*
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcgets.c,v 1.2 1999/06/14 21:42:46 meyer Exp $
 * $Log: marcgets.c,v $
 * Revision 1.2  1999/06/14 21:42:46  meyer
 * Added support for negative occurrence numbers.
 *
 * Revision 1.1  1998/12/28 21:03:02  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_get_subfield (
    MARCP         mp,           /* Pointer to structure     */
    int           sf_id,        /* Subfield code/identifier */
    int           sf_occ,       /* Occurrence number        */
    unsigned char **datap,      /* Put data pointer here    */
    size_t        *datalen      /* Put data only length here*/
) {
    FLDDIR        *dp;          /* Ptr to directory entry   */
    SFDIR         *sp;          /* Ptr to directory entry   */
    int           count,        /* Subfield loop counter    */
                  occ,          /* Occurrence counter       */
                  retcode;      /* Return code              */


    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Nothing found yet */
    *datap   = NULL;
    *datalen = 0;

    /* Function only works for variable length fields */
    if (MARC_CUR_TAG(mp) < MARC_FIRST_VARFIELD) {

        /* Point to current field */
        dp = mp->fdirp + mp->cur_field;

        /* Treat subfield "id" as an offset (origin 0)
         *   to data within leader or fixed field.
         */
        if (dp->len <= (size_t) sf_id)
            retcode = MARC_RET_FX_OFFSET;
        else {
            *datap = mp->rawbufp + dp->offset + sf_id;

            /* Treat "occurrence" number as requested length */
            if (dp->len < (size_t) (sf_occ + sf_id)) {
                *datalen = dp->len - sf_id;
                retcode = MARC_RET_FX_LENGTH;
            }
            else {
                *datalen = sf_occ;
                retcode  = 0;
            }
        }
    }

    else {

        /* Requested subfield of variable length field.
         * Create a subfield directory.
         * Does nothing if the right directory already exists.
         */
        if ((retcode = marc_xsfdir (mp)) != 0)
            return (retcode);

        /* Haven't found the subfield yet */
        retcode = MARC_RET_NO_SUBFIELD;

        /* Look for positive occurrence numbers */
        if (sf_occ >= 0) {

            /* Walk subfield directory looking for id and occ number */
            sp  = mp->sdirp;
            occ = 0;
            for (count=0; count<mp->sf_count; count++) {

                /* If found requested field */
                if (sp->id == sf_id) {

                    /* If requested occurrence */
                    if (occ == sf_occ) {

                        /* Got it */
                        retcode = 0;
                        break;
                    }

                    /* Didn't get it, but count occ and change retcode */
                    ++occ;
                    retcode = MARC_RET_NO_SUBFLDOCC;
                }

                ++sp;
            }
        }

        else {

            /* Negative occs found by walking backwards */
            count = mp->sf_count - 1;
            sp    = mp->sdirp + count;
            occ   = -1;
            for (; count>=0; count++) {

                /* If found requested field */
                if (sp->id == sf_id) {

                    /* If requested occurrence */
                    if (occ == sf_occ) {

                        /* Got it */
                        retcode = 0;
                        break;
                    }

                    /* Didn't get it, but count occ and change retcode */
                    --occ;
                    retcode = MARC_RET_NO_SUBFLDOCC;
                }

                --sp;
            }
        }

        /* If subfield found */
        if (!retcode) {

            /* Return requested information.
             * If "sf" is the indicators, we have it.
             * Else compensate for subfield delimiter and code.
             */
            *datap   = sp->startp;
            *datalen = sp->len;
            if (sf_id > MARC_INDIC2) {
                *datap   += 2;
                *datalen -= 2;
            }

            /* Set position indicator for future marc_next_subfield() */
            mp->cur_sf = count;
        }
    }

    return (retcode);

} /* marc_get_subfield */
