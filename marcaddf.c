/************************************************************************
* marc_add_field ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Add a field to the internal directory and make it the current   *
*       context for data additions using marc_add_subfield()            *
*                                                                       *
*       Fields may be added in any order, however they will,            *
*       by default, be placed into the marc record in field             *
*       id number order.                                                *
*                                                                       *
*       For fields which allow them, default indicators (both blank)    *
*       are inserted in the raw data buffer, but no other data is       *
*       inserted.  Use marc_add_subfield() to modify indicators if      *
*       required, and to add variable length subfields.                 *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Field number to add.                                            *
*           0 = Leader.                                                 *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcaddf.c,v 1.1 1998/12/28 20:30:00 meyer Exp $
 * $Log: marcaddf.c,v $
 * Revision 1.1  1998/12/28 20:30:00  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_add_field (
    MARCP   mp,             /* Pointer to structure     */
    int     field_id        /* Field tag/identifier     */
) {
    FLDDIR  *dp;            /* Ptr to directory entry   */
    int     i;              /* Loop counter             */
    unsigned char *datap;   /* Ptr to data in field     */


    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Can't add fields to read only record */
    if (mp->read_only)
        return MARC_ERR_READ_ONLY;

    /* Test range of passed field id */
    if (field_id < 0 || field_id > MARC_MAX_FIELDID)
        return MARC_ERR_FIELDID;

    /* If one of fixed fields, can't repeat them */
    if (field_id < MARC_FIRST_VARFIELD) {
        for (i=0; i<mp->field_count; i++) {
            if (mp->fdirp->tag == field_id)
                return MARC_ERR_DUPFFLD;
        }
    }

    /* Ensure sufficient room in the field directory */
    if (mp->field_count >= mp->field_max) {

        /* Don't exceed reasonable limits */
        if (mp->field_count + MARC_DFT_DIRINC > MARC_MAX_FLDCOUNT)
            return MARC_ERR_MAXFIELDS;

        /* Re-size field directory */
        if (marc_xallocate ((void **) &mp->fdirp, (void **) NULL,
                            (size_t *) &mp->field_max,
                            mp->field_max + MARC_DFT_DIRINC,
                            sizeof(FLDDIR)) != 0)
            return MARC_ERR_DIRALLOC;
    }

    /* Create directory entry */
    dp           = mp->fdirp + mp->field_count;
    dp->tag      = field_id;
    dp->offset   = mp->raw_datalen;
    dp->len      = 0;
    dp->order    = mp->field_count;
    dp->pos_bits = 0;

    /* Create defaults for variable length fields */
    if (field_id >= MARC_FIRST_VARFIELD) {

        /* Need room in raw buffer for indicators plus some data */
        if (mp->raw_buflen < mp->raw_datalen + 5) {

            if (marc_xallocate ((void **) &mp->rawbufp, (void **) &mp->endrawp,
                            &mp->raw_buflen, mp->raw_buflen + MARC_DFT_RAWINC,
                            sizeof(unsigned char)) != 0)
                return MARC_ERR_RAWALLOC;
        }

        /* Indicators */
        datap = mp->rawbufp + dp->offset;
        *datap++ = ' ';
        *datap   = ' ';
        dp->len  = 2;
        mp->raw_datalen += 2;

        /* Assume default subfield collation */
        dp->startprot =
        dp->endprot   = 0;
    }

    /* Set current and next fields */
    mp->cur_field = mp->field_count++;

    /* Any field parse is no longer guaranteed accurate.
     * We could get more specific here to only invalidate it if it
     *   is known to have changed, but that optimization is unlikely
     *   to make a difference in real applications.
     */
    mp->cur_sf_field = MARC_NO_FIELD;

    return 0;

} /* marc_add_field */
