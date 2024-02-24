/************************************************************************
* mrv_util.c                                                            *
*                                                                       *
*   DEFINITION                                                          *
*       Utility functions & procedures for MaRC processing...           *
*                                                                       *
*                                               Author: Mark Vereb      *
************************************************************************/

#include "mrv_util.h"
#include <ctype.h>

MATCHING_PAREN_STRUCT forward_parens;
MATCHING_PAREN_STRUCT backward_parens;
MATCHING_PAREN_STRUCT inmost_parens;

void *marc_alloc(int, int);
void marc_dealloc(void *, int);

/************************************************************************
* add_ending_punc                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       appends append_punc to outp if it doesn't already end in        *
*       ending_punc                                                     *
*                                                                       *
*   PARAMETERS                                                          *
*       outp        ptr to data string                                  *
*       out_len     length of data                                      *
*       dflt_punc   character to be appended                            *
*       other_punc  list of characters that count as ending punc.       *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void add_ending_punc(char *outp,
		     size_t *out_len,
		     char *dflt_punc,
		     char *other_punc)
{
  char          *lastchar,    /* Ptr to temp data                    */
                *tmpp;        /* Ptr to temp data                    */
  int           have_it;      /* flag to append punctuation          */


  tmpp = outp + *out_len - 1;
  have_it = 0;

  /*=========================================================================*/
  /* Loop through other_punc to see if last character of outp matches any    */
  /*=========================================================================*/
  for(lastchar=other_punc; (*lastchar) && (!have_it); lastchar++) {
    have_it = (*tmpp==*lastchar);
  }

  /*=========================================================================*/
  /* If not, append default punctuation to outp & set out_len                */
  /*=========================================================================*/
  if (!have_it) {
    strcat(outp, dflt_punc);
    *out_len = strlen(outp);
  }

} /* add_ending_punc */



/************************************************************************
* allocate_n_copy()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Allocates memory and copies src to dest                         *
*                                                                       *
*   PASS                                                                *
*       Ptr to dest                                                     *
*       Ptr to src                                                      *
*       Name of file (in case of error)       .                         *
*                                                                       *
*   RETURN                                                              *
*       0 success all else fatal                                        *
************************************************************************/

int allocate_n_copy(char **dest, char *src, char *fname)
{
  /*=========================================================================*/
  /* Allocate memory based on size of src...                                 */
  /*=========================================================================*/

#ifdef DEBUG
  if ((*dest = (char *) marc_alloc(strlen(src)+1, 900)) == NULL) { //TAG:900
      cm_error(CM_ERROR, "Error allocating memory for %s processing. " "Record not processed", fname);
      return CM_STAT_KILL_RECORD;
  }
#else
  if ((*dest = (char *)malloc(strlen(src)+1)) == NULL) {
      cm_error(CM_ERROR, "Error allocating memory for %s processing. " "Record not processed", fname);
      return CM_STAT_KILL_RECORD;
  }
#endif

  /*=========================================================================*/
  /* ...and then copy src over to dest                                       */
  /*=========================================================================*/
  memset(*dest, '\0', strlen(src)+1);
  strcpy(*dest, src);

  return 0;

} /* allocate_n_copy */



/************************************************************************
* allocate_n_ncopy()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Allocates memory and copies n bytes of src to dest              *
*                                                                       *
*   PASS                                                                *
*       Ptr to dest                                                     *
*       Ptr to src                                                      *
*       Number of bytes to copy                                         *
*       Name of file (in case of error)       .                         *
*                                                                       *
*   RETURN                                                              *
*       0 success all else fatal                                        *
************************************************************************/

int allocate_n_ncopy(char **dest, size_t tsrc_len, int extraspace,
		     char *src, int compress_spaces, char *fname)
{
  size_t src_len;

  if(tsrc_len==CALC_LENGTH)
    src_len = strlen(src);
  else
    src_len = tsrc_len;

  /*=========================================================================*/
  /* Allocate memory based on size of src...                                 */
  /*=========================================================================*/

#ifdef DEBUG
  if ((*dest = (char *) marc_alloc(src_len+extraspace+1, 901)) == NULL) {  //TAG:901
      cm_error(CM_ERROR, "Error allocating memory for %s processing. " "Record not processed", fname);
      return CM_STAT_KILL_RECORD;
  }
#else
  if ((*dest = (char *)malloc(src_len+extraspace+1)) == NULL) {
      cm_error(CM_ERROR, "Error allocating memory for %s processing. " "Record not processed", fname);
      return CM_STAT_KILL_RECORD;
  }
#endif

  memset(*dest, '\0', src_len+extraspace+1);

  /*=========================================================================*/
  /* ...and then copy src over to dest                                       */
  /*=========================================================================*/
  if(compress_spaces == NO_COLLAPSE_SPACES)
    strncpy(*dest, src, src_len);
  else
    collapse_spaces(dest, src, src_len);

  return 0;

} /* allocate_n_ncopy */



/************************************************************************
* collapse_spaces()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Copies srcp to (already allocated) outp collapsing multiple " "s*
*                                                                       *
*   PASS                                                                *
*       Ptr to src                                                      *
*       Ptr to length of src                                            *
*       Ptr to dest                                                     *
*                                                                       *
*   RETURN                                                              *
*       0 success all else fatal                                        *
************************************************************************/

void collapse_spaces(char **outp, char *srcp, size_t src_len)
{
  char          *tmpin;         /* Ptr to temp data                    */
  char          *tmpout;        /* Ptr to temp data                    */

  tmpout = *outp;
  for(tmpin=srcp; tmpin<srcp+src_len; tmpin++)
    if (strncmp(tmpin, "  ", 2) != 0)
      *tmpout++=*tmpin;

} /* collapse_spaces */



/************************************************************************
* collapse_spaces_in_place()                                            *
*                                                                       *
*   DEFINITION                                                          *
*       Collapses multiple spaces in srcp...                            *
*                                                                       *
*   PASS                                                                *
*       Ptr to srcp                                                     *
*       integer specifying trim instructions                            *
*                                                                       *
*   RETURN                                                              *
*       0 success all else fatal                                        *
************************************************************************/

void collapse_spaces_in_place(char **srcp, int trim_instructions)
{
  char          *tmpin;         /* Ptr to temp data                    */
  char          *tmpout;        /* Ptr to temp data                    */

  tmpin=tmpout=*srcp;

  if((trim_instructions==LTRIM) ||
     (trim_instructions==FTRIM))
     for(;*tmpin==' ';tmpin++);

  for(; *tmpin; tmpin++)
    if (strncmp(tmpin, "  ", 2) != 0)
      *tmpout++=*tmpin;

  *tmpout = '\0';

  if((trim_instructions==RTRIM) ||
     (trim_instructions==FTRIM))
     for(tmpout--;*tmpout==' ';tmpout--)
       *tmpout = '\0';


} /* collapse_spaces_in_place */



/************************************************************************
* decode_value                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Determines whether src string matches an entry in decode table  *
*                                                                       *
*   PASS                                                                *
*       decode table                                                    *
*       source string                                                   *
*       ptr to table entry                                              *
*                                                                       *
*   RETURN                                                              *
*       0 couldn't find source in table                                 *
*       non-zero found source in table                                  *
************************************************************************/

int decode_value(DECODE_TBL *decode_table,
		 char *srcp,
		 size_t src_len,
		 DECODE_TBL **cnp,
		 int exact_match,
		 int normalize_data)
{
  DECODE_TBL *tmpp;
  char       *nrmp;
  int        datalen;

  if(normalize_data == NORMALIZE_DATA) {
    if ((nrmp = normalized(srcp, src_len)) == NULL)
      return 0;
    datalen=strlen(nrmp);
  }
  else {
    nrmp=srcp;
    datalen=src_len;
  }

  /*=========================================================================*/
  /* Move through the decode table, searching for nrmp = hstar_data          */
  /*=========================================================================*/
  for(tmpp=decode_table; tmpp->hstar_data; tmpp++) {

      /*=======================================================================*/
      /* Look for an exact match if that's what's indicated...                 */
      /*=======================================================================*/
      if (exact_match == EXACT_MATCH) {
          if ((strncmp(nrmp, tmpp->hstar_data, datalen) == 0) && (datalen==strlen(tmpp->hstar_data))) {
	          /*===================================================================*/
	          /* If we find it, set pointer to current entry and return 1          */
	          /*===================================================================*/
	          *cnp = tmpp;
	          if (normalize_data == NORMALIZE_DATA) {
#ifdef DEBUG
                  marc_dealloc(nrmp, 902); //TAG:902
#else
                  free(nrmp);
#endif
              }
	          return 1;
           }
      } else {
          /*=======================================================================*/
          /* Otherwise, just look for a prefix match...                            */
          /*=======================================================================*/
          if (strncmp(nrmp, tmpp->hstar_data, strlen(tmpp->hstar_data)) == 0) {
	          /*===================================================================*/
	          /* If we find it, set pointer to current entry and return 1          */
	          /*===================================================================*/
	          *cnp = tmpp;
	          if(normalize_data == NORMALIZE_DATA) {
#ifdef DEBUG
                  marc_dealloc(nrmp, 903); //TAG:903
#else
                  free(nrmp);
#endif
              }
	          return 1;
           }
      }
  }
  /*=========================================================================*/
  /* If we don't, return 0                                                   */
  /*=========================================================================*/
  if(normalize_data == NORMALIZE_DATA) {
#ifdef DEBUG
      marc_dealloc(nrmp, 904); //TAG:904
#else
      free(nrmp);
#endif
  }

  return 0;

} /* decode_value */



/************************************************************************
* dumpdecodetbl()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Allocates memory and copies src to dest                         *
*                                                                       *
*   PASS                                                                *
*       decode table                                                    *
*       number of entries                                               *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void dumpdecodetbl(DECODE_TBL *decode_table, int max_entries)
{
  DECODE_TBL *table_entry;  /* Pointer into field control table     */
  int i;

  /*=========================================================================*/
  /* Print each table entry                                                  */
  /*=========================================================================*/
  table_entry = decode_table;
  for(i=0; i<max_entries; i++) {
    if (table_entry->hstar_data)
      printf ("ENTRY #%d\nCORP NAME: %s\nMARC DATA: %s\n\n",
	      i,
	      table_entry->hstar_data,
	      table_entry->marc_data);
    ++table_entry;
  }

} /* dumpdecodetbl */



/************************************************************************
* dumpstringarray()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Allocates memory and copies src to dest                         *
*                                                                       *
*   PASS                                                                *
*       decode table                                                    *
*       number of entries                                               *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void dumpstringarray(char *fname, char *string_array[], int max_entries)
{
  int i;

  /*=========================================================================*/
  /* Print each table entry                                                  */
  /*=========================================================================*/
  printf("CONTENTS OF %s\n", fname);
  for(i=0; i<max_entries; i++) {
    if (string_array[i])
      printf ("ENTRY #%d\tENTRY CONTENT: '%s'\n",
	      i,
	      string_array[i]);
  }
  printf("\n");

} /* dumpstringarray */


/************************************************************************
* find_array_entry                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Determines whether src string matches an entry in string array  *
*                                                                       *
*   PASS                                                                *
*       string array                                                    *
*       source string                                                   *
*       ptr to table entry                                              *
*                                                                       *
*   RETURN                                                              *
*       0 couldn't find source in table                                 *
*       non-zero found source in table                                  *
************************************************************************/

int find_array_entry(char *string_array[],
		     char *srcp,
		     size_t src_len,
		     char **entryp,
		     int exact_match,
		     int normalize_data)
{
  int   occ;
  char  *nrmp;
  int   datalen;

  if(normalize_data == NORMALIZE_DATA) {
    if ((nrmp = normalized(srcp, src_len)) == NULL)
      return 0;
    datalen=strlen(nrmp);
  }
  else {
    nrmp=srcp;
    datalen=src_len;
  }

  /*=========================================================================*/
  /* Move through the string array, searching for nrmp                       */
  /*=========================================================================*/
  for(occ=0; string_array[occ]; occ++) {

    /*=======================================================================*/
    /* Look for an exact match if that's what's indicated...                 */
    /*=======================================================================*/
    if (exact_match == EXACT_MATCH) {
      if ((strncmp(nrmp, string_array[occ], datalen) == 0) &&
	  (datalen==strlen(string_array[occ]))) {
	/*===================================================================*/
	/* If we find it, set pointer to current entry and return 1          */
	/*===================================================================*/
	*entryp = string_array[occ];
	if(normalize_data == NORMALIZE_DATA) {
#ifdef DEBUG
      marc_dealloc(nrmp, 905); //TAG:905
#else
	  free(nrmp);
#endif
    }
	return 1;
      }
    }
    else {
    /*=======================================================================*/
    /* Otherwise, just look for a prefix match...                            */
    /*=======================================================================*/
      if (strncmp(nrmp, string_array[occ], strlen(string_array[occ])) == 0) {
	/*===================================================================*/
	/* If we find it, set pointer to current entry and return 1          */
	/*===================================================================*/
	*entryp = string_array[occ];
	if(normalize_data == NORMALIZE_DATA) {
#ifdef DEBUG
      marc_dealloc(nrmp, 906); //TAG:906
#else
	  free(nrmp);
#endif
    }
	return 1;
      }
    }
  }
  /*=========================================================================*/
  /* If we don't, return 0                                                   */
  /*=========================================================================*/
  if(normalize_data == NORMALIZE_DATA) {
#ifdef DEBUG
      marc_dealloc(nrmp, 907); //TAG:907
#else
      free(nrmp);
#endif
  }

  return 0;

} /* find_array_entry */



/************************************************************************
* find_statement                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Determines whether entry in string array is found in srcp and,  *
*       if so, sets stmt_loc to the location where it occurs            *
*                                                                       *
*   PASS                                                                *
*       statement location                                              *
*       statement length                                                *
*       decode table                                                    *
*       source string                                                   *
*       ptr to table entry                                              *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void find_statement(char **stmt_loc,
		    int   *stmt_len,
		    char *string_array[],
		    char *srcp,
		    size_t src_len,
		    int normalize_data)
{
  char *tmpp;
  char *tmps;
  char *nrmp;
  int  datalen;
  int  i;

  if(normalize_data == NORMALIZE_DATA) {
    if ((nrmp=normalized(srcp, src_len)) == NULL)
      return;
    datalen=strlen(nrmp);
  }
  else {
    nrmp=srcp;
    datalen=src_len;
  }


  *stmt_loc = NULL;
  *stmt_len = 0;
  tmps = srcp;

  /*=========================================================================*/
  /* Walk through the nrmp, checking for each entry in string_array          */
  /*=========================================================================*/
  for(tmpp=nrmp; tmpp<nrmp+datalen; tmpp++) {

    /*=======================================================================*/
    /* We also need to walk through the original string so we know where we  */
    /* are if/when we find a match (if we normalized, we need to skip over   */
    /* characters that were dropped during normalization process)...         */
    /*=======================================================================*/
    if(tmpp!=nrmp)
      for(tmps++;(tolower(*tmps)!=tolower(*tmpp));tmps++);
    else
      if(tolower(*tmps)!=tolower(*tmpp))
	for(tmps++;(tolower(*tmps)!=tolower(*tmpp));tmps++);


    /*=======================================================================*/
    /* Now, check to see if any of the string_array elements match our string*/
    /*=======================================================================*/
    for(i=0;string_array[i];i++)
      if(strncmp(tmpp,string_array[i],strlen(string_array[i])) == 0) {
	/*===================================================================*/
	/* If so, position stmt_loc and calculate length of matching string  */
	/*===================================================================*/
	*stmt_loc = tmps;
	/*===================================================================*/
	/* If we normalized, we may need to skip over additional characters  */
	/*===================================================================*/
	if(normalize_data==NORMALIZE_DATA) {
	  *stmt_len = 0;
	  for(tmpp=string_array[i];*tmpp;tmpp++) {
	    *stmt_len=*stmt_len+1;
	    if(tmpp!=string_array[i])
	      for(tmps++;(tolower(*tmps)!=tolower(*tmpp)); tmps++)
		*stmt_len=*stmt_len+1;
	  }
	}
	/*===================================================================*/
	/* Otherwise, it's just as long as string we matched on...           */
	/*===================================================================*/
	else
	  *stmt_len = strlen(string_array[i]);

	return;
      }
  }

  /*=========================================================================*/
  /* When done, return to caller...                                          */
  /*=========================================================================*/
  return;

} /* find_statement */



/************************************************************************
* find_word                                                             *
*                                                                       *
*   DEFINITION                                                          *
*       identifies a word in a specified string                         *
*                                                                       *
*   find_word(srcp,src_len,&tmpp,&srn_len," ,.",WORD_BACKWARD)          *
*                                                                       *
*   PASS                                                                *
*       ptr to string in which word is to be found                      *
*       ptr to beginning of word (if not null, must be in string        *
*           and search starts from that point)                          *
*       ptr to size (so you know how big the word is)                   *
*       direction (WORD_BACKWARD or WORD_FORWARD)                       *
*                                                                       *
*   RETURN                                                              *
*       1 if able to identify word                                      *
*       0 if unable to identify word                                    *
************************************************************************/
int ignored(char *tmpp, char *ignore_chars)
{
  int rc;

  rc = (strchr(ignore_chars, *tmpp)!=NULL);
  return rc;
}

int find_word(char *srcp,  size_t src_len,
	      char **stmpp,size_t *tmp_len,
	      char *ignore_chars,
	      int direction)
{
  char          *tmpp,        /* Ptr to temp data                    */
                *trmp;        /* Ptr to temp data                    */
  int           initscan;     /* flag for initial positioning        */

  tmpp=*stmpp;

  if(src_len<1) {
    *tmp_len=0;
    return 0;
  }

  /*=========================================================================*/
  /* Special processing if string is only one character...                   */
  /*=========================================================================*/
  if(src_len==1){
    /*=======================================================================*/
    /* If tmpp is NULL, that means we're looking at first call, we have it   */
    /* as long as the character isn't supposed to be ignored...              */
    /*=======================================================================*/
    if((!tmpp) && (!ignored(srcp,ignore_chars))) {
	*stmpp=srcp;
	*tmp_len=1;
	return 1;
    }
    /*=======================================================================*/
    /* ...otherwise, no word found in the string.                            */
    /*=======================================================================*/
    else {
      *tmp_len=0;
      return 0;
    }
  }

  /*=========================================================================*/
  /* Depending on the direction, we use different operations                 */
  /*=========================================================================*/


  /*=========================================================================*/
  /* If going FORWARD...                                                     */
  /*=========================================================================*/
  if(direction==WORD_FORWARD) {
    trmp=srcp;

    /*=======================================================================*/
    /* If tmpp is already established (and it's in the current string),      */
    /* we're getting the next word, so we first have to get past this one... */
    /*=======================================================================*/
    if(tmpp) {
      if((tmpp>=srcp)&&(tmpp<srcp+src_len)) {
	for(trmp=tmpp+1;
	    (*trmp)&&(trmp<srcp+src_len)&&(!ignored(trmp,ignore_chars));
	    trmp++);

	/*===================================================================*/
	/* If we're at the end of the string, we don't have a next word      */
	/*===================================================================*/
	if(trmp>=srcp+src_len) {
	  *tmp_len = 0;
	  return 0;
	}
      }
    }

    /*=======================================================================*/
    /* Now we have to skip over characters to ignore to identify the start   */
    /* of the next word (or if we haven't established ourselves yet, to      */
    /* ignore characters at the beginning of srcp)...                        */
    /*=======================================================================*/
    for(tmpp=trmp;
	(*tmpp)&&(tmpp<srcp+src_len)&&(ignored(tmpp,ignore_chars));
	tmpp++);

    /*=======================================================================*/
    /* Just to be safe, check to make sure we're not at the end...           */
    /*=======================================================================*/
    if(tmpp>=srcp+src_len) {
      *tmp_len = 0;
      return 0;
    }

    /*=======================================================================*/
    /* Now, we need to find the end of the word...                           */
    /*=======================================================================*/
    for(trmp=tmpp+1;
	(*trmp)&&(trmp<srcp+src_len)&&(!ignored(trmp,ignore_chars));
	trmp++);

    *stmpp = tmpp;
    *tmp_len = trmp-tmpp;
    return 1;
  }

  /*=========================================================================*/
  /* If going BACKWARD...                                                    */
  /*=========================================================================*/
  if(direction==WORD_BACKWARD) {

    /*=======================================================================*/
    /* If tmpp is NULL, we need to establish it at the end...                */
    /*=======================================================================*/
    initscan = 1;
    if(!tmpp) {
      tmpp=srcp+src_len-1;
      trmp=tmpp;
      initscan = ignored(tmpp,ignore_chars);
    }
    /*=======================================================================*/
    /* Else if we're already at the beginning, there's nothing to do...      */
    /*=======================================================================*/
    else
      if(tmpp==srcp) {
	*tmp_len = 0;
	return 0;
      }

    /*=======================================================================*/
    /* We're getting the previous word, so back up until we get to a         */
    /* character we're not supposed to ignore...                             */
    /*=======================================================================*/
    if(initscan)
      for(trmp=tmpp-1;(trmp>srcp)&&(ignored(trmp,ignore_chars));trmp--)
	trmp=trmp;

    /*=======================================================================*/
    /* If we're at the start of the string, we don't have a previous word    */
    /*=======================================================================*/
    if(trmp==srcp) {
      *tmp_len = 0;
      return 0;
    }

    /*=======================================================================*/
    /* Now we work backwards until we get to something to ignore...          */
    /*=======================================================================*/
    for(tmpp=trmp;(tmpp>srcp)&&(!ignored(tmpp,ignore_chars));tmpp--)
      tmpp=tmpp;

    if(ignored(tmpp,ignore_chars))
      tmpp++;
    *stmpp = tmpp;
    *tmp_len = trmp-tmpp+1;
    return 1;
  }
  return 0;

} /* find_word */



/************************************************************************
* get_enclosed_string                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Searches srcp looking for beginning and ending delimiters.      *
*       If found, returns contents in newp (establishing length in      *
*       new_len).                                                       *
*                                                                       *
*   PARAMETERS                                                          *
*       srcp        string to be searched                               *
*       src_len     length of data                                      *
*       newp        ptr to enclosed string                              *
*       new_len     length of enclosed string                           *
*       begin_delim beginning delimiter                                 *
*       end_delim   ending delimiter                                    *
*                                                                       *
*   RETURN                                                              *
*       non-zero if enclosed string identified                          *
*       0        if not                                                 *
************************************************************************/

int get_enclosed_string(char *srcp, size_t src_len,
			char **newp, size_t *new_len,
			char *begin_delim, char *end_delim)
{
  char          *tmpp;        /* Ptr to temp data                    */
  char          *tmp2;        /* Ptr to temp data                    */
  int            rc;

  *newp = NULL;
  *new_len = 0;
  rc = 0;
  /*=========================================================================*/
  /* First, we need to see if we have a beginning delimiter...               */
  /*=========================================================================*/
  for(tmpp=srcp;
      ((strncmp(tmpp,begin_delim,strlen(begin_delim))!=0)&&
       (tmpp<srcp+src_len));
      tmpp++);

  /*=========================================================================*/
  /* If we have the begin_delim, check to see if we have an end_delim...     */
  /*=========================================================================*/
  if(strncmp(tmpp,begin_delim,strlen(begin_delim))==0) {
    tmp2=tmpp;
    for(tmp2++;
	((strncmp(tmp2,end_delim,strlen(end_delim))!=0)&&
	 (tmp2<srcp+src_len));
	tmp2++);

    if(strncmp(tmp2,end_delim,strlen(end_delim))==0) {
      *newp=tmpp+1;
      *new_len = tmp2-tmpp-1;
      rc = 1;
    }
  }

  return rc;
} /* get_enclosed_string */



/************************************************************************
* load_decode_tbl()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Load a decode table. Decoded values are to be placed            *
*       directly into output record.                                    *
*                                                                       *
*       Example input file line format is:                              *
*           string@@1 @2 $avalue-for-subfield-a$bvalue-for-subfield-b   *
*                                                                       *
*       entry consists of value to search against (terminated with @)   *
*       following first at will be the 'decode value' which must be     *
*       parsed...                                                       *
*             @ specifies next character is indicator followed by value *
*               (e.g., @10 means indicator 1 is 0                       *
*                      @22 means indicator 2 is 2)                      *
*             $ specifies next character is subfield followed by value  *
*               up to the next @ or $                                   *
*                                                                       *
*   PASS                                                                *
*       Name of file to be loaded.                                      *
*       pointer to array into which file is to be loaded.               *
*       maximum number of entries in the array.                         *
*                                                                       *
*   RETURN                                                              *
*       Void.  All errors are fatal.                                    *
************************************************************************/

void load_decode_tbl (char *fname,
		      DECODE_TBL *decode_table,
		      int max_entries,
		      int normalize_data,
		      int null_decode_allowed)
{
  DECODE_TBL *cnp;        /* Pointer into field control table     */
  FILE    *fp;            /* Input file                           */
  char    *textp,         /* Ptr to line in control table         */
          *c_cnp,         /* Pointer to field id in ctl table line*/
          *c_mdp,         /* Ptr to subfield id                   */
          *tmpp;          /* Ptr to group id string               */
  int     entry_cnt,      /* Count of entries in file             */
          rc;

  cnp = decode_table;
  entry_cnt = 0;

  /*=========================================================================*/
  /* Open decode table file...                                               */
  /*=========================================================================*/
  open_ctl_file (fname, &fp);

  /*=========================================================================*/
  /* Read each line of the file...                                           */
  /*=========================================================================*/
  while (get_ctl_line (fp, &textp)) {

    /*=======================================================================*/
    /* Make sure we don't have too many                                      */
    /*=======================================================================*/
    if (entry_cnt >= max_entries)
      cm_error (CM_FATAL, "Decode table full from file %s (Max=%d).",
		fname, max_entries);

    /*=======================================================================*/
    /* Then break it apart at the first record separator ("@") sign          */
    /*=======================================================================*/
    c_cnp = strtok(textp, DLM_REC_SEPARATOR);
    if (!c_cnp) {
      cm_error (CM_ERROR,
		"Missing delimiter or data from entry number %d. "
		"Expecting decode_value@marc_data", entry_cnt);
      cm_error(CM_CONTINUE,"Entry='%s'", textp);
      continue;
    }
    else {
      c_mdp = c_cnp + strlen(c_cnp) + 1;
      if ((!*c_mdp)&&(null_decode_allowed==NO_NULL_DECODE)) {
	cm_error (CM_ERROR,
		  "Missing marc data from entry number %d.  "
		  "Expecting decode_value@marc_data", entry_cnt);
	cm_error(CM_CONTINUE,"Entry='%s'", textp);
	continue;
      }
    }

    /*=======================================================================*/
    /* Allocate space for each part and place them into decode element       */
    /*=======================================================================*/
    if(normalize_data == NORMALIZE_DATA) {
      if ((tmpp=normalized(c_cnp, strlen(c_cnp))) != NULL)
	cnp->hstar_data = tmpp;
    }
    else {
      if ((rc=allocate_n_copy(&tmpp, c_cnp, fname)) == 0)
	cnp->hstar_data = tmpp;
    }

    if ((rc=allocate_n_copy(&tmpp, c_mdp, fname)) == 0)
      cnp->marc_data = tmpp;

    /*=======================================================================*/
    /* Advance to next element in the array and increment line count         */
    /*=======================================================================*/
    ++cnp;
    ++entry_cnt;
  }

  /*=========================================================================*/
  /* "terminate" array elements                                              */
  /*=========================================================================*/
  if (entry_cnt < max_entries) {
    cnp->hstar_data = '\0';
    cnp->marc_data = '\0';
  }

  /*dumpdecodetbl(decode_table, entry_cnt);*/

  /*=========================================================================*/
  /* and close the decode table file                                         */
  /*=========================================================================*/
  close_ctl_file (&fp);

} /* load_decode_tbl */



/************************************************************************
* load_and_collapse_decode_tbl()                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Load a decode table and then collapse multiple spaces in it     *
*                                                                       *
*   PASS                                                                *
*       Name of file to be loaded.                                      *
*       pointer to array into which file is to be loaded.               *
*       maximum number of entries in the array.                         *
*                                                                       *
*   RETURN                                                              *
*       Void.  All errors are fatal.                                    *
************************************************************************/

void load_and_collapse_decode_tbl (char *fname,
				   DECODE_TBL *decode_table,
				   int max_entries,
				   int normalize_data,
				   int null_decode_allowed)
{
  DECODE_TBL *cnp;        /* Pointer into field control table     */

  load_decode_tbl(fname,
		  decode_table,
		  max_entries,
		  normalize_data,
		  null_decode_allowed);

  for(cnp=decode_table; cnp->hstar_data != '\0'; cnp++) {
    collapse_spaces_in_place(&cnp->hstar_data,FTRIM);
    collapse_spaces_in_place(&cnp->marc_data,FTRIM);
  }

} /* load_and_collapse_decode_tbl */


/************************************************************************
* load_string_array()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Load a string array table.                                      *
*                                                                       *
*   PASS                                                                *
*       Name of file to be loaded.                                      *
*       pointer to array into which file is to be loaded.               *
*       maximum number of entries in the array.                         *
*                                                                       *
*   RETURN                                                              *
*       Void.  All errors are fatal.                                    *
************************************************************************/

void load_string_array (char *fname,
			char *string_array[],
			int max_entries,
			int normalize_data)
{
  FILE    *fp;            /* Input file                           */
  char    *textp,         /* Ptr to line in control table         */
          *tmp2,          /* Ptr to subfield id                   */
          *tmpp;          /* Ptr to group id string               */
  int     entry_cnt,      /* Count of entries in file             */
          rc;

  entry_cnt = 0;

  /*=========================================================================*/
  /* Open decode table file...                                               */
  /*=========================================================================*/
  open_ctl_file (fname, &fp);

  /*=========================================================================*/
  /* Read each line of the file...                                           */
  /*=========================================================================*/
  while (get_ctl_line (fp, &textp)) {

    /*=======================================================================*/
    /* Make sure we don't have too many                                      */
    /*=======================================================================*/
    if (entry_cnt >= max_entries)
      cm_error (CM_FATAL, "Table full from file %s (Max=%d).",
		fname, max_entries);

    /*=======================================================================*/
    /* Allocate space for each part and place them into decode element       */
    /*=======================================================================*/
    tmp2 = textp + strlen(textp)-1;
    if((*textp=='"') && (*tmp2=='"')) {
      textp++;
      *tmp2='\0';
    }

    if(normalize_data == NORMALIZE_DATA) {
      if ((tmpp=normalized(textp, strlen(textp))) != NULL)
	string_array[entry_cnt++] = tmpp;
    }
    else {
      if ((rc=allocate_n_copy(&tmpp, textp, fname)) == 0)
	string_array[entry_cnt++] = tmpp;
    }

  }

  /*=========================================================================*/
  /* "terminate" array elements                                              */
  /*=========================================================================*/
  if (entry_cnt < max_entries)
    string_array[entry_cnt] = '\0';

  /*dumpstringarray(fname, string_array, entry_cnt);*/

  /*=========================================================================*/
  /* and close the decode table file                                         */
  /*=========================================================================*/
  close_ctl_file (&fp);

} /* load_string_array */



/************************************************************************
* load_validation_tbl()                                                 *
*                                                                       *
*   DEFINITION                                                          *
*       Load a validation table. Similar to load_decode_tbl             *
*                                                                       *
*       Example input file line format is:                              *
*           string@@1 @2 $avalue-for-subfield-a$bvalue-for-subfield-b   *
*                                                                       *
*       entry consists of value to search against (terminated with @)   *
*       following first at will be the 'decode value' which must be     *
*       parsed...                                                       *
*             @ specifies next character is indicator followed by value *
*               (e.g., @10 means indicator 1 is 0                       *
*                      @22 means indicator 2 is 2)                      *
*             $ specifies next character is subfield followed by value  *
*               up to the next @ or $                                   *
*                                                                       *
*   PASS                                                                *
*       Name of file to be loaded.                                      *
*       pointer to array into which file is to be loaded.               *
*       maximum number of entries in the array.                         *
*                                                                       *
*   RETURN                                                              *
*       Void.  All errors are fatal.                                    *
************************************************************************/

void load_validation_tbl (char *fname,
			  DECODE_TBL *decode_table,
			  char *delim,
			  int max_entries,
			  int normalize_data)
{
  DECODE_TBL *cnp;        /* Pointer into field control table     */
  FILE    *fp;            /* Input file                           */
  char    *textp,         /* Ptr to line in control table         */
          *c_cnp,         /* Pointer to field id in ctl table line*/
          *c_mdp,         /* Ptr to subfield id                   */
          *tmpp;          /* Ptr to group id string               */
  int     entry_cnt,      /* Count of entries in file             */
          rc;

  cnp = decode_table;
  entry_cnt = 0;

  /*=========================================================================*/
  /* Open decode table file...                                               */
  /*=========================================================================*/
  open_ctl_file (fname, &fp);

  /*=========================================================================*/
  /* Read each line of the file...                                           */
  /*=========================================================================*/
  while (get_ctl_line (fp, &textp)) {

    /*=======================================================================*/
    /* Make sure we don't have too many                                      */
    /*=======================================================================*/
    if (entry_cnt >= max_entries)
      cm_error (CM_FATAL, "Decode table full from file %s (Max=%d).",
		fname, max_entries);

    /*=======================================================================*/
    /* Then break it apart at the first record separator ("@") sign          */
    /*=======================================================================*/
    c_cnp=textp;
    if((c_mdp = strstr(textp,delim))!=NULL) {
      *c_mdp='\0';
      c_mdp = c_mdp + strlen(delim);
      if (!*c_mdp) {
	cm_error (CM_ERROR,
		  "Missing decode data from entry number %d. "
		  "Expecting coded_value%sdecode_data", entry_cnt, delim);
	continue;
      }
    }

    /*=======================================================================*/
    /* Allocate space for each part and place them into decode element       */
    /*=======================================================================*/
    if(normalize_data == NORMALIZE_DATA) {
      if ((tmpp=normalized(c_cnp, strlen(c_cnp))) != NULL)
	cnp->hstar_data = tmpp;
    }
    else {
      if ((rc=allocate_n_copy(&tmpp, c_cnp, fname)) == 0)
	cnp->hstar_data = tmpp;
    }

    if(!c_mdp)
      cnp->marc_data = NULL;
    else
      if ((rc=allocate_n_copy(&tmpp, c_mdp, fname)) == 0)
	cnp->marc_data = tmpp;

    /*=======================================================================*/
    /* Advance to next element in the array and increment line count         */
    /*=======================================================================*/
    ++cnp;
    ++entry_cnt;
  }

  /*=========================================================================*/
  /* "terminate" array elements                                              */
  /*=========================================================================*/
  if (entry_cnt < max_entries)
    cnp->hstar_data = '\0';

  /*dumpdecodetbl(decode_table, entry_cnt);*/

  /*=========================================================================*/
  /* and close the decode table file                                         */
  /*=========================================================================*/
  close_ctl_file (&fp);

} /* load_validation_tbl */



/************************************************************************
* matching_parens                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       determines if there are unmatched parens in srcp...             *
*                                                                       *
*   PARAMETERS                                                          *
*       srcp          pointer to field contents                         *
*       src_len       length  of field contents                         *
*                                                                       *
*   RETURN                                                              *
*       0 if there are unmatched parens in srcp                         *
*   non-0 if all parens have matching counterparts                      *
************************************************************************/

int matching_parens(char **tsrcp,
		    size_t *tsrc_len,
		    char *begin_paren,
		    char *end_paren,
		    int  display_msg,
		    int  removal_action)
{
  char          *srcp,        /* Ptr to temp data                    */
                *tmpp;        /* Ptr to temp data                    */
  int           rc,           /* return code                         */
                notsorted,    /* return code                         */
                idx,          /* return code                         */
                numbegs,      /* return code                         */
                numends,      /* return code                         */
                savends;      /* return code                         */
  MATCHING_PAREN_ARRAY save_locs;

  srcp = *tsrcp;

  /*=========================================================================*/
  /* Initialize variables, assuming we have matching parens...               */
  /*=========================================================================*/
  numbegs =
    numends =
    forward_parens.entries =
    backward_parens.entries =
    inmost_parens.entries = 0;
  idx = -1;
  rc  =  1;

  /*=========================================================================*/
  /* First, let's move forward through the string, matching up parens...     */
  /*=========================================================================*/
  for(tmpp=srcp;tmpp<srcp+*tsrc_len;tmpp++) {
    /*=======================================================================*/
    /* If we find an opening paren...                                        */
    /*=======================================================================*/
    if(strncmp(tmpp,begin_paren,strlen(begin_paren))==0) {
      /*=====================================================================*/
      /* Count it and save its location...                                   */
      /*=====================================================================*/
      idx = numbegs;
      numbegs++;
      if(numbegs>MAX_PARENS) {
	if(display_msg==PAREN_MSG)
	  cm_error(CM_ERROR,"Maximum number of parens exceeded.");
	return -1;
      }
      forward_parens.paren_array[idx].begin_paren_loc=tmpp-srcp;
      forward_parens.paren_array[idx].end_paren_loc  =-1;
    }
    else
    /*=======================================================================*/
    /* If we find a closing paren...                                         */
    /*=======================================================================*/
    if(strncmp(tmpp,end_paren,strlen(end_paren))==0) {
      /*=====================================================================*/
      /* Count it...                                                         */
      /*=====================================================================*/
      numends++;
      if(numends>MAX_PARENS) {
	if(display_msg==PAREN_MSG)
	  cm_error(CM_ERROR,"Maximum number of parens exceeded.");
	return -1;
      }
      /*=====================================================================*/
      /* and make sure it has a matching opening paren...                    */
      /*=====================================================================*/
      for(;(idx>0)&&(forward_parens.paren_array[idx].end_paren_loc!=-1);idx--);
      /*=====================================================================*/
      /* If not, send out an error message...                                */
      /*=====================================================================*/
      if(idx<0) {
	rc = 0;
	if(display_msg==PAREN_MSG) {
	  cm_error(CM_WARNING,"Unmatched closing '%*.*s' at position %d in:",
		   strlen(end_paren),strlen(end_paren),end_paren, tmpp-srcp+1);
	  cm_error(CM_CONTINUE,"%*.*s", *tsrc_len,*tsrc_len,srcp);
	}
      }
      else
      if(forward_parens.paren_array[idx].end_paren_loc!=-1) {
	rc = 0;
	if(display_msg==PAREN_MSG) {
	  cm_error(CM_WARNING,"Unmatched closing '%*.*s' at position %d in:",
		   strlen(end_paren),strlen(end_paren),end_paren, tmpp-srcp+1);
	  cm_error(CM_CONTINUE,"%*.*s", *tsrc_len,*tsrc_len,srcp);
	}
      }
      /*=====================================================================*/
      /* If it does have a matching opening paren, save location...          */
      /*=====================================================================*/
      else
	forward_parens.paren_array[idx].end_paren_loc=tmpp-srcp;
    }
  }
  forward_parens.entries = numbegs;
  savends = numends;

  numbegs = numends = 0;
  idx = -1;
  /*=========================================================================*/
  /* Now, start from the end and work backwards, doing the same thing...     */
  /*=========================================================================*/
  for(tmpp=srcp+*tsrc_len-1;tmpp>=srcp;tmpp--) {
    /*=======================================================================*/
    /* If we find a closing paren...                                         */
    /*=======================================================================*/
    if(strncmp(tmpp,end_paren,strlen(end_paren))==0) {
      /*=====================================================================*/
      /* Count it and save its location...                                   */
      /*=====================================================================*/
      idx = numends;
      numends++;
      if(numends>MAX_PARENS) {
	if(display_msg==PAREN_MSG)
	  cm_error(CM_ERROR,"Maximum number of parens exceeded.");
	return -1;
      }
      backward_parens.paren_array[idx].begin_paren_loc=-1;
      backward_parens.paren_array[idx].end_paren_loc  =tmpp-srcp;
    }
    else
    /*=======================================================================*/
    /* If we find an opening paren...                                        */
    /*=======================================================================*/
    if(strncmp(tmpp,begin_paren,strlen(begin_paren))==0) {
      /*=====================================================================*/
      /* Count it...                                                         */
      /*=====================================================================*/
      numbegs++;
      if(numbegs>MAX_PARENS) {
	if(display_msg==PAREN_MSG)
	  cm_error(CM_ERROR,"Maximum number of parens exceeded.");
	return -1;
      }
      /*=====================================================================*/
      /* and make sure it has a matching closing paren...                    */
      /*=====================================================================*/
      for(;(idx>0)&&(backward_parens.paren_array[idx].begin_paren_loc!=-1);
	  idx--);
      /*=====================================================================*/
      /* If not, send out an error message...                                */
      /*=====================================================================*/
      if(idx<0) {
	rc = 0;
	if(display_msg==PAREN_MSG) {
	  cm_error(CM_WARNING,"Unmatched opening '%*.*s' at position %d in:",
		   strlen(begin_paren),strlen(begin_paren),begin_paren,
		   tmpp-srcp+1);
	  cm_error(CM_CONTINUE,"%*.*s", *tsrc_len,*tsrc_len,srcp);
	}
      }
      else
      if(backward_parens.paren_array[idx].begin_paren_loc!=-1) {
	rc = 0;
	if(display_msg==PAREN_MSG) {
	  cm_error(CM_WARNING,"Unmatched opening '%*.*s' at position %d in:",
		   strlen(begin_paren),strlen(begin_paren),begin_paren,
		   tmpp-srcp+1);
	  cm_error(CM_CONTINUE,"%*.*s", *tsrc_len,*tsrc_len,srcp);
	}
      }
      /*=====================================================================*/
      /* If it does have a matching closing paren, save location...          */
      /*=====================================================================*/
      else
	backward_parens.paren_array[idx].begin_paren_loc=tmpp-srcp;
    }
  }
  backward_parens.entries = numends;

  /*=========================================================================*/
  /* The counts better match up or we have a serious problem...              */
  /*=========================================================================*/
  if(((numbegs != forward_parens.entries) ||
      (savends !=backward_parens.entries)) &&
     rc &&
     (display_msg==PAREN_MSG)) {
    cm_error(CM_ERROR,"Different counts identified for matching paren scan.");
    cm_error(CM_CONTINUE," Forward: Begin-%d, End-%d.",
	     forward_parens.entries,savends);
    cm_error(CM_CONTINUE,"Backward: Begin-%d, End-%d.",
	     numbegs,backward_parens.entries);
    cm_error(CM_FATAL,"%*.*s", *tsrc_len,*tsrc_len,srcp);
  }

  /*=========================================================================*/
  /* If we don't have an even number of opening and closing and we haven't   */
  /* already sent out a message, send one out now...                         */
  /*=========================================================================*/
  if((numbegs!=numends)&&rc) {
    rc=0;
    if(display_msg==PAREN_MSG)
      cm_error(CM_WARNING,"Unmatched parens: %*.*s", *tsrc_len,*tsrc_len,srcp);
  }

  /*=========================================================================*/
  /* Print out both arrays as debugging information...                       */
  /*=========================================================================*/
  /*if(!rc) {
  if(numends>1) {
    if(display_msg==PAREN_MSG) {
      if(rc)
	cm_error(CM_WARNING,"%*.*s", *tsrc_len,*tsrc_len,srcp);

      print_paren_array("FORWARD", forward_parens);
      print_paren_array("BACKWARD", backward_parens);

    }
    }*/

  /*=========================================================================*/
  /* Now let's take care of removal actions...                               */
  /*=========================================================================*/


  /*=========================================================================*/
  /* If we're to remove unmatched initial parens...                          */
  /*=========================================================================*/
  if((removal_action==REMOVE_UNMATCHED_INITIAL) ||
     (removal_action==REMOVE_UNMATCHED_ENDS)) {
    /*=======================================================================*/
    /* Check to see if we have any...                                        */
    /*=======================================================================*/
    if((forward_parens.entries>0)                           &&
       (forward_parens.paren_array[0].begin_paren_loc==  0) &&
       (backward_parens.paren_array[0].end_paren_loc == -1)) {
      /*=====================================================================*/
      /* If so, remove them from the string and decrease its length...       */
      /*=====================================================================*/
      tmpp=srcp+1;
      remove_substr(&srcp,srcp,tmpp);
      --*tsrc_len;
      /*=====================================================================*/
      /* Then remove the corresponding entry from the paren_array and be     */
      /* sure to decrement begin_paren_loc for every other instance...       */
      /*=====================================================================*/
      for(idx=0;idx<--forward_parens.entries;idx++) {
	forward_parens.paren_array[idx]=forward_parens.paren_array[idx+1];
	forward_parens.paren_array[idx].begin_paren_loc--;
      }
      /*=====================================================================*/
      /* Send out informational message...                                   */
      /*=====================================================================*/
      if(display_msg==PAREN_MSG)
	cm_error(CM_CONTINUE,"Removing unmatched initial paren...");
    }
  }

  /*=========================================================================*/
  /* If we're to remove unmatched terminating parens...                      */
  /*=========================================================================*/
  if((removal_action==REMOVE_UNMATCHED_TERMINAL) ||
     (removal_action==REMOVE_UNMATCHED_ENDS)) {
    /*=======================================================================*/
    /* Check to see if we have any...                                        */
    /*=======================================================================*/
    if((backward_parens.entries>0)                          &&
       (backward_parens.paren_array[0].begin_paren_loc==-1) &&
       (backward_parens.paren_array[0].end_paren_loc == *tsrc_len-1)) {
      /*=====================================================================*/
      /* If so, remove them from the string and decrease its length...       */
      /*=====================================================================*/
      tmpp=srcp+*tsrc_len-1;
      *tmpp='\0';
      --*tsrc_len;
      /*=====================================================================*/
      /* Then remove the corresponding entry from the paren_array...         */
      /*=====================================================================*/
      for(idx=0;idx<--backward_parens.entries;idx++) {
	backward_parens.paren_array[idx]=backward_parens.paren_array[idx+1];
      }
      /*=====================================================================*/
      /* Send out informational message...                                   */
      /*=====================================================================*/
      if(display_msg==PAREN_MSG)
	cm_error(CM_CONTINUE,"Removing unmatched terminal paren...");
    }
  }

  /*=========================================================================*/
  /* If we're to remove any unmatched parens, we're in trouble cause we can't*/
  /* do that yet...                                                          */
  /*=========================================================================*/
  if(removal_action==REMOVE_UNMATCHED_ANY) {
    if(display_msg==PAREN_MSG) {
      cm_error(CM_ERROR,"Instructions received to remove unmatched parens.");
      cm_error(CM_CONTINUE,"This option not implemented. No action taken.");
    }
  }

  /*=========================================================================*/
  /* Now let's re-arrange order from innermost out into inmost_parens...     */
  /*=========================================================================*/
  inmost_parens = forward_parens;

  /*=========================================================================*/
  /* Let's do bubble sort since most of them will be in order or close to it */
  /*=========================================================================*/
  for(notsorted=(inmost_parens.entries>1);notsorted;) {
    notsorted=0;
    /*=======================================================================*/
    /* Walk through each of them...                                          */
    /*=======================================================================*/
    for(idx=0;idx<inmost_parens.entries-1;idx++) {
      /*=====================================================================*/
      /* If any are unmatched, remove them from this array...                */
      /*=====================================================================*/
      if((inmost_parens.paren_array[idx].begin_paren_loc==-1)||
	 (inmost_parens.paren_array[idx].end_paren_loc  ==-1)) {
	notsorted=1;
	/*===================================================================*/
	/* ...by collapsing the rest around it                               */
	/*===================================================================*/
	for(;idx<--inmost_parens.entries;idx++)
	  inmost_parens.paren_array[idx]=inmost_parens.paren_array[idx+1];
	break;
      }

      /*=====================================================================*/
      /* If the current one is greater than the next one...                  */
      /*=====================================================================*/
      if(inmost_parens.paren_array[idx].end_paren_loc>
	 inmost_parens.paren_array[idx+1].end_paren_loc) {
	/*===================================================================*/
	/* Switch them around...                                             */
	/*===================================================================*/
	save_locs=inmost_parens.paren_array[idx];
	inmost_parens.paren_array[idx]=inmost_parens.paren_array[idx+1];
	inmost_parens.paren_array[idx+1]=save_locs;
	notsorted=1;
      }
    }
  }

  /* if(inmost_parens.entries>1)
     print_paren_array("INMOST", inmost_parens); */




  return rc;

} /* matching_parens */



void print_paren_array(char *array_desc, MATCHING_PAREN_STRUCT paren_struct)
{
  int idx;

  cm_error(CM_CONTINUE,"%s ARRAY",array_desc);
  cm_error(CM_CONTINUE," Begin    End");
  cm_error(CM_CONTINUE," -----  -----");

  for(idx=0;idx<paren_struct.entries;idx++)
    cm_error(CM_CONTINUE," %5d  %5d",
	     paren_struct.paren_array[idx].begin_paren_loc,
	     paren_struct.paren_array[idx].end_paren_loc);
}



/************************************************************************
* normalized()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Allocates memory and copies src to dest after normalizing it    *
*                                                                       *
*   PASS                                                                *
*       Ptr to src                                                      *
*       Size to be allocated                                            *
*                                                                       *
*   RETURN                                                              *
*       ptr to normalized data on success                               *
*       NULL if failure                                                 *
************************************************************************/

char *normalized(char *srcp, size_t src_len)
{
  char          *tmpp;          /* Ptr to temp data                    */
  char          *dest;          /* returned Ptr                        */
  int           numspaces;

  /*=========================================================================*/
  /* Allocate memory based on size of src...                                 */
  /*=========================================================================*/

#ifdef DEBUG
  if ((dest = (char *) marc_alloc(src_len+1, 908)) == NULL) { ///TAG:908
      cm_error(CM_ERROR, "Error allocating memory for normalization process. " "Record not processed");
      return dest;
  }
#else
  if ((dest = (char *)malloc(src_len+1)) == NULL) {
    cm_error(CM_ERROR, "Error allocating memory for normalization process. " "Record not processed");
    return dest;
  }
#endif

  /*=========================================================================*/
  /* Initialize the destination buffer...                                    */
  /*=========================================================================*/
  memset(dest, '\0', src_len+1);

  /*=========================================================================*/
  /* and begin copying over the contents of the source buffer                */
  /*=========================================================================*/
  for(tmpp=srcp;(*tmpp) && (tmpp<srcp+src_len);tmpp++) {
    /*=======================================================================*/
    /* If a character is alpha-numeric, copy it over...                      */
    /*=======================================================================*/
    if(isalnum(*tmpp) ||
       (*tmpp=='-'))
      strncat(dest, tmpp, 1);
    else {
      /*=====================================================================*/
      /* also copy over a single space but ignore any other non-alphanumerics*/
      /*=====================================================================*/
      numspaces = 0;
      for(;(*tmpp)&&(!isalnum(*tmpp))&&(tmpp<srcp+src_len); tmpp++) {
	if((*tmpp==' ')&&(numspaces==0)) {
	  strcat(dest, " ");
	  numspaces++;
	}
      }
      /*=====================================================================*/
      /* since we went forward, back up one to be properly placed            */
      /*=====================================================================*/
      tmpp--;
    }

  }

  /*=========================================================================*/
  /* and now lowercase everything...                                         */
  /*=========================================================================*/
  for(tmpp=dest;*tmpp;tmpp++) {
    *tmpp=tolower(*tmpp);
  }


  return dest;

} /* normalized */



/************************************************************************
* parse_decode_table                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       parses decode table element into output field tag               *
*                                                                       *
*   PASS                                                                *
*       ptr to table entry                                              *
*       tag id                                                          *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/

CM_STAT parse_decode_tbl(CM_PROC_PARMS *pp,
			 char *tag,
			 DECODE_TBL *cnp)
{
  char *tmpp;
  char *delimiter;
  char *field;
  char *subfield;
  char *nextchar;

  char *advancer;

  int first_time;

  /*=========================================================================*/
  /* Do all allocations up front                                             */
  /*=========================================================================*/

#ifdef DEBUG
  tmpp = (char *) marc_alloc(strlen(cnp->marc_data) + 2, 909);  //TAG:909
  delimiter = (char *) marc_alloc(2, 910);  //TAG:910
  field = (char *) marc_alloc(CM_CTL_MAX_LINE, 911);  //TAG:911
  subfield = (char *) marc_alloc(2, 912); //TAG:912
  nextchar = (char *) marc_alloc(2, 913); //TAG:913
#else
  tmpp = (char *) malloc(strlen(cnp->marc_data) + 2);
  delimiter = (char *) malloc(2);
  field = (char *) malloc(CM_CTL_MAX_LINE);
  subfield = (char *) malloc(2);
  nextchar = (char *) malloc(2);
#endif

  /*=========================================================================*/
  /* and return error if any is null                                         */
  /*=========================================================================*/
  if ((tmpp      == NULL) ||
      (delimiter == NULL) ||
      (field     == NULL) ||
      (subfield  == NULL) ||
      (nextchar  == NULL)) {
    if (*tag)
      cm_error(CM_ERROR,"Error allocating memory for parsing of decode table "
	       "for tag %s. Record not processed", tag);
    else
      cm_error(CM_ERROR,"Error allocating memory for parsing of decode table. "
	       "Record not processed");
    return CM_STAT_KILL_RECORD;
  }

  /*=========================================================================*/
  /* Initialize everything                                                   */
  /*=========================================================================*/
  first_time = 1;

  memset(tmpp, '\0', strlen(cnp->marc_data)+2);
  memset(delimiter, '\0', 2);
  memset(field, '\0', CM_CTL_MAX_LINE);
  memset(subfield, '\0', 2);
  memset(nextchar, '\0', 2);

  strcpy(field, tag);

  /*=========================================================================*/
  /* Walk through the marc_data...                                           */
  /*=========================================================================*/
  for(advancer = cnp->marc_data; *advancer; advancer++) {

    strncpy(nextchar, advancer, 1);

    /*=======================================================================*/
    /* If the next character is a delimiter...                               */
    /*=======================================================================*/
    if ((strcmp(nextchar,DLM_TAG)==0) ||
	(strcmp(nextchar,DLM_INDICATOR)==0) ||
	(strcmp(nextchar,DLM_SUBFIELD)==0)) {
      /*=====================================================================*/
      /* and what we had before was a delimiter for an indicator or subfield */
      /*=====================================================================*/
      if ((strcmp(delimiter,DLM_INDICATOR)==0) ||
	  (strcmp(delimiter,DLM_SUBFIELD)==0)) {
	/*===================================================================*/
	/* We need to write out the contents of that indicator/subfield...   */
	/*===================================================================*/
	write_fld_sfld(pp, tmpp, delimiter, field, subfield, &first_time);
      }
      /*=====================================================================*/
      /* otherwise, the delimiter we had before must have been for a new tag */
      /* Set first_time flag back to one so we create a new instance,        */
      /*=====================================================================*/
      else {
	first_time = 1;
      }
      /*=====================================================================*/
      /* Once we're done with the previous stuff, prepare for the current    */
      /* delimiter by copying delimiter and subfield and clearing output     */
      /*=====================================================================*/
      strcpy(delimiter, nextchar);
      if ((strcmp(delimiter,DLM_INDICATOR)==0) ||
	  (strcmp(delimiter,DLM_SUBFIELD)==0)) {
	strncpy(subfield, ++advancer, 1);
	memset(tmpp, '\0', strlen(cnp->marc_data)+2);
      }
      else
	memset(field, '\0', CM_CTL_MAX_LINE);

    }

    /*=======================================================================*/
    /* If the next character is a not a delimiter, put it where it belongs   */
    /*=======================================================================*/
    else {
      /*=====================================================================*/
      /* If we have an indicator or subfield delimiter, move it to output    */
      /*=====================================================================*/
      if ((strcmp(delimiter,DLM_INDICATOR)==0) ||
	  (strcmp(delimiter,DLM_SUBFIELD)==0))
	strcat(tmpp, nextchar);
      else {
	/*===================================================================*/
	/* If we have a field delimiter, only move it if it's not a blank    */
	/*===================================================================*/
	if (strcmp(DLM_TAG, delimiter) == 0) {
	  if (strcmp(nextchar, " ")!=0)
	    strcat(field, nextchar);
	  else
	    /*===============================================================*/
	    /* If it is a blank, let's not copy any more characters to field */
	    /*===============================================================*/
	    strcpy(delimiter, "");
	}
      }
    }
  }

  /*=========================================================================*/
  /* We need to write out the contents of the last indicator/subfield...     */
  /*=========================================================================*/
  if ((strcmp(delimiter,DLM_INDICATOR)==0) ||
      (strcmp(delimiter,DLM_SUBFIELD)==0))
    write_fld_sfld(pp, tmpp, delimiter, field, subfield, &first_time);


  /*=========================================================================*/
  /* Now release memory                                                      */
  /*=========================================================================*/

#ifdef DEBUG
  marc_dealloc(tmpp, 914); //TAG:914
  marc_dealloc(delimiter, 915); //TAG:915
  marc_dealloc(field, 916);  //TAG:916
  marc_dealloc(subfield, 917); //TAG:917
  marc_dealloc(nextchar, 918); //TAG:918
#else
  free(tmpp);
  free(delimiter);
  free(field);
  free(subfield);
  free(nextchar);
#endif

  return CM_STAT_OK;

} /* parse_decode_table */



/************************************************************************
* remove_parens_from_begin                                              *
*                                                                       *
*   DEFINITION                                                          *
*       searches for matching parens at beginning of string & removes   *
*                                                                       *
*   PARAMETERS                                                          *
*       outp        ptr to data string                                  *
*       out_len     length of data                                      *
*       begin_paren beginning paren                                     *
*       end_paren   ending paren                                        *
*                                                                       *
*   RETURN                                                              *
*       Pointer to beginning of string (minus bracketed data, if exists)*
************************************************************************/

char *remove_parens_from_begin(char *outp,
			      size_t *out_len,
			      char *begin_paren,
			      char *end_paren)
{
  char          *tmpp;        /* Ptr to temp data                    */

  /*=========================================================================*/
  /* Check to see if the first character of outp matches begin_paren         */
  /*=========================================================================*/
  tmpp = outp;
  if (*tmpp==*begin_paren) {

    /*=======================================================================*/
    /* Walk forward until you get to end_paren                               */
    /*=======================================================================*/
    for(tmpp++;((*tmpp!=*end_paren)&&(tmpp<outp+*out_len));tmpp++);

    /*=======================================================================*/
    /* Then walk forward until you get a non-space character                 */
    /*=======================================================================*/
    for(tmpp++;((*tmpp==' ')&&(tmpp<outp+*out_len));tmpp++);

    /*=======================================================================*/
    /* Reset out_len                                                         */
    /*=======================================================================*/
    *out_len -= (tmpp - outp);
  }

  return tmpp;
} /* remove_parens_from_begin */



/************************************************************************
* remove_parens_from_end                                                *
*                                                                       *
*   DEFINITION                                                          *
*       searches for matching parens at end of string and removes them  *
*                                                                       *
*   PARAMETERS                                                          *
*       outp        ptr to data string                                  *
*       out_len     length of data                                      *
*       begin_paren beginning paren                                     *
*       end_paren   ending paren                                        *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void remove_parens_from_end(char *outp,
			    size_t *out_len,
			    char *begin_paren,
			    char *end_paren)
{
  char          *tmpp;        /* Ptr to temp data                    */

  /*=========================================================================*/
  /* Check to see if the last character of outp matches end_paren            */
  /*=========================================================================*/
  tmpp = outp + *out_len - 1;
  if (*tmpp==*end_paren) {

    /*=======================================================================*/
    /* Back up until you get to the begin_paren                              */
    /*=======================================================================*/
    for(tmpp--; ((*tmpp!=*begin_paren)&&(tmpp>outp)); tmpp--);

    /*=======================================================================*/
    /* Then back up until you get a non-space character                      */
    /*=======================================================================*/
    tmpp--;
    if (*tmpp==' ')
      for(; ((*tmpp==' ') && (tmpp > outp)); tmpp--);

    /*=======================================================================*/
    /* Move forward one to terminate the string and reset out_len            */
    /*=======================================================================*/
    tmpp++;
    *tmpp = '\0';
    *out_len = strlen(outp);
  }
} /* remove_parens_from_end */



/************************************************************************
* remove_substr                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       removes from srcp, the substr delineated by begpoint & endpoint *
*                                                                       *
*   PARAMETERS                                                          *
*       srcp        ptr to data string                                  *
*       beg_point   beginning of substr to be removed                   *
*       end_point   ending of substr to be removed (exclusive)          *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void    remove_substr(char **srcp, char *beg_point, char *end_point)
{
  char          *tmpp,        /* Ptr to temp data                    */
                *tmp2;        /* Ptr to temp data                    */

  /*=========================================================================*/
  /* From beginning, copy everything up to beg_point...                      */
  /*=========================================================================*/
  if((end_point>=beg_point)&&(beg_point>=*srcp)) {

    for(tmpp=tmp2=*srcp;(tmpp<beg_point)&&*tmpp;)
      *tmp2++=*tmpp++;

    /*=======================================================================*/
    /* Now skip from beg_point up to, but not including, end_point           */
    /*=======================================================================*/
    for(;tmpp<end_point;tmpp++);

    /*=======================================================================*/
    /* Then, copy whatever's remaining over...                               */
    /*=======================================================================*/
    for(;*tmpp;)
      *tmp2++=*tmpp++;

    /*=======================================================================*/
    /* Finally, insure we terminate the string...                            */
    /*=======================================================================*/
    *tmp2='\0';
  }

} /* remove_substr */



/************************************************************************
* skip_punc                                                             *
*                                                                       *
*   DEFINITION                                                          *
*       moves pointer forward, skipping over punctuation and spaces     *
*                                                                       *
*   PASS                                                                *
*       ptr                                                             *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void skip_punc(char **srcp)
{
  char *tmpp;

  for(tmpp=*srcp;((ispunct(*tmpp))||(*tmpp==' '));tmpp++);

  *srcp = tmpp;
} /* skip_punc */



/************************************************************************
* skip_punc_backwards                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       moves pointer backwards, skipping over punctuation and spaces   *
*       then moving forward one so as to be beyond "good" data.         *
*                                                                       *
*   PASS                                                                *
*       ptr                                                             *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void skip_punc_backwards(char *ostr, char **srcp)
{
  char *tmpp;

  for(tmpp=*srcp;(tmpp>=ostr)&&((ispunct(*tmpp))||(*tmpp==' '));tmpp--);
  if(tmpp!=*srcp)
    tmpp++;

  *srcp = tmpp;
} /* skip_punc_backwards */



/************************************************************************
* skip_punc_no_paren                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       moves pointer forward, skipping over punctuation and spaces     *
*       but stopping if it encounters any stopchars                     *
*   PASS                                                                *
*       ptr                                                             *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void skip_punc_no_paren(char **srcp, char *stopchars)
{
  char *tmpp;

  for(tmpp=*srcp;
      (*tmpp==' ')||
	(ispunct(*tmpp)&&(strchr(stopchars,*tmpp)==NULL));
      tmpp++);

  *srcp = tmpp;
} /* skip_punc_no_paren */



/************************************************************************
* skip_punc_backwards_no_paren                                          *
*                                                                       *
*   DEFINITION                                                          *
*       moves pointer backwards, skipping over punctuation and spaces   *
*       then moving forward one so as to be beyond "good" data.         *
*       Processing stops if stopchars encountered.                      *
*   PASS                                                                *
*       ptr                                                             *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void skip_punc_backwards_no_paren(char *ostr, char **srcp, char *stopchars)
{
  char *tmpp;

  for(tmpp=*srcp;
      (tmpp>=ostr)&&
	(
	 (*tmpp==' ')||
	 (ispunct(*tmpp)&&(strchr(stopchars,*tmpp)==NULL))
	 );
      tmpp--);
  tmpp++;

  *srcp = tmpp;
} /* skip_punc_backwards_no_paren */



/************************************************************************
* skip_spaces                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       moves pointer forward, skipping over spaces                     *
*                                                                       *
*   PASS                                                                *
*       ptr                                                             *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void skip_spaces(char **srcp)
{
  char *tmpp;

  for(tmpp=*srcp;(*tmpp==' ');tmpp++);

  *srcp = tmpp;
} /* skip_spaces */



/************************************************************************
* stricmp                                                               *
*                                                                       *
*   DEFINITION                                                          *
*       does equivalent of strcmp but ignoring case...                  *
*                                                                       *
*   PASS                                                                *
*       *s, *t                                                          *
*                                                                       *
*   RETURN                                                              *
*       <0 if s less than t                                             *
*        0 if s equals t                                                *
*       >0 if s greater than t                                          *
************************************************************************/

int stricmp(char *s, char *t)
{
  for (;tolower(*s)==tolower(*t); s++, t++)
    if(*s=='\0')
      return 0;

  return tolower(*s) - tolower(*t);

} /* stricmp */



/************************************************************************
* strincmp                                                              *
*                                                                       *
*   DEFINITION                                                          *
*       does equivalent of strncmp but ignoring case...                 *
*                                                                       *
*   PASS                                                                *
*       *s, *t, i                                                       *
*                                                                       *
*   RETURN                                                              *
*       <0 if s less than t                                             *
*        0 if s equals t                                                *
*       >0 if s greater than t                                          *
************************************************************************/

int strincmp(char *s, char *t, int i)
{
  int j;

  for (j=1; (tolower(*s)==tolower(*t)) && (j<i); s++, t++, j++)
    if(*s=='\0')
      return 0;


  return tolower(*s) - tolower(*t);

} /* strincmp */



/************************************************************************
* strcmpnrm                                                             *
*                                                                       *
*   DEFINITION                                                          *
*       does equivalent of strcmp after normalizing strings...          *
*                                                                       *
*   PASS                                                                *
*       *s, *t                                                          *
*                                                                       *
*   RETURN                                                              *
*       <0 if s less than t                                             *
*        0 if s equals t                                                *
*       >0 if s greater than t                                          *
************************************************************************/

int strcmpnrm(char *s, char *t) {

  char *nrms, *nrmt;
  int rc;

  nrms=normalized(s, strlen(s));
  nrmt=normalized(t, strlen(t));

  rc = strcmp(nrms, nrmt);

#ifdef DEBUG
  marc_dealloc(nrms, 919); //TAG:919
  marc_dealloc(nrmt, 920); //TAG:920
#else
  free(nrms);
  free(nrmt);
#endif

  return rc;

} /* strcmpnrm */



/************************************************************************
* strncmpnrm                                                            *
*                                                                       *
*   DEFINITION                                                          *
*       does equivalent of strncmp after normalizing string...          *
*                                                                       *
*   PASS                                                                *
*       *s, *t, i                                                       *
*                                                                       *
*   RETURN                                                              *
*       <0 if s less than t                                             *
*        0 if s equals t                                                *
*       >0 if s greater than t                                          *
************************************************************************/

int strncmpnrm(char *s, int i, char *t, int j) {

  char *nrms, *nrmt;
  int rc;

  nrms=normalized(s, i);
  nrmt=normalized(t, j);

  rc = strcmp(nrms, nrmt);

#ifdef DEBUG
  marc_dealloc(nrms, 921); //TAG:921
  marc_dealloc(nrmt, 922); //TAG:922
#else
  free(nrms);
  free(nrmt);
#endif

  return rc;

} /* strncmpnrm */



/************************************************************************
* strwrd                                                                *
*                                                                       *
*   DEFINITION                                                          *
*       determines whether srcp contains word wrd...                    *
*                                                                       *
*   PASS                                                                *
*       ptr to word  to be searched                                     *
*       size of word to be searched                                     *
*       ptr to word to look for                                         *
*                                                                       *
*   RETURN                                                              *
*       non-zero if found; 0 if not                                     *
************************************************************************/
int     strwrd(char *srcp, size_t src_len, char *wrd)
{
  char          *tmpp;
  size_t        src2len;

  /*=========================================================================*/
  /* Walk through srcp, inspecting each word...                              */
  /*=========================================================================*/
  for(tmpp=NULL;find_word(srcp,src_len,&tmpp,&src2len," ",WORD_FORWARD);)
    if(strncmpnrm(tmpp,src2len,wrd,strlen(wrd))==0) {
      return 1;
      break;
    }

  return 0;

} /* strwrd */



/************************************************************************
* write_fld_sfld                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       formats parameters and writes output field/subfield             *
*                                                                       *
*   PASS                                                                *
*       ptr to table entry                                              *
*       ptr to subfield value                                           *
*       ptr to delimiter (@ = indicator, $ = subfield)                  *
*       ptr to field to be written                                      *
*       ptr to subfield to be written                                   *
*       ptr to flag indicating whether field is to be newly created     *
*                                                                       *
*   RETURN                                                              *
*       nothing                                                         *
************************************************************************/

void write_fld_sfld(CM_PROC_PARMS *pp,
		    char *tmpp,
		    char *delimiter,
		    char *field,
		    char *subfield,
		    int *first_time)
{
  char tag_buf[12];

  /*=========================================================================*/
  /* First, make sure we have a field & subfield to write to                 */
  /*=========================================================================*/
  if (*field && *subfield) {
    /*=======================================================================*/
    /* Make sure that we have a valid field                                  */
    /*=======================================================================*/
    if (strlen(field) > 3) {
      cm_error(CM_ERROR,"Format of decode table incorrect for identifying tag."
	       " Record not processed");
    }
    /*=======================================================================*/
    /* If this is the first time to this field, create a new instance        */
    /*=======================================================================*/
    if (*first_time) {
      sprintf(tag_buf, "%s:+%s%s", field, delimiter, subfield);
      *first_time = 0;
    }
    else
      sprintf(tag_buf, "%s:-1%s%s", field, delimiter, subfield);

    /*=======================================================================*/
    /* then write out the contents                                           */
    /*=======================================================================*/
    cmp_buf_write(pp, tag_buf, (unsigned char *)tmpp, strlen(tmpp), 0);
  }

} /* write_fld_sfld */



