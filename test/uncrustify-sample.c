/*
Extremely poorly formatted code to test uncrustify.cfg against the contiki code style
 */

#if defined (FOO)
 # define BAR 3
#else
 # define BAR 3
# endif

/* Aligment of parameters doesn't work as completely expected. Should align the
 * stars themselves. */
static int    some_function ( int  *f  , char **c, LongTypeException a)      {

	/* This is indented with a tab. Should become spaces */
	int a = 5;      // This should become a C comment
	int d= - 10;    /* Space around assignment, No space between - and 10 */
	int* b;         /* no space before the *, yes space between * and variable name */

  some_function(
		a,
		b
	)

/* Should indent the for correctly and sort out spacing mess:
 - for(i = 0; i < 10; ++i)
 - Should pull the opening brace up to the same line as the for
*/
for(  i=0  ;i<10; ++ i ) {	if (a < 0) {
	  a= ! c ; /* Should add space after a and remove space before and after c */

	  /* } else { always in the same line */
	}
	else
	{
	    /* incorrect indentation here */

	    f();
	}
	}

  b = & c;   /* 'address of' adjacent to variable */
  * b = 3;    /* dereference: no space */

  /* Should automatically add braces below */
  if(a == 0)
    printf ( "a\n") ;

  while(1)  ; /* This needs fixed */

  switch(a) {
  case    3  :
4;
5;
  break;
  }

  /* No blank lines before the closing brace */

  return (-1); /* No parenthesis around return values */


}
