typedef union
{
	char *val_string;
	int val_number;
	int val_version_major, val_version_minor;
	int val_enum;
} YYSTYPE;

#define	Q_CONST_STRING	257
#define	Q_CONST_NUMBER	258
#define	Q_CONST_VERSION	259
#define	Q_CONST_ID	260

#define	Q_EQ	272
#define	Q_NEQ	273
#define	Q_LEQ	274
#define	Q_GEQ	275
#define	Q_LT	276
#define	Q_GT	277

#define	Q_PERIOD  282

#define	PARSE_ERROR	283


extern YYSTYPE yylval;
