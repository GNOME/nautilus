
/* ad hoc vcard parsing routines - replace with something better sometime */

char * vcard_full_name (gchar* vcard);
char * vcard_title(gchar* vcard);
char * vcard_postal(gchar* vcard);
char * vcard_email(gchar* vcard);
char * vcard_fax(gchar* vcard);
char * vcard_web(gchar* vcard);
char * vcard_logo(gchar* vcard);
char * vcard_streetaddress(gchar* vcard);
char * vcard_city_state_zip(gchar* vcard);
char * vcard_company(gchar* vcard);
char * vcard_address_list(gchar* vcard);

