/************************************************************************
* marc_rename_subfield ()                                               *
*                                                                       *
*   DEFINITION                                                          *
*       Rename the last subfield of the current field by changing       *
*       its subfield code.                                              *
*                                                                       *
*       This admittedly exotic function is used in a marc->marc         *
*       conversion program and could be used in editors or other        *
*       software.                                                       *
*                                                                       *
*   PASS                                                                *
*       Pointer to marcctl structure.                                   *
*       New subfield id/code.                                           *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else no position to save, or error.                             *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcrens.c,v 1.1 1998/12/28 21:12:35 meyer Exp meyer $
 * $Log: marcrens.c,v $
 * Revision 1.1  1998/12/28 21:12:35  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_rename_subfield (
    MARCP         mp,       /* Ptr to marcctl       */
    int           new_id    /* New field id/tag     */
) {
    unsigned char *datap;   /* Ptr to subfield data */
    size_t        datalen;  /* Len current data     */
    int           sf_id,    /* Current sf code      */
                  retcode;  /* Return code          */



    /* Position to last subfield in field
     * Performs MARC_XCHECK for us along the way
     * Also ensures that field is of type that has subfields
     */
    if ((retcode = marc_pos_subfield (mp, -1, &sf_id, &datap, &datalen))==0) {

        /* Is new subfield legal marc? */
        if ((retcode = marc_ok_subfield (new_id)) == 0)

            /* Modify the byte just before the actual data */
            *(datap - 1) = (unsigned char) new_id;
    }

    return retcode;

} /* marc_rename_subfield */
