/************************************************************************
* marc_init ()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Allocate and initialize a marcctl structure.                    *
*                                                                       *
*       Allocate and initialize all structures controlled by the        *
*       marcctl.                                                        *
*                                                                       *
*       Must be called in order to prepare for marc processing.         *
*                                                                       *
*   PASS                                                                *
*       Pointer to place to put pointer to marcctl structure.           *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcinit.c,v 1.2 2000/10/02 18:43:20 meyer Exp $
 * $Log: marcinit.c,v $
 * Revision 1.2  2000/10/02 18:43:20  meyer
 * Changed defaults to turn off subfield sorting.  This is a bad default
 * at NLM.
 *
 * Revision 1.1  1998/12/28 21:04:54  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

void *marc_calloc(int, int, int);

int marc_init (
    MARCP *mpp      /* Return pointer to marc_ctl here  */
) {
    MARCP mp;       /* marcctl to initialize and return */
    int   retcode;  /* Return code                      */


    /* Will return null pointer if error */
    *mpp = NULL;

    /* Allocate marc control struct */

#ifdef DEBUG
    if ((mp = (MARCCTL *) marc_calloc(sizeof(MARCCTL), 1, 1000)) == NULL) {  //TAG:1000
        return MARC_ERR_ALLOC;
    }
#else
    if ((mp = (MARCCTL *) calloc (sizeof(MARCCTL), 1)) == NULL) {
        return MARC_ERR_ALLOC;
    }
#endif

    /* Start and end tags.
     * This minimal initialization is required for marc_free()
     *   to recognize mp as a pointer to a marcctl structure.
     * Also tested in all calls to marc_xcheck().
     */
    mp->start_tag = MARC_START_TAG;
    mp->end_tag   = MARC_END_TAG;

    /* Raw buffer */
    if (marc_xallocate ((void **) &mp->rawbufp, (void **) &mp->endrawp, &mp->raw_buflen, MARC_DFT_RAWSIZE, sizeof(unsigned char)) != 0) {
        marc_free (mp);
        return MARC_ERR_RAWALLOC;
    }
    /* Marc buffer */
    if (marc_xallocate ((void **) &mp->marcbufp, (void **) &mp->endmarcp,
            &mp->marc_buflen, MARC_DFT_MARCSIZE, sizeof(unsigned char)) != 0) {
        marc_free (mp);
        return MARC_ERR_MARCALLOC;
    }

    /* Internal field directory */
    if (marc_xallocate ((void **) &mp->fdirp, (void **) NULL,
            (size_t *) &mp->field_max, MARC_DFT_DIRSIZE, sizeof(FLDDIR)) != 0){
        marc_free (mp);
        return MARC_ERR_DIRALLOC;
    }

    /* Internal subfield directory */
    if (marc_xallocate ((void **) &mp->sdirp, (void **) NULL,
            (size_t *) &mp->sf_max, MARC_DFT_SFDIRSIZE, sizeof(SFDIR)) != 0) {
        marc_free (mp);
        return MARC_ERR_SFDIRALLOC;
    }

    /* Default is that all fields sort, subfields do not sort for output */
    mp->start_prot =
    mp->end_prot   = MARC_NO_ORDER;
    mp->field_sort = 1;
    mp->sf_sort    = 0;

    /* Initialize to default subfield collation sequence */
    if ((retcode = marc_set_collate (mp, MARC_DFT_SF_COLL)) == 0) {

        /* Initialize for new record */
        if ((retcode = marc_new (mp)) == 0) {
            /* Make everything available to caller */
            *mpp = mp;
        }
    }
    return (retcode);

} /* marc_init */
