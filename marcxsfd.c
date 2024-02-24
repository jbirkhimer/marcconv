/************************************************************************
* marc_xsfdir ()                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Construct a directory of subfields for the current field.       *
*                                                                       *
*       This directory is for internal use only.  It allows us to find  *
*       multiple subfields in a field without parsing the field         *
*       multiple times, and is also used in sorting subfields within    *
*       a field.                                                        *
*                                                                       *
*   PASS                                                                *
*       Pointer to marcctl.                                             *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

/* $Id: marcxsfd.c,v 1.1 1998/12/28 21:16:59 meyer Exp meyer $
 * $Log: marcxsfd.c,v $
 * Revision 1.1  1998/12/28 21:16:59  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_xsfdir (
    MARCP         mp            /* Control for marc record  */
) {
    unsigned char *endp,        /* Ptr after end of field   */
                  *datap;       /* Ptr to subfield data     */
    FLDDIR        *fdp;         /* Ptr to current field dir */
    SFDIR         *sdp;         /* Ptr to sf directory entry*/
    int           sf_count,     /* Count of subfields       */
                  retcode;      /* Return code              */


    /* Error if no current field */
    if (mp->cur_field < 0)
        return MARC_ERR_CUR_FIELD;

    /* No errors yet */
    retcode = 0;

    /* Only execute if we haven't already built a subfield directory
     *   for this field.
     * Other functions must invalidate mp->cur_sf_field if
     *   they have changed something which can affect this.
     *   See marc_add_field() and marc_add_sub_field().
     */
    if (mp->cur_sf_field != mp->cur_field) {

        /* Point to field directory */
        fdp = mp->fdirp + mp->cur_field;
        mp->cur_sf_field = mp->cur_field;

        /* Control fields have no subfields */
        if (fdp->tag < MARC_FIRST_VARFIELD) {
            mp->sf_count = 0;
            return MARC_ERR_CTLFIELD;
        }

        /* Initialize pointers and counter */
        datap = mp->rawbufp + fdp->offset;
        endp  = datap + fdp->len;
        sdp   = mp->sdirp;

        /* Install 2 indicator "subfields".
         * This provides a uniform interface to calling routines
         *   enabling them to treat indicators as just another subfield.
         */
        for (sf_count=0; sf_count<2; sf_count++) {
            sdp->id     = (unsigned char) ((sf_count == 0) ?
                                            MARC_INDIC1 : MARC_INDIC2);
            sdp->startp = datap++;
            sdp->len    = 1;
            sdp->order  = (unsigned char) sf_count;
            ++sdp;
        }

        /* Scan field, looking for subfield delimiters */
        while (datap < endp) {

            /* If on subfield delimiter */
            if (*datap == MARC_SF_DELIM) {

                /* Don't exceed directory size */
                if (sf_count > mp->sf_max) {
                    retcode = MARC_ERR_MAX_SFS;
                    break;
                }

                /* Set length of previous var subfield, if there is one */
                if (sf_count > 2)
                    (sdp-1)->len = (size_t) (datap - (sdp-1)->startp);

                /* Set info on current subfield.
                 * We store the subfield collation to speed sorting
                 */
                sdp->startp = datap;
                sdp->id     = *(datap + 1);
                sdp->coll   = mp->sf_collate[sdp->id];
                sdp->order  = (unsigned char) sf_count;

                /* Ready for next */
                ++sdp;
                ++sf_count;
            }

            ++datap;
        }

        /* Length of last subfield */
        (sdp-1)->len = (size_t) (datap - (sdp-1)->startp);

        /* Set final count into marcctl */
        mp->sf_count = sf_count;

        /* We haven't positioned to any subfields in this field yet */
        mp->cur_sf = -1;

        /* If no subfields found, caller built field and put nothing in it */
        if (sf_count == 0)
            retcode = MARC_ERR_EMPTY_FLD;
    }

    return (retcode);

} /* marc_xsfdir */
