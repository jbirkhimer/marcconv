/************************************************************************
* marc_free ()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Free all buffers used in marc processing.                       *
*                                                                       *
*   PASS                                                                *
*       Pointer to marcctl structure.                                   *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcfree.c,v 1.1 1998/12/28 20:35:21 meyer Exp meyer $
 * $Log: marcfree.c,v $
 * Revision 1.1  1998/12/28 20:35:21  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"
void marc_dealloc(void *, int);

int marc_free (
    MARCP mp        /* Ptr to marcctl       */
) {
    /* Minimal test to be sure this is a marcctl structure */
    if (mp->start_tag != MARC_START_TAG)
        return MARC_ERR_START_TAG;
    if (mp->end_tag != MARC_END_TAG)
        return MARC_ERR_END_TAG;

    /* Free all allocated buffers */

    if (mp->sdirp) {
#ifdef DEBUG
        marc_dealloc(mp->sdirp, 400); //TAG:400
#else
        free (mp->sdirp);
#endif
        mp->sdirp = NULL;
    }

    if (mp->fdirp) {
#ifdef DEBUG
        marc_dealloc (mp->fdirp, 401);  //TAG:401
#else
        free (mp->fdirp);
#endif
        mp->fdirp = NULL;
    }

    if (mp->marcbufp) {
#ifdef DEBUG
        marc_dealloc(mp->marcbufp, 402);  //TAG:402
#else
        free (mp->marcbufp);
#endif
        mp->marcbufp = NULL;
    }


    if (mp->rawbufp) {
#ifdef DEBUG
        marc_dealloc(mp->rawbufp, 403);  //TAG:403
#else
        free (mp->rawbufp);
#endif
        mp->rawbufp = NULL;
    }

    /* Set tags to prevent user re-using this memory, just in case */
    mp->start_tag = 0;
    mp->end_tag   = 0;

    /* Free structure itself */
#ifdef DEBUG
    marc_dealloc(mp, 404); //TAG:404
#else
    free (mp);
#endif
    mp = NULL;

    return 0;

} /* marc_free */
