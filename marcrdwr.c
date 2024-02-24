/************************************************************************
* marc_read_rec ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Read a marc record from a sequential file.                      *
*                                                                       *
*   PASS                                                                *
*       Pointer to open file.                                           *
*       Pointer to buffer.                                              *
*       Length of buffer.                                               *
*                                                                       *
*                                                                       *
*   RETURN                                                              *
*       0   = Success.                                                  *
*       EOF = End of file.                                              *
*       Else error.                                                     *
************************************************************************/

/* $Id: marcrdwr.c,v 1.2 1998/12/31 21:17:46 meyer Exp $
 * $Log: marcrdwr.c,v $
 * Revision 1.2  1998/12/31 21:17:46  meyer
 * Replace return code of MARC_ERR_WRITE_PACK with actual error code
 * from lower level routine.
 *
 * Revision 1.1  1998/12/28 21:11:07  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"
#include <errno.h>
#include <string.h>

#ifdef DEBUG
extern int g_recnum;
#endif

int marc_read_rec (
    FILE          *fp,      /* Input file       */
    unsigned char *bufp,    /* Ptr to buffer    */
#ifdef DEBUG
    size_t        buflen,    /* Don't overflow   */
    int           *len
#else
    size_t        buflen    /* Don't overflow   */
#endif
) {
    size_t        lrecl;    /* From record      */

#ifdef DEBUG
    g_recnum++;
#endif

    /* Buffer has to be a minimum size before we start */
    if (buflen < 100)
        return MARC_ERR_READ_BSIZE;

    /* Read leader */
    if (fread (bufp, MARC_LEADER_LEN, 1, fp) != 1) {

        /* Normal end of file or error? */
        if (!feof (fp))
            return MARC_ERR_READ_LDR;
        return EOF;
    }

    /* Get logical record length */
    lrecl = marc_xnum (bufp, 5);

#ifdef DEBUG
    *len = lrecl;
#endif
    /* Check buf size again */
    if (buflen < lrecl)
        return MARC_ERR_READ_OFLOW;

    /* Read rest of record */
    if (fread (bufp + MARC_LEADER_LEN, lrecl - MARC_LEADER_LEN, 1, fp) != 1) {

        /* End of file indicates incomplete record at end */
        if (!feof (fp))
            return MARC_ERR_READ_INC;
        return MARC_ERR_READ;
    }

    /* Simple test of complete record */
    if (*((bufp + lrecl) - 1) != MARC_REC_TERM)
        return MARC_ERR_READ_TERM;

    /* Null terminate record */
    *(bufp + lrecl) = '\0';

    return 0;

} /* marc_read_rec */


/************************************************************************
* marc_write_rec ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Write one marc record to a sequential file.                     *
*                                                                       *
*       Records are written sequentially, without separators other      *
*       than the standard marc record terminator.                       *
*                                                                       *
*   PASS                                                                *
*       Pointer to open output file.                                    *
*       Pointer to marc control for record to write, or to packed       *
*           marc record.  We autodetect the difference.                 *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

int marc_write_rec (
    FILE   *fp,     /* Output file                      */
    void   *marc    /* Marc record ctl or full marcrec  */
) {
    int    stat;    /* Return from marc_get_record()    */
    unsigned char
           *datap;  /* Ptr to packed record             */
    size_t datalen; /* Length of packed record          */


    /* If we have an unpacked record, pack it */
    if ((marc_xcheck ((MARCP) marc)) == 0) {
        if ((stat = marc_get_record ((MARCP) marc, &datap, &datalen)) != 0)
            return stat;
    }
    else {
        /* Point to passed record */
        datap = (unsigned char *) marc;

        /* Find length of record from logical record length at start */
        datalen = marc_xnum (datap, 5);

        /* Sanity check */
        if (*((datap + datalen) - 1) != MARC_REC_TERM) {
            printf("Failed sanity check in %s: %d\n", __FILE__, __LINE__);
            return MARC_ERR_WRITE_TERM;
        }
    }

    /* Write it to the output file */
    if (fwrite (datap, datalen, 1, fp) != 1) {
        printf("Failed write in %s: %d err %s\n", __FILE__, __LINE__, strerror(errno));
        return MARC_ERR_WRITE;
    }

    return 0;

} /* marc_write_rec */
