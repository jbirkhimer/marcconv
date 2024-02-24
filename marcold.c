/************************************************************************
* marc_old ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Copy a marc record into buffers controlled by a marcctl         *
*       structure - making it available for data element retrieval      *
*       or update.                                                      *
*                                                                       *
*       For speed and simplicity, the copied data includes information  *
*       (field and record terminators) not found in a record that       *
*       we create from scratch using marc_new(), marc_add_field(),      *
*       and marc_add_subfield().  However these extra bytes don't       *
*       hurt anything.                                                  *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Pointer to valid marc record.                                   *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcold.c,v 1.2 2000/05/15 17:24:18 meyer Exp $
 * $Log: marcold.c,v $
 * Revision 1.2  2000/05/15 17:24:18  meyer
 * Fixed serious bug - not initializing mp->raw_datalen.
 *
 * Revision 1.1  1998/12/28 21:06:34  meyer
 * Initial revision
 *
 */

#include <ctype.h>
#include <string.h>
#include "marcdefs.h"

int marc_old (
    MARCP         mp,       /* Pointer to structure     */
    unsigned char *marcp    /* Pointer to record        */
) {
    FLDDIR        *dp;      /* Ptr to internal directory*/
    unsigned char *srcp;    /* Temp ptr into marc rec   */
    size_t        i,        /* Loop counter             */
                  lrecl,    /* Logical record length    */
                  base_addr,/* Base address of data     */
                  dirlen,   /* Directory length in bytes*/
                  datalen,  /* Length of data portion   */
                  datasum,  /* Sum of all data bytes    */
                  needlen;  /* New buffer size needed   */
    int           stat;     /* Return from lower level  */


    /* Can't re-initialize read only record */
    if (mp->read_only)
        return MARC_ERR_READ_ONLY;

    /* Get logical record length, first testing for good format */
    srcp = marcp + MARC_RECLEN_OFF;
    for (i=0; i<MARC_RECLEN_LEN; i++)
        if (!isdigit(*srcp++))
            return MARC_ERR_MRECLEN;
    lrecl = (size_t) marc_xnum (marcp + MARC_RECLEN_OFF, MARC_RECLEN_LEN);

    /* Test valid record length */
    if (*(marcp + lrecl - 1) != MARC_REC_TERM)
        return MARC_ERR_MRECTERM;

    /* Same for base address of data */
    srcp = marcp + MARC_BASEADDR_OFF;
    for (i=0; i<MARC_BASEADDR_LEN; i++)
        if (!isdigit(*srcp++))
            return MARC_ERR_MBASEADDR;
    base_addr = (size_t) marc_xnum (marcp + MARC_BASEADDR_OFF,
                                    MARC_BASEADDR_LEN);

    /* Test validity, looking for field term at end of directory */
    if (*(marcp + base_addr - 1) != MARC_FIELD_TERM)
        return MARC_ERR_MDIRTERM;

    /* Physically validate the directory */
    srcp   = marcp + MARC_LEADER_LEN;
    dirlen = base_addr - MARC_LEADER_LEN - 1;
    for (i=0; i<dirlen; i++)
        if (!isdigit(*srcp++))
            return MARC_ERR_MDIRCHARS;

    /* Initialize marcctl for new record.
     * This eliminates whatever was there.
     * Also performs marc_xcheck() for us.
     */
    if ((stat = marc_new (mp)) != 0)
        return (stat);

    /* Ensure sufficient room in output buffer */
    datalen = lrecl - base_addr + 1;
    needlen = MARC_LEADER_LEN + datalen;
    if (marc_xallocate ((void **) &mp->rawbufp, (void **) &mp->endrawp,
                        &mp->raw_buflen, needlen, sizeof(unsigned char)) != 0)
        return MARC_ERR_RAWALLOC;

    /* Copy in leader, overwriting default set in marc_new() */
    memcpy (mp->rawbufp, marcp, MARC_LEADER_LEN);
    mp->raw_datalen = MARC_LEADER_LEN;

    /* Same for data */
    memcpy (mp->rawbufp + MARC_LEADER_LEN, marcp + base_addr, datalen);
    mp->raw_datalen += datalen;

    /* Convert directory length to count */
    dirlen /= MARC_DIR_SIZE;

    /* Ensure sufficient room.
     * One directory entry for each marc field, plus one for
     *   the leader - which we temporarily treat as a control field.
     */
    if (marc_xallocate ((void **) &mp->fdirp, (void **) NULL,
                        (size_t *) &mp->field_max, dirlen + 1,
                        sizeof(FLDDIR)) != 0)
        return MARC_ERR_DIRALLOC;

    /* Point to first byte of marc directory */
    srcp = marcp + MARC_LEADER_LEN;

    /* First entry in internal directory, after field 0 (leader) */
    dp = mp->fdirp + 1;

    datasum = 0;
    for (i=0; i<dirlen; i++) {

        /* Decode marc directory.
         * Note that our internal directory does not include
         *   the field terminator in its length.
         */
        dp->tag    = marc_xnum (srcp, MARC_TAG_LEN);
        dp->len    = marc_xnum (srcp + MARC_TAG_LEN, MARC_FIELDLEN_LEN) - 1;
        dp->offset = marc_xnum (srcp + (MARC_TAG_LEN + MARC_FIELDLEN_LEN),
                                MARC_OFFSET_LEN);

        /* Add to sum for later integrity check */
        datasum += dp->len + 1;

        /* Normalize offset for stored leader */
        dp->offset += MARC_LEADER_LEN;

        /* Default values of internal directory fields */
        dp->order     = i;
        dp->pos_bits  = 0;
        dp->startprot = 0;
        dp->endprot   = 0;

        /* Next entry */
        ++dp;
        ++mp->field_count;
        srcp += MARC_DIR_SIZE;
    }

    /* Another integrity check                                  */
    /* datalen has 2 extra bytes, for recterm + null delimiters */
    if (datasum != datalen - 2)
        return MARC_ERR_LENMATCH;

    /* All done */
    return 0;

} /* marc_old */
