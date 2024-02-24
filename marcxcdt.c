/************************************************************************
* marc_xcheck_data ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Check that the data to be entered is legal MARC data.           *
*                                                                       *
*       Because different applications might use MARC in strange        *
*       ways, it's hard to know what character set is valid here.       *
*                                                                       *
*       It will always be illegal to enter a binary 0, or a MARC        *
*       delimiter character as data.  What else may be illegal is       *
*       not always clear to the original author of this program.        *
*                                                                       *
*       Users might replace this function with their own if desired.    *
*                                                                       *
*   PASS                                                                *
*       Field id into which data will be added.                         *
*       Subfield code.                                                  *
*           See marc_add_subfield() for what can appear here.           *
*       Pointer to data to insert into the record.                      *
*       Length of data.                                                 *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

#include "marcdefs.h"

int marc_xcheck_data (
    int           field_id, /* Of field to which we are adding  */
    int           sf_code,  /* Of subfield                      */
    unsigned char *datap,   /* Pointer to data                  */
    size_t        datalen   /* Length, 1..n                     */
) {
    unsigned char *p;       /* Ptr into data                    */
    size_t        i;        /* Length counter                   */


    /* A simple check for reserved MARC delimiters */
    p = datap;
    for (i=0; i<datalen; i++) {
        if (*p <= MARC_SF_DELIM && *p >= MARC_REC_TERM)
            return MARC_ERR_BAD_CHAR;
        ++p;
    }

    /* Put anything else desired here */

    /* Purely to silence warnings - can't happen */
    if (field_id < 0 || sf_code < 0)
        return MARC_ERR_CHK_PARM;

    return 0;

} /* marc_xdata_check */

