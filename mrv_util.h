/*********************************************************************
* mrv_util.h                                                         *
*                                                                    *
*   PURPOSE                                                          *
*       Header file with prototypes for utilities                    *
*                                                                    *
*                                               Author: Mark Vereb   *
*********************************************************************/

#ifndef MRVUTIL_H
#define MRVUTIL_H

#include <string.h>
#include "marcconv.h"

#define CALC_LENGTH -1

#define NO_COLLAPSE_SPACES 0  /* for allocate_n_ncopy */
#define COLLAPSE_SPACES 1     /* for allocate_n_ncopy */

#define NoTRIM 0              /* for collapse_spaces_in_place */
#define LTRIM 1               /* for collapse_spaces_in_place */
#define RTRIM 2               /* for collapse_spaces_in_place */
#define FTRIM 3               /* for collapse_spaces_in_place */

#define WORD_FORWARD 1        /* for find_word */
#define WORD_BACKWARD -1      /* for find_word */

#define NO_EXACT_MATCH 0      /* for decode_value */
#define EXACT_MATCH 1         /* for decode_value */

#define NO_NORMALIZE_DATA 0   /* for load_decode_tbl & decode_value */
#define NORMALIZE_DATA 1      /* for load_decode_tbl & decode_value */
#define NO_NULL_DECODE 0      /* for load_decode_tbl & decode_value */
#define ALLOW_NULL_DECODE 1   /* for load_decode_tbl & decode_value */

#define DLM_REC_SEPARATOR "@" /* parsing decode tables */
#define DLM_TAG "~"           /* parsing decode tables */
#define DLM_INDICATOR "@"     /* parsing decode tables */
#define DLM_SUBFIELD "$"      /* parsing decode tables */

#define NO_PAREN_MSG 0                /* matching_parens */
#define PAREN_MSG 1                   /* matching_parens */

#define REMOVE_NONE 0                 /* matching_parens */
#define REMOVE_UNMATCHED_INITIAL 1    /* matching_parens */
#define REMOVE_UNMATCHED_TERMINAL 2   /* matching_parens */
#define REMOVE_UNMATCHED_ENDS 3       /* matching_parens */
#define REMOVE_UNMATCHED_ANY 4        /* matching_parens */

#define MAX_PARENS 255


typedef struct decode_element {
    char   *hstar_data; /* Prt to HSTAR data (to be searched)          */
    char   *marc_data;  /* Ptr to decoded marc data for output         */
} DECODE_TBL;


typedef struct string_count_array {
    char  *value;      /* Prt to string data               */
    int    count;      /* Count of string data occurrences */
} STRING_COUNT_ARRAY;


typedef struct matching_paren_array {
  int     begin_paren_loc; /* language code */
  int     end_paren_loc;   /* length of article */
} MATCHING_PAREN_ARRAY;

typedef struct matching_paren_struct {
  int     entries;
  MATCHING_PAREN_ARRAY paren_array[MAX_PARENS];
} MATCHING_PAREN_STRUCT;

extern MATCHING_PAREN_STRUCT forward_parens;
extern MATCHING_PAREN_STRUCT backward_parens;
extern MATCHING_PAREN_STRUCT inmost_parens;



void    add_ending_punc(char *outp, 
			size_t *out_len, 
			char *append_punc, 
			char *ending_punc);

int     allocate_n_copy(char **dest, 
			char *src, 
			char *fname);

int     allocate_n_ncopy(char **dest, 
			 size_t tsrc_len,
			 int extraspace,
			 char *src, 
			 int compress_spaces,
			 char *fname);

void    collapse_spaces(char **outp, char *srcp, size_t src_len);

void    collapse_spaces_in_place(char **srcp, int trim_instructions);

int     decode_value(DECODE_TBL *decode_table, 
		     char *srcp, 
		     size_t src_len,
		     DECODE_TBL **cnp,
		     int exact_match,
		     int normalize_data);

void    dumpdecodetbl(DECODE_TBL *decode_table, int max_entries);

void    dumpstringarray(char *fname,
			char *string_array[], 
			int max_entries);

int     find_array_entry(char *string_array[],
			 char *srcp, 
			 size_t src_len,
			 char **entryp,
			 int exact_match,
			 int normalize_data);

void    find_statement(char **stmt_loc,
		       int   *stmt_len,
		       char *string_array[], 
		       char *srcp, 
		       size_t src_len,
		       int  normalize_data);

int     find_word(char *srcp, size_t src_len,
		  char **tmpp,size_t *tmp_len,
		  char *ignore_chars,
		  int direction);

int     get_enclosed_string(char *srcp, size_t src_len, 
			    char **newp, size_t *new_len,
			    char *begin_delim, char *end_delim);

void    load_decode_tbl (char *fname, 
			 DECODE_TBL *decode_table,
			 int max_entries,
			 int normalize_data,
			 int null_decode_allowed);

void    load_and_collapse_decode_tbl (char *fname, 
				      DECODE_TBL *decode_table,
				      int max_entries,
				      int normalize_data,
				      int null_decode_allowed);
     
void    load_string_array (char *fname, 
			   char *string_array[],
			   int max_entries,
			   int normalize_data);

void    load_validation_tbl (char *fname, 
			     DECODE_TBL *decode_table,
			     char *delim,
			     int max_entries,
			     int normalize_data);

int     matching_parens(char **tsrcp,
			size_t *tsrc_len,
			char *begin_paren,
			char *end_paren,
			int  display_msg,
			int  removal_action);

void    print_paren_array(char *array_desc, 
			  MATCHING_PAREN_STRUCT paren_struct);

char   *normalized(char *srcp, size_t src_len);

CM_STAT parse_decode_tbl(CM_PROC_PARMS *pp,
			 char *tag, 
			 DECODE_TBL *cnp);

char   *remove_parens_from_begin(char *outp,
				 size_t *out_len,
				 char *begin_paren,
				 char *end_paren);

void    remove_parens_from_end(char *outp,
			       size_t *out_len,
			       char *begin_paren,
			       char *end_paren);

void    remove_substr(char **srcp, char *beg_point, char *end_point);

void    skip_punc(char **srcp);

void    skip_punc_backwards(char *ostr, char **srcp);

void    skip_punc_no_paren(char **srcp, char *stopchars);

void    skip_punc_backwards_no_paren(char *ostr, char **srcp, char *stopchars);

void    skip_spaces(char **srcp);

int     stricmp(char *s, char *t);
int     strincmp(char *s, char *t, int i);

int     strcmpnrm(char *s, char *t);
int     strncmpnrm(char *s, int i, char *t, int j);

int     strwrd(char *srcp, size_t src_len, char *wrd);

void    write_fld_sfld(CM_PROC_PARMS *pp,
		       char *tmpp,
		       char *delimiter,
		       char *field,
		       char *subfield,
		       int *first_time);








#endif /* HSTARPROCLIST_H */
