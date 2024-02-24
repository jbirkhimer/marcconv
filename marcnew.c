/************************************************************************
* marc_new ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Initialize a marcctl structure, for processing a new record.    *
*                                                                       *
*       Creates an empty record with a default leader.                  *
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

#include "marcdefs.h"

void marc_dealloc(void *, int);

int marc_new (
    MARCP mp        /* Pointer to structure     */
) {
    int   stat;     /* Status/return code       */


    /* Test, return if failed */
    MARC_XCHECK(mp);

    /* Can't re-initialize read only record */
    if (mp->read_only)
        return MARC_ERR_READ_ONLY;

    /* Free all allocated buffers */
    //if (mp->sdirp) {
        //free (mp->sdirp);
        //mp->sdirp = NULL;
    //}
    //if (mp->sdirp) {
        //marc_dealloc(mp->sdirp, 500); //TAG:500
        //mp->sdirp = NULL;
    //}
    //if (mp->fdirp) {
        //free (mp->fdirp);
        //mp->fdirp = NULL;
    //}
    //if (mp->fdirp) {
        //marc_dealloc(mp->fdirp, 501);  //TAG:501
        //mp->fdirp = NULL;
    //}
    //if (mp->marcbufp) {
        //free (mp->marcbufp);
        //mp->marcbufp = NULL;
    //}
    //if (mp->marcbufp) {
        //marc_dealloc(mp->marcbufp, 502);  //TAG:502
        //mp->marcbufp = NULL;
    //}
    //if (mp->rawbufp) {
        //free (mp->rawbufp);
        //mp->rawbufp = NULL;
    //}
    //if (mp->rawbufp) {
        //marc_dealloc(mp->rawbufp, 503);  //TAG:503
        //mp->rawbufp = NULL;
    //}

    /* Re-initialize all pointers and counts */
    mp->marc_datalen = 0;
    mp->raw_datalen  = 0;
    mp->field_count  = 0;
    mp->sf_count     = 0;
    mp->cur_sf       = 0;
    mp->cur_sf_field = MARC_NO_FIELD;

    /* Create a default record leader.
     * Also initializes current field information.
     */
    if ((stat = marc_add_field (mp, 0)) == 0) {
        stat = marc_add_subfield (mp, 0, (unsigned char *) MARC_LEADER, MARC_LEADER_LEN);
    }

    return stat;

} /* marc_new */
