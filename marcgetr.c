/************************************************************************
* marc_get_record ()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Validate that a record meets minimum marc requirements.         *
*                                                                       *
*       Create the marc record in the marc buffer by computing          *
*       all system generated fields, directory, etc., and copying       *
*       in the data portions with required delimiters.                  *
*                                                                       *
*       Any empty fields, i.e,. fields for which marc_add_field()       *
*       was called but not marc_add_subfield(), are silently            *
*       discarded.  This is probably a convenience for conversion       *
*       programs.                                                       *
*                                                                       *
*       Return a pointer to, and length of, the record.                 *
*                                                                       *
*       For the convenience of the caller, a null terminator will       *
*       be placed immediately after the final marc record terminator.   *
*       The returned length is for all bytes up to, but not             *
*       including, the final null.                                      *
*                                                                       *
*       It is the caller's responsibility to write the record to        *
*       a file, or do whatever else is desired.                         *
*                                                                       *
*   PASS                                                                *
*       Pointer to marcctl structure.                                   *
*       Pointer to place to put a pointer to the start of the record.   *
*       Pointer to place to put the record length.                      *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcgetr.c,v 1.1 1998/12/28 21:02:33 meyer Exp meyer $
 * $Log: marcgetr.c,v $
 * Revision 1.1  1998/12/28 21:02:33  meyer
 * Initial revision
 *
 */

#include <string.h>         /* memcpy   */
#include <stdio.h>          /* sprintf  */
#include "marcdefs.h"

/* qsort compare routine needs access to control information */
static MARCCTL *S_mp;
static FLDDIR  *S_dp;

/* Internal prototypes */
static int  marc_xcompfld    (const void *, const void *);
static int  marc_xcompsf     (const void *, const void *);
static int  marc_xoutfield   (MARCCTL *, FLDDIR *, unsigned char *,
                              unsigned char *, size_t *);
int check(int);

/* Tells if a field is empty */
#define IS_EMPTYFLD(x) ((x->tag<10 && x->len<1) || (x->tag>9 && x->len<3))

int marc_get_record (
    MARCP   mp,             /* Ptr to marcctl           */
    unsigned char **recp,   /* Put pointer to rec here  */
    size_t  *reclen         /* Put record length here   */
) {
    FLDDIR  *dp;            /* Ptr to directory entry   */
    int     i,              /* Loop counter             */
            field_cnt,      /* Count of fields w/data   */
            stat;           /* Return from called func  */
    size_t  field_len,      /* Bytes in one output field*/
            data_len,       /* Bytes in data portion    */
            dir_len,        /* Bytes in directory       */
            rec_len;        /* Bytes in marc record     */
    char    numbuf[15];     /* Construct numbers here   */
    unsigned char *datap,   /* Ptr to output data       */
                  *dirp;    /* Ptr char fmt directory   */

    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Compute directory, data, and record lengths */
    /* Start after the leader, treated as field 0  */
    dp        = mp->fdirp + 1;
    data_len  = 0;
    field_cnt = 0;
    for (i = 1; i < mp->field_count; i++) {

        /* Only count fields with data in them */
        if (!IS_EMPTYFLD(dp)) {

            /* Add data length, plus length of field terminator */
            data_len += (dp->len + 1);
            ++field_cnt;
        }

        /* Next field */
        ++dp;
    }

    /* Add record terminator length */
    data_len += 1;

    /* Directory len is field count * size + directory terminator */
    dir_len = field_cnt * MARC_DIR_SIZE + 1;

    /* Logical record length is sum of all other lengths plus null terminator */
    rec_len = MARC_LEADER_LEN + dir_len + data_len;

    /* Test for max legal len */
    if (rec_len > (size_t) MARC_MAX_RECLEN)
        return MARC_ERR_RECLEN;

    /*
     *  Test for room in output buffer and enlarge if necessary
     *  Allocate rec_len + 1, because later we add a null at the end of record,
     *  See below line tagged...   REC NULL TERMINATOR
     *  But that must not increase the actual record length (rec_len).
     */
    if (rec_len > mp->marc_buflen) {
        if (marc_xallocate ((void **) &mp->marcbufp, (void **) &mp->endmarcp, &mp->marc_buflen, rec_len + 1, sizeof(unsigned char)) != 0) {
            return MARC_ERR_MARCALLOC;
        }
    }

    /* Copy in leader */
    memcpy (mp->marcbufp, mp->rawbufp, MARC_LEADER_LEN);

    /* Point to output destinations for directory and data */
    dirp  = mp->marcbufp + MARC_LEADER_LEN;
    datap = dirp + dir_len;

    /* Sort fields into directory order.
     * Default directory order is:
     *   If two fields have different tags, lower number tag output first.
     *   Else field added first is output first.
     * Caller can protect a range of fields from sorting, e.g.,
     *   for authority records, 400-599 might be protected, by calling
     *   marc_field_order().
     * Sorting requires access to the marcctl.
     */
    if (mp->field_sort) {
        S_mp = mp;
        qsort ((void *) mp->fdirp, mp->field_count, sizeof(FLDDIR),
               marc_xcompfld);
    }

    /* Create each field, in sorted order, starting after leader */
    dp = mp->fdirp + 1;
    for (i = 1; i < mp->field_count; i++) {

        /* Only fields with data */
        if (!IS_EMPTYFLD(dp)) {

            /* Create directory entry and field data */
            mp->cur_field = i;
            if ((stat = marc_xoutfield (mp, dp, dirp, datap, &field_len)) != 0) {
                return (stat);
            }

            /* Advance output */
            dirp  += MARC_DIR_SIZE;

            datap += field_len;
        }

        /* Advance input position */
        ++dp;
    }

    /* Add final terminators to directory and record */
    *dirp = MARC_FIELD_TERM;

    *datap++ = MARC_REC_TERM;

    /* Add null terminator, not marc but a convenience for caller */
    *datap = '\0';   // REC NULL TERMINATOR

    /*
     *  Insert logical record length into leader.
     *  We save and restore the byte after the record length
     *  so we can use sprintf, which plants a terminating 0 after
     *  the number.
     */
    sprintf (numbuf, "%0*u", MARC_RECLEN_LEN, rec_len);
    memcpy (mp->marcbufp + MARC_RECLEN_OFF, numbuf, MARC_RECLEN_LEN);

    /* Same for base address of data */
    sprintf (numbuf, "%0*u", MARC_BASEADDR_LEN, MARC_LEADER_LEN + dir_len);
    memcpy (mp->marcbufp + MARC_BASEADDR_OFF, numbuf, MARC_BASEADDR_LEN);

    /* Return info to caller */
    *recp   = mp->marcbufp;
    *reclen = rec_len;

    return 0;

} /* marc_get_record */


/************************************************************************
* marc_xoutfield ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Output all data for one field.  Handles differences between     *
*       field types, indicators, sorting of subfields, etc.             *
*                                                                       *
*       We do NOT handle the leader here.  Caller must not pass         *
*       a pointer to the leader's internal directory entry.             *
*                                                                       *
*   PASS                                                                *
*       Pointer to marcctl.                                             *
*       Ptr to internal marc directory for field to output.             *
*       Ptr to place to put output directory entry.                     *
*       Ptr to place to put field data.                                 *
*       Ptr to place to put length of output field.                     *
*           This includes indicators and field terminator.              *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

static int marc_xoutfield (
    MARCCTL       *mp,      /* Control for marc record      */
    FLDDIR        *fdp,     /* Ptr to internal field info   */
    unsigned char *dirp,    /* Put marc directory here      */
    unsigned char *datap,   /* Copy data here               */
    size_t        *flenp    /* Put field length here        */
) {
    SFDIR         *sdp;     /* Ptr to internal sf info      */
    size_t        len,      /* Length of output data        */
                  offset;   /* Offset in marc directory     */
    unsigned char *srcp,    /* Ptr to source for copy       */
                  *destp;   /* Ptr to next output byte      */
    int           i,        /* Loop counter                 */
                  retcode;  /* Return code                  */

    /* Keep track of directory offsets and lengths here */
    static size_t s_last_offset;
    static size_t s_last_len;


    /* No errors yet */
    retcode = 0;

    /* Point to start of output data */
    destp = datap;

    /* Output length is field length */
    len = fdp->len;

    /* Point to start of field in raw buffer to be output */
    srcp = mp->rawbufp + fdp->offset;

    /* Variable length fields have indicators and subfields */
    if (fdp->tag >= MARC_FIRST_VARFIELD) {

        /* Indicators */
        *destp++ = *srcp++;
        *destp++ = *srcp;

        /* Create subfield directory */
        if ((retcode = marc_xsfdir (mp)) == 0) {

            /* Sort the portion of it after the two indicators */
            if (mp->sf_sort && mp->sf_count > 3) {
                S_dp = fdp;
                qsort ((void *) (mp->sdirp + 2), mp->sf_count - 2,
                       sizeof(SFDIR), marc_xcompsf);
            }

            /* Output data in sorted order, after 2 indicators */
            sdp = mp->sdirp + 2;
            for (i=2; i<mp->sf_count; i++) {
                memcpy (destp, sdp->startp, sdp->len);
                destp += sdp->len;
                ++sdp;
            }
        }
    }

    else {
        /* Control fields are much simpler.  No indicators or sfs */
        memcpy (destp, srcp, fdp->len);
        destp += fdp->len;
    }

    if (!retcode) {

        /* Append field terminator */
        *destp = MARC_FIELD_TERM;
        ++len;

        /* Compute offset to field.
         * First field, offset always = 0.
         * Subsequent fields refer to offset to previous field.
         */
        if (mp->cur_field == 1)
            offset = 0;
        else
            offset = s_last_offset + s_last_len;

        /* Create marc directory entry */
        sprintf ((char *) dirp, "%03d%04u%05u", fdp->tag, len, offset);

        /* Remember for next field */
        s_last_offset = offset;
        s_last_len    = len;

        /* Tell caller how many bytes were output */
        *flenp = len;
    }

    else
        /* Cancel output from caller's point of view */
        *flenp = 0;

    return (retcode);

} /* marc_xoutfield */


/************************************************************************
* marc_xcompfld ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Compare two field directory entries for the directory qsort.    *
*       Sort order is:                                                  *
*           Field number.                                               *
*           If field numbers are the same, then order entered in the    *
*               directory.                                              *
*           If field numbers are within range protected from sorting,   *
*               then within that range only, we use the order           *
*               entered in the directory.                               *
*                                                                       *
*   PASS                                                                *
*       Pointer to first directory entry.                               *
*       Pointer to second.                                              *
*                                                                       *
*       marc_xcompfld() also requires access to the marcctl struct      *
*       for the record - which must be provided via filewide static.    *
*                                                                       *
*   RETURN                                                              *
*       Negative number if d1 sorts before d2.                          *
*       Else positive number.                                           *
*       We never return zero.                                           *
************************************************************************/

static int marc_xcompfld (
    const void *dir1p,
    const void *dir2p
) {
    FLDDIR *d1p,        /* dir1p in correct data type   */
           *d2p;        /* dir2p  "    "      "    "    */


    /* Cast pointer types */
    d1p = (FLDDIR *) dir1p;
    d2p = (FLDDIR *) dir2p;

    /* If field id and sort order both indicate the same thing */
    if (d1p->tag < d2p->tag && d1p->order < d2p->order)
        return (-1);

    if (d1p->tag > d2p->tag && d1p->order > d2p->order)
        return 1;

    /* Else check to see if fields are in range protected from sorting */
    if (S_mp->start_prot <= d1p->tag && S_mp->end_prot > d1p->tag &&
            S_mp->start_prot <= d2p->tag && S_mp->end_prot > d2p->tag)

        /* Fields are protected from sorting.  Use entry order */
        return (d1p->order - d2p->order);

    /* Fields are not protected.  If tags same, use order, else tags */
    if (d1p->tag == d2p->tag)
        return (d1p->order - d2p->order);
    return (d1p->tag - d2p->tag);

} /* marc_xcompfld */


/************************************************************************
* marc_xcompsf ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Compare two subfield directory entries for qsort.               *
*       This is just like marc_xcompfld() except that subfield          *
*       directory entries are compared.                                 *
*                                                                       *
*       Note that we are comparing subfield collation indicators,       *
*       not subfield tags.  Thus, by default, '1' sorts after 'z'.      *
*                                                                       *
*   PASS                                                                *
*       Pointer to first directory entry.                               *
*       Pointer to second.                                              *
*                                                                       *
*       marc_xcompsf() also requires access to the flddir struct        *
*       for the field - which must be provided via filewide static.     *
*                                                                       *
*   RETURN                                                              *
*       Negative number if d1 sorts before d2.                          *
*       Else positive number.                                           *
*       We never return zero.                                           *
************************************************************************/

static int marc_xcompsf (
    const void *dir1p,
    const void *dir2p
) {
    SFDIR *d1p,         /* dir1p in correct data type   */
          *d2p;         /* dir2p  "    "      "    "    */

    /* Cast pointer types */
    d1p = (SFDIR *) dir1p;
    d2p = (SFDIR *) dir2p;

    /* If sf coll and sort order both indicate the same thing */
    if (d1p->coll < d2p->coll && d1p->order < d2p->order)
        return (-1);
    if (d1p->coll > d2p->coll && d1p->order > d2p->order)
        return 1;

    /* Else check to see if fields are in range protected from sorting */
    if (S_dp->startprot <= d1p->coll && S_dp->endprot > d1p->coll &&
            S_dp->startprot <= d2p->coll && S_dp->endprot > d2p->coll)

        /* Subfields are protected from sorting.  Use entry order */
        return (d1p->order - d2p->order);

    /* Fields are not protected.  If colls same, use order, else tags */
    if (d1p->coll == d2p->coll)
        return (d1p->order - d2p->order);
    return (d1p->coll - d2p->coll);

} /* marc_xcompsf */
