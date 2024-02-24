/************************************************************************
* marc_del_subfield ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       Delete the current subfield.                                    *
*                                                                       *
*       Must establish valid position first via marc_get_subfield()     *
*       or another positioning function.                                *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcdels.c,v 1.2 2000/03/20 15:54:45 meyer Exp $
 * $Log: marcdels.c,v $
 * Revision 1.2  2000/03/20 15:54:45  meyer
 * Added initialization of retcode variable.
 *
 * Revision 1.1  1998/12/28 20:24:26  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_del_subfield (
    MARCP mp            /* Pointer to structure     */
) {
    SFDIR *sfp;         /* sfdir for sf to delete   */
    int   retcode;      /* Return code              */


    /* Test, return if failed */
    MARC_XCHECK(mp)

    retcode = 0;

    /* Only works on variable fields */
    if ( (mp->fdirp + mp->cur_field)->tag >= MARC_FIRST_VARFIELD) {

        /* Update subfield directory if necessary */
        if ((retcode = marc_xsfdir (mp)) == 0) {

            /* This next test will fail under either of two conditions:
             *   1. We aren't positioned at a variable subfield.
             *        sf 0 = indicator 1.
             *        sf 1 = indicator 2.
             *   2. marc_xsfdir rebuilt the subfield directory, setting
             *      mp->cur_sf = MARC_NO_FIELD (-1).
             *      If it had to rebuild the directory, the original
             *      mp->cur_sf was not current and could not be used.
             */
            if (mp->cur_sf > 1) {

                /* Point to subfield directory entry */
                sfp = mp->sdirp + mp->cur_sf;

                /* Delete bytes at that point */
                retcode = marc_xcompress (mp,
                          (size_t) (sfp->startp - mp->rawbufp), sfp->len);

                /* Subfield directory and position now invalid */
                mp->cur_sf = MARC_NO_FIELD;
            }
            else
                retcode = (mp->cur_sf == MARC_NO_FIELD) ?
                        MARC_ERR_NO_DELPOS : MARC_ERR_DEL_INDIC;
        }
    }

    return (retcode);

} /* marc_del_subfield */
