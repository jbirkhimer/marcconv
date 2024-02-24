/*********************************************************************
* marcdefs.h                                                         *
*                                                                    *
*   PURPOSE                                                          *
*       Internal header file for National Library of Medicine        *
*       MaRC (Machine Readable Cataloging) format record             *
*       management routines.                                         *
*                                                                    *
*       Defines internal structures visible only to the marc         *
*       software library.                                            *
*                                                                    *
*                                               Author: Alan Meyer   *
*********************************************************************/

/* $Id: marcdefs.h,v 1.4 2000/02/01 21:26:55 meyer Exp $
 * $Log: marcdefs.h,v $
 * Revision 1.4  2000/02/01 21:26:55  meyer
 * Changed MARC_DFT_SFDIRSIZE again, this time to 256.
 *
 * Revision 1.3  1999/08/30 23:11:43  meyer
 * Added prototype for internal routine marc_xcheck_data.
 *
 * Revision 1.2  1999/06/14 21:45:36  meyer
 * Changed MARC_DFT_SFDIRSIZE from 40 to 100 to accomodate NLM holdings recs.
 *
 * Revision 1.1  1998/12/20 20:59:08  meyer
 * Initial revision
 *
 */

#ifndef MARCDEFS_H
#define MARCDEFS_H

#include "marc.h"       /* Public definitions   */

/*********************************************************************
*   Constants                                                        *
*********************************************************************/

/*-------------------------------------------------------------------\
|   Default initial sizes of arrays                                  |
|                                                                    |
\-------------------------------------------------------------------*/
#define MARC_DFT_RAWSIZE    0x4000  /* Initial raw buffer size      */
#define MARC_DFT_RAWINC        400  /* Add this much at a time      */
#define MARC_DFT_MARCSIZE   0x4000  /* Initial marc buffer size     */
#define MARC_DFT_DIRSIZE        60  /* Initial entries in field dir */
#define MARC_DFT_DIRINC         50  /* Add this many at a time      */
#define MARC_DFT_SFDIRSIZE     5000  /* Room for this many in 1 field*/
#define MARC_DFT_SFDIRINC       10  /* Add this many at a time      */

/*-------------------------------------------------------------------\
|   Maximum sizes                                                    |
\-------------------------------------------------------------------*/
#define MARC_MAX_MARCDATA  0x1869F  /* Largest legal marc (99999)   */
#define MARC_MAX_FLDCOUNT     1000  /* Unreasonable to have more    */
#define MARC_MAX_FIELDID       999  /* Legal field ids are 0..999   */
#define MARC_MAX_VARLEN       9999  /* Max legal marc field length  */
#define MARC_MAX_SFID          'z'  /* Max legal subfield code      */
#define MARC_MAX_FIXLEN        150  /* Max expected fixed field len */
#ifdef MARC16
#define MARC_MAX_RECLEN      65000  /* Max legal for 16 bits        */
#else
#define MARC_MAX_RECLEN      99999  /* Max legal marc record        */
#endif

/*-------------------------------------------------------------------\
|   marcctl sentinels                                                |
\-------------------------------------------------------------------*/
#define MARC_START_TAG  0x4D615243  /* Sentinel "MaRC" (high endian)*/
#define MARC_END_TAG    0x4352614D  /* Sentinel "CRaM" (high endian)*/

/*-------------------------------------------------------------------\
|   Marc delimiters                                                  |
\-------------------------------------------------------------------*/
#define MARC_SF_DELIM           31  /* ASCII for subfield delimiter */
#define MARC_FIELD_TERM         30  /* ASCII for field terminator   */
#define MARC_REC_TERM           29  /* ASCII for record terminator  */

/*-------------------------------------------------------------------\
|   Default leader, 0's overwritten later                            |
\-------------------------------------------------------------------*/
#define MARC_LEADER      "00000nb   2200000   4500"
#define MARC_LEADER_LEN         24  /* Length of leader string      */
#define MARC_RECLEN_OFF          0  /* Position of record length    */
#define MARC_RECLEN_LEN          5  /* Length of record length      */
#define MARC_BASEADDR_OFF       12  /* Offset to base addr of data  */
#define MARC_BASEADDR_LEN        5  /* Length of base address       */

/*-------------------------------------------------------------------\
|   Marc field directory constants                                   |
\-------------------------------------------------------------------*/
#define MARC_TAG_LEN             3  /* Length of tag / field id     */
#define MARC_FIELDLEN_LEN        4  /* Length of length of field    */
#define MARC_OFFSET_LEN          5  /* Length of offset to field    */

/*-------------------------------------------------------------------\
|   Save/restore field position constants                            |
\-------------------------------------------------------------------*/
#ifdef MARC16
#define MARC_MAX_SAVE_POS       15  /* Number of save_pos calls ok  */
#else
#define MARC_MAX_SAVE_POS       31  /* Number of save_pos calls ok  */
#endif

/*-------------------------------------------------------------------\
|   Miscellaneous constants                                          |
\-------------------------------------------------------------------*/
#define MARC_FIRST_VARFIELD     10  /* Lower ids are "fixed" fields */
#define MARC_FILL_CHAR         '|'  /* Fixed field filler char      */
#define MARC_DIR_SIZE           12  /* Size of one directory entry  */
#define MARC_NO_FIELD          (-1) /* Refers to no field           */
#define MARC_SFTBL_SIZE        128  /* Size subfield collation table*/

/* Default collation sequence for subfields, see marc_set_collate() */
#define MARC_DFT_SF_COLL "aqzb-pr-y1-9"

/*-------------------------------------------------------------------\
|                                                                    |
\-------------------------------------------------------------------*/
/*********************************************************************
*   Macros                                                           *
*********************************************************************/
/* Processing done at the beginning of all public functions.
 * Assigns passed void pointer to pointer with correct cast.
 * Calls marc_xcheck() to verify structure okay.
 * If failed, then returns error to higher level caller.
 */
#define MARC_XCHECK(mp) {         \
    if (marc_xcheck (mp)) return (marc_xcheck(mp)); \
}

/* Determine the field id/tag for the current field */
#define MARC_CUR_TAG(mp) ( (mp->fdirp + mp->cur_field)->tag )

/* Byte at indicator position, i=1 or 2 for indicator 1 or 2 */
#define MARC_INDIC(mp,i) \
    (*(mp->rawbufp + mp->fdirp[mp->cur_field].offset + i - 1))


/*********************************************************************
* flddir                                                             *
*                                                                    *
*   Internal format field directory.                                 *
*                                                                    *
*   Each time we add a field to a marc record, we use an entry       *
*   in an array of these structures to track it.                     *
*   Later, information from here is used to construct the marc       *
*   record directory.                                                *
*********************************************************************/
typedef struct flddir {
    int    tag;             /* Field number, 0=directory            */
    size_t len;             /* Length, including field term         */
    size_t offset;          /* Offset from rawbufp to field start   */
    int    order;           /* Order entered, used in sorting       */
    unsigned pos_bits;      /* For marc_save_pos() positioning      */
    unsigned char startprot;/* First sf in sort protected range     */
    unsigned char endprot;  /* End sf - see marc_get_record()       */
} FLDDIR;


/*********************************************************************
* sfdir                                                              *
*                                                                    *
*   Internal format subfield directory.                              *
*                                                                    *
*   An array of these structures is used to hold a parse of a        *
*   variable length field, tracking the positions of each of the     *
*   subfields.  Only one field is parsed at a time.                  *
*                                                                    *
*   This is for internal use only.  There is no analog for it        *
*   in marc.                                                         *
*********************************************************************/
typedef struct sfdir {
    unsigned char *startp;  /* Pointer to first byte (sf delimiter) */
    unsigned char id;       /* Subfield code                        */
    unsigned char coll;     /* S_sf_collate[subfield_code]          */
    unsigned char order;    /* Order in which sf was entered        */
    size_t        len;      /* Length                               */
} SFDIR;


/*********************************************************************
* marcctl                                                            *
*                                                                    *
*   One of these allocated by marc_init() to control a single        *
*   marc record.  More than one may be allocated if a caller         *
*   wishes to work with more than one marc record concurrently.      *
*                                                                    *
*   A pointer to one of these is required by all marc_ routines.     *
*********************************************************************/
typedef struct marcctl {
    long   start_tag;       /* Sentinel to test for corruption      */
    unsigned char *rawbufp; /* Ptr to "raw" buffer.  Pre-marc here  */
    unsigned char *endrawp; /* Ptr to byte after end of raw buffer  */
    unsigned char *marcbufp;/* Ptr to "marc" buffer, formatted here */
    unsigned char *endmarcp;/* Ptr to byte after end of marc buffer */
    FLDDIR *fdirp;          /* Ptr internal format field directory  */
    SFDIR  *sdirp;          /* Ptr to internal subfield directory   */
    size_t raw_buflen;      /* Length of raw buffer                 */
    size_t raw_datalen;     /* Amount of data in buffer             */
    size_t marc_buflen;     /* Length of marc buffer                */
    size_t marc_datalen;    /* Amount of data in buffer             */
    int    rectype;         /* Bib or Auth (currently unused)       */
    int    read_only;       /* True=no modifications allowed        */
    int    field_max;       /* Length (num entries) in fdirp        */
    int    field_count;     /* Num entries in use in fdirp          */
    int    cur_field;       /* Last field op was on this fdirp entry*/
    int    field_sort;      /* True=Use start/end_prot in sorting   */
    int    start_prot;      /* Don't sort fields between start      */
    int    end_prot;        /*   and end.  Caller specified order   */
    int    sf_max;          /* Length (num entries) in sdirp        */
    int    sf_count;        /* Num entries in use in sdirp          */
    int    cur_sf;          /* Last sf op was on this sdirp entry   */
    int    cur_sf_field;    /* sf directory is for this fielddir    */
    int    sf_sort;         /* True=Use start/end_prot sorting sfs  */
    unsigned last_pos;      /* Last bit used in marking field dirs  */
    unsigned char sf_collate[MARC_SFTBL_SIZE]; /* Sf collation table*/
    long   end_tag;         /* Sentinel                             */
} MARCCTL;


/*********************************************************************
*   Prototypes for internal functions                                *
*********************************************************************/
int marc_xcheck        (MARCP);
int marc_xsfdir        (MARCP);
int marc_xallocate     (void **, void **, size_t *, size_t, size_t);
int marc_xdel_field    (MARCP, int);
int marc_xcompress     (MARCP, size_t, size_t);
int marc_xcheck_data   (int, int, unsigned char *, size_t);
unsigned int marc_xnum (unsigned char *, int);

#define _FILE_OFFSET_BITS 64

#endif /* MARCDEFS_H */
