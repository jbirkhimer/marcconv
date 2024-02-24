/************************************************************************
* marc_del_field ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Delete the current field.                                       *
*                                                                       *
*       Only works if there is a current field.                         *
*                                                                       *
*       Can't delete the leader.                                        *
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

/* $Id: marcdelf.c,v 1.3 1999/07/27 02:42:25 meyer Exp $
 * $Log: marcdelf.c,v $
 * Revision 1.3  1999/07/27 02:42:25  meyer
 * Changed check for deleting leader from >1 to >0.  The leader is field 0.
 *
 * Revision 1.2  1998/12/28 20:28:59  meyer
 * Fixed bug when deleting last subfield in record.  move_len==0 but
 * still have to update the record and field control structures.
 *
 * Revision 1.1  1998/12/28 20:25:17  meyer
 * Initial revision
 *
 */

#include <string.h>
#include "marcdefs.h"

int marc_del_field (
    MARCP   mp              /* Pointer to structure     */
) {
    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Delete current if there is one and it's not the leader */
    if (mp->cur_field > 0) {
        marc_xdel_field (mp, mp->cur_field);
        return 0;
    }
    else
        return MARC_ERR_NO_DFIELD;

} /* marc_del_field */


/************************************************************************
* marc_xdel_field ()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Delete a field by it's field position.                          *
*                                                                       *
*       Internal routine for marc_del_field(), marc_get_record().       *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Field position in the directory, origin 0.                      *
*                                                                       *
*   RETURN                                                              *
*       0 = success.                                                    *
************************************************************************/

int marc_xdel_field (
    MARCP   mp,             /* Pointer to structure     */
    int     fpos            /* Position to delete       */
) {
    FLDDIR  *fdp;           /* Ptr to directory entry   */
    int     stat;           /* Return from called func  */
    size_t  move_bytes;     /* Number of bytes to shift */


    /* Find field to delete.  Caller must have already checked this */
    fdp = mp->fdirp + fpos;

    /* If any data there, compress it out */
    if (fdp->len)
        if ((stat = marc_xcompress (mp, fdp->offset, fdp->len)) != 0)
            return (stat);

    /* Compress directory entry by right number of bytes */
    if ((move_bytes = ((mp->field_count - 1) - fpos) * sizeof(FLDDIR)) > 0)
        memmove (fdp, fdp+1, move_bytes);
    --mp->field_count;

    /* Adjust position of current field if possible */
    if (mp->cur_field >= fpos)
        --mp->cur_field;

    return 0;

} /* marc_xdel_field */


/************************************************************************
* marc_xcompress ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Compress bytes out of the raw data buffer.                      *
*                                                                       *
*       Caller should have performed all checks.                        *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Offset to first byte to compress out.                           *
*       Number of bytes to compress out.                                *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
************************************************************************/

int marc_xcompress (
    MARCP   mp,             /* Pointer to structure     */
    size_t  offset,         /* Compress starting here   */
    size_t  del_len         /* For this many bytes      */
) {
    unsigned char *srcp,    /* Source of data to copy   */
                  *destp;   /* Destination for copy     */
    FLDDIR  *fdp,           /* Ptr to directory entry   */
            *end_fdp;       /* Ptr after end of directry*/
    size_t  move_len;       /* Bytes to move            */


    /* Find source and destination for shift */
    destp = mp->rawbufp + offset;
    srcp  = destp + del_len;

    /* Number of bytes to move */
    move_len = (size_t) ((mp->rawbufp + mp->raw_datalen) - srcp);

    /* If there are any, shift the lower bytes up */
    if (move_len)
        memmove (destp, srcp, move_len);

    /* Find first affected directory entry
     * We'll actually search from after first entry (leader)
     *   since the leader cannot be deleted from a record.
     */
    fdp     = mp->fdirp;
    end_fdp = fdp + mp->field_count;
    while (++fdp < end_fdp) {
        if (fdp->offset > offset)
            break;
    }

    /* Deleted bytes might be inside a field.
     * Check last entry with offset before deleted bytes.
     * Since we can't delete leader with this technique, and
     *   leader has an entry in the field directory, we don't
     *   have to worry about there not being a previous entry.
     */
    --fdp;
    if (fdp->offset + fdp->len >= offset)
        fdp->len -= del_len;

    /* Adjust all directory offsets after deleted data */
    while (++fdp < end_fdp) {
        if (fdp->offset > offset)
            fdp->offset -= del_len;
    }

    /* Adjust data end info */
    mp->raw_datalen -= del_len;

    /* Dirty the current subfield directory */
    mp->cur_sf_field = MARC_NO_FIELD;

    /* Might add checking in the future, but none for now */
    return 0;

} /* marc_xcompress */
