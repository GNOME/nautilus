%{

/*
This was mostly grabbed from gnome/oaf/oafd/ac-query-lex.l
*/

#include <glib.h>
#include <eazel-install-query-parse.h>

#include <string.h>
#include <stdlib.h>
#define YY_NO_UNPUT

static char* putSymbol( char *_name );
static char* putString( char *_name );
static int yywrap(void);
int yylex (void);
void initFlex( const char *_code );

%}

DIGIT [0-9]

%%

"=="  { return Q_EQ; }
"!="  { return Q_NEQ; }
"<"   { return Q_LT; }
">"   { return Q_GT; }
"<="  { return Q_LEQ; }
">="  { return Q_GEQ; }

"."   { return Q_PERIOD; }

{DIGIT}+ { yylval.val_number = atof( yytext ); return Q_CONST_NUMBER; }
{DIGIT}+"."{DIGIT}+ { char *ptr; yylval.val_version_major = atof( yytext ); 
                      ptr = strchr (yytext, '.'); ptr++;
                      yylval.val_version_minor = atof (ptr); return Q_CONST_VERSION; }

"'"(\\'|[^'])+"'" { yylval.val_string = putString( yytext ); return Q_CONST_STRING; }
[a-zA-Z_-][a-zA-Z0-9_:-]* { yylval.val_string = putSymbol( yytext ); return Q_CONST_ID; }

[ \t\n\r]+          /* eat up whitespace */

. { return PARSE_ERROR; }

%%

static char *
putSymbol( char *_name )
{
  return g_strdup(_name);
}

static char *
putString( char *_str )
{
  int l = strlen( _str );
  char *p = (char*)g_malloc( l + 1);
  char *s = _str + 1;
  char *d = p;
  while ( s < _str + l - 1 )
  {
     if ( *s != '\\' )
        *d++ = *s++;
     else
     {
        s++;
        if ( *s == '\\' )
          *d++ = '\\';
        else if ( *s == 'n' )
          *d++ = '\n';
        else if ( *s == 'r' )
          *d++ = '\r';
        else if ( *s == 't' )
          *d++ = '\t';
        s++;
     }
  }

  *d = 0;
  return p;
}

void
initFlex( const char *_code )
{
  yy_switch_to_buffer( yy_scan_string( _code ) );
}

static int
yywrap(void)
{
  yy_delete_buffer( YY_CURRENT_BUFFER );
  return 1;
}
