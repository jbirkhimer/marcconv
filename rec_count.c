/************************************************************************
* marcprn.c                                                             *
*                                                                       *
*   DEFINITION                                                          *
*       Test program for marc package.                                  *
*                                                                       *
*       Reads records from a sequential input file of marc records,     *
*       and dumps the records.                                          *
*                                                                       *
*   COMMAND LINE ARGUMENTS                                              *
*       Name of input file.                                             *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include "marcdefs.h"

#ifdef MARC16
#define IBSIZE  0x2000      /* Input buffer size    */
#else
#define IBSIZE  100000
#endif

#define REC_TERM    29      /* marc rec terminator  */
#define OUT_RECLEN   75
#define DLEN(x) (x > OUT_RECLEN ? OUT_RECLEN : x)

static long S_in_count;


static void copy_rec (MARCP, MARCP, int);
static long compare  (char *, unsigned char *, unsigned char *);
static void fatal    (char *, ...);

int main (int argc, char *argv[])
{
    FILE   *infp;           /* Input file           */
    unsigned char *inbuf,   /* Input buffer         */
           *datap, *datam;  /* Ptr to data          */
    MARCP  m_org;           /* Original input record*/
    int    i,               /* Loop counter         */
           count,
           fd,
           stat;            /* Return code          */
    long   first_dump,      /* First to dump, org 0 */
           count_dump,      /* Num to dump          */
           out_count;       /* Num recs dumped      */
    size_t datalen;         /* Returned data length */

    /* usage */
    if (argc < 2) {
        fprintf (stderr, "usage: marcprn inputmarcfile {count {start}}\n");
        fprintf (stderr, "Dumps marc records human readable to stdout\n");
        fprintf (stderr, "  inputmarcfile = Sequential file of marc recs\n");
        fprintf (stderr, "  count         = Optional num records to dump\n");
        fprintf (stderr, "  start         = Optional first rec, org 0\n");
        exit (1);
    }

#ifdef MCK
    /* Init memory checker */
    mc_startcheck(erf_logfile);
    atexit ((void (*)()) mc_endcheck);
#endif

    /* Open files */
    if ((infp = fopen (argv[1], "rb")) == NULL) {
        perror (argv[1]);
        fatal ("Unable to open input file");
    }

    /* Starting number and count */
    S_in_count =
    out_count  =
    first_dump =
    count_dump = 0L;
    if (argc > 2) {
        count_dump = atol (argv[2]);
        if (argc > 3)
            first_dump = atol (argv[3]);
    }

    /* Create buffer */
    if ((inbuf = malloc (IBSIZE)) == NULL)
        fatal ("Memory\n");

    /* Create marc control structures */
    if ((stat = marc_init (&m_org)) != 0)
        fatal ("Error %d initializing m_org", stat);

    /* Turn off sorting in original record */
    if ((stat = marc_field_sort (m_org, 0)) != 0)
        fatal ("Error %d turning off sort for m_org", stat);
    if ((stat = marc_subfield_sort (m_org, 0)) != 0)
        fatal ("Error %d turning off subfield sort for m_org", stat);

    /* While we successfully read records from input file */
    count = 0;
    while (marc_read_rec (infp, inbuf, IBSIZE) != EOF) {

        ++count;
    }

    printf("\n\nNumber of records: %d\n\n\n", count);


    return 0;

}


/************************************************************************
* fatal ()                                                              *
*                                                                       *
*   DEFINITION                                                          *
*       Print message and exit.                                         *
*                                                                       *
*   PASS                                                                *
*       Variable printf args.                                           *
*                                                                       *
*   RETURN                                                              *
*       Void.  No return.                                               *
************************************************************************/

static void fatal (
    char *fmt,          /* Printf format for message    */
    ...                 /* Additional vsprintf args     */
) {
    va_list args;       /* Ptr to first variable arg.   */
    char msgbuf[256];   /* Assemble message here        */


    /* Initialize for variable arguments */
    va_start (args, fmt);

    /* Create a message string */
    vsprintf (msgbuf, fmt, args);
    va_end (args);

    /* Output current record number */
    printf ("Input record: %ld\n", S_in_count);

    /* Message */
    printf ("%s\n", msgbuf);

    /* Fatal error */
    exit (1);

} /* fatal */
