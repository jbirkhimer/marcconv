/************************************************************************
* marc_add_subfield ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       Add a subfield to the current field.                            *
*                                                                       *
*       Typical usage is to call marc_add_field() to create a new       *
*       field, then to call marc_add_subfield() one or more times       *
*       to put subfield data into the field.                            *
*                                                                       *
*       Subfields may be added in any order, however they will, by      *
*       default, be placed into the field using the collation           *
*       order defined in marc_get_record().                             *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Subfield code.                                                  *
*           This is a one character subfield id, passed as an int.      *
*           For the leader and "fixed" fields, the subfield code        *
*               is the position desired in the field, origin 0.         *
*               For example, to put data in bytes 5-7 of field 8        *
*               (008) pass 4 as the subfield code, 3 as length.         *
*           For "variable" fields, codes defined in marc.h represent    *
*               indicators (or use marc_set_indic().)                   *
*       Pointer to data to insert into the record.                      *
*       Length of data.                                                 *
*           If this is MARC_SF_STRLEN, we assume the data is null       *
*               terminated and use strlen() to find its length.         *
*           Otherwise this is the length of the subfield to add.        *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcadds.c,v 1.6 1999/11/29 22:01:23 meyer Exp meyer $
 * $Log: marcadds.c,v $
 * Revision 1.6  1999/11/29 22:01:23  meyer
 * Added a couple of initializations to silence unneeded warnings in MSVC.
 *
 * Revision 1.5  1999/08/30 23:12:08  meyer
 * Added data check on incoming data.
 *
 * Revision 1.4  1999/07/28 20:12:43  meyer
 * Fixed bug in fixed_len sfs when new data is beyond end of existing field.
 *
 * Revision 1.3  1999/06/14 21:41:43  meyer
 * Added support for negative occurrence numbers.
 * Fixed bug in adding sfs to inner fields, was not updating offsets after.
 *
 * Revision 1.2  1999/02/09 02:40:13  meyer
 * Fixed bug concerning inappropriate use of the fill character on fixed
 * fields.
 *
 * Revision 1.1  1998/12/28 20:30:15  meyer
 * Initial revision
 *
 */

#include <string.h>
#include "marcdefs.h"

/* Types of marc_add_subfield() operation */
typedef enum addtype {
    add_fixed,                      /* Adding fixed bytes           */
    add_var_sf,                     /* Adding var len subfield      */
    add_indics                      /* Adding indicators            */
} ADDTYPE;

int marc_add_subfield (
    MARCP   mp,             /* Pointer to structure     */
    int     sf_code,        /* Field tag/identifier     */
    unsigned char *datap,   /* Pointer to data          */
    size_t  datalen         /* Len data, or 0           */
) {
    FLDDIR  *dp;            /* Ptr to directory entry   */
    ADDTYPE add_type;       /* Fixed, var, or indics    */
    int     i,              /* Field loop counter       */
            add_len,        /* Adding this many bytes   */
            stat;           /* Return code from func    */
    size_t  new_buflen,     /* New size if buf too small*/
            data_offset;    /* Offset to insertion point*/
    unsigned char *insertp, /* Insert data here         */
            *shiftp,        /* Shift from here          */
            *srcp,          /* Right end of shift       */
            *destp;         /* To here                  */


    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Can't add subfields to read only record */
    if (mp->read_only)
        return MARC_ERR_READ_ONLY;

    /* Must have added a field first */
    if (mp->cur_field < 0)
        return MARC_ERR_NO_AFIELD;

    /* These initializations aren't needed, but they silence warnings
     *   from the Microsoft compiler
     */
    data_offset = 0;
    shiftp      = NULL;

    /* Get length of data to add */
    if (datalen == MARC_SF_STRLEN)
        datalen = strlen ((char *) datap);

    /* But don't know yet whether we have to add to length of record */
    add_len = 0;

    /* Point to current field */
    dp = mp->fdirp + mp->cur_field;

    /* Find type of add operation */
    if (dp->tag < MARC_FIRST_VARFIELD)
        add_type = add_fixed;
    else {
        if (sf_code == MARC_INDIC1 || sf_code == MARC_INDIC2)
            add_type = add_indics;
        else
            add_type = add_var_sf;
    }

    /* Validate data okay */
    if ((stat = marc_xcheck_data (dp->tag, sf_code, datap, datalen)) != 0)
        return (stat);

    /* Validation based on type */
    switch (add_type) {

        case add_indics:
            /* Only allowed len is 1 */
            if (datalen != 1)
                return MARC_ERR_INDICLEN;

            /* Otherwise, just handle this simple case right here */
            if (sf_code == MARC_INDIC1)
                *(mp->rawbufp + dp->offset) = *datap;
            else
                *(mp->rawbufp + dp->offset + 1) = *datap;
            return 0;

        case add_var_sf:
            /* Legal subfield code? */
            if ((stat = marc_ok_subfield (sf_code)) != 0)
                return stat;

            /* Check max field length */
            if (dp->len + datalen + 1 > MARC_MAX_VARLEN)
                return MARC_ERR_FIELDLEN;

            /* We'll need enough room in the buffer for
             *   existing data,
             *   new data,
             *   new delimiter and subfield code
             */
            add_len = datalen + 2;

            /* Data will be appended to the end of the current field */
            data_offset = dp->offset + dp->len;

            /* We shift data if necessary to make room for it */
            shiftp = mp->rawbufp + data_offset;

            break;

        case add_fixed:
            /* sf_code is a byte offset, must be reasonable */
            if (sf_code < 0 || sf_code > MARC_MAX_FIXLEN)
                return MARC_ERR_FIELDLEN;

            /* How much room we need depends on whether we are
             *   overwriting part or all of an existing field
             *   or adding to a new one.
             * If we start adding anywhere but the start of the
             *   field, we need padding before it.
             */
            if ((add_len = sf_code + datalen - dp->len) < 0)
                add_len = 0;

            /* Data inserted at passed offset, adjusted for origin */
            data_offset = dp->offset + sf_code;

            /* We shift data from the data offset, or end of field,
             *   whichever is less.
             * This is because end of field may be before data offset
             */
            shiftp = mp->rawbufp + data_offset;
            if (shiftp > mp->rawbufp + dp->offset + dp->len)
                shiftp = mp->rawbufp + dp->offset + dp->len;
    }

    /* If too small, increase raw buffer */
    new_buflen = mp->raw_datalen + add_len + 1;
    if (new_buflen >= mp->raw_buflen) {

        /* Marc limits overall record by 5 char ASCII record length */
        if (new_buflen > (size_t) MARC_MAX_MARCDATA)
            return MARC_ERR_RECLEN;

        /* Test buffer and get larger if needed */
        if (marc_xallocate ((void **) &mp->rawbufp, (void **) &mp->endrawp,
                            &mp->raw_buflen, new_buflen,
                            sizeof(unsigned char)) != 0)
            return MARC_ERR_RAWALLOC;
    }

    /* Insert is at data_offset from buffer start */
    insertp = mp->rawbufp + data_offset;

    /* If not adding to the end of the record, shift stuff down */
    if (data_offset != mp->raw_datalen && add_len > 0) {

        /* Shift from end of record */
        srcp    = mp->rawbufp + mp->raw_datalen ;
        destp   = srcp + add_len;
        while (srcp > shiftp)
            *--destp = *--srcp;
    }

    /* Fixed length fields may require padding
     * If last byte of current field is before our insertion point
     *   then pad with fill chars from end of existing field to
     *   beginning of new data.
     */
    if (add_type == add_fixed) {
        destp = shiftp;
        while (destp < insertp)
            *destp++ = MARC_FILL_CHAR;
    }

    /* Var len fields get delimiter + sf_code */
    else {
        *insertp++ = MARC_SF_DELIM;
        *insertp++ = (unsigned char) sf_code;
    }

    /* Copy in the data */
    memcpy (insertp, datap, datalen);

    /* Update buffer used and field length */
    mp->raw_datalen += add_len;
    dp->len         += add_len;

    /* Update any fields beyond this one to reflect new offsets */
    if (add_len) {
        ++dp;
        for (i=mp->cur_field+1; i<mp->field_count; i++) {
            dp->offset += add_len;
            ++dp;
        }
    }

    /* Invalidate any previous subfield parse.
     * We could get more specific here to only invalidate it if it
     *   is known to have changed, but that optimization is unlikely
     *   to make a difference in real applications.
     */
    mp->cur_sf_field = MARC_NO_FIELD;

    return 0;

} /* marc_add_subfield */
