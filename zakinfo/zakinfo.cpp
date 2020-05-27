#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char * loadFile(const char *fn, int &len)
{
	FILE * f = fopen(fn, "rb");
	if (!f)
	{
		return 0;
	}
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *b = new char[len];
	if (!b)
	{
		fclose(f);
		return 0;
	}
	fread(b, 1, len, f);
	fclose(f);
	return b;
}

unsigned short* outbuf = 0;
int outbuf_idx = 0;
int debug = 0;

char rfc3986[256] = { 0 };
char html5[256] = { 0 };

void url_encoder_rfc_tables_init() {

	int i;

	for (i = 0; i < 256; i++) {

		rfc3986[i] = isalnum(i) || i == '~' || i == '-' || i == '.' || i == '_' ? i : 0;
		html5[i] = isalnum(i) || i == '*' || i == '-' || i == '.' || i == '_' ? i : (i == ' ') ? '+' : 0;
	}
}

void url_encode_print(char *table, unsigned char *s) 
{
	for (; *s; s++) {

		if (table[*s]) printf("%c", table[*s]);
		else printf("%%%02X", *s);
	}
}

void printPascalString_html(char* s)
{
	int len = *(unsigned char*)s;
	s++;
	while (len)
	{
		if (*s == '<')
			printf("&lt;");
		else if (*s == '>')
			printf("&gt;");
		else if (*s == '&')
			printf("&amp;");
		else if (*s >= 32 && *s <= 126)
			printf("%c", *s);
		else
			printf("?");
		s++;
		len--;
	}
}

void printPascalString(char* s)
{
	int len = *(unsigned char*)s;
	s++;
	while (len)
	{
		if (*s >= 32 && *s <= 126)
			printf("%c", *s);
		else
			printf("?");
		s++;
		len--;
	}
}

class Zak
{
public:
	int hdrsize;
	int chiptype;
	int flags;
	int kchunks;
	int lastchunk;
	int loopchunk;
	int loopbyte;
	int emuspeed;
	int chipspeed;
	int totalsize;
	int ay;
	char * data;

	Zak() { data = 0; }
	~Zak() { delete[] data; }

	void print_header()
	{
		printf(
			"<!DOCTYPE html>\n"
			"<html lang=\"en\">\n"
			"  <head>\n"
			"    <meta charset=\"utf-8\" />\n"
			"    <meta http-equiv=\"x-ua-compatible\" content=\"ie=edge\" />\n"
			"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
			"\n"
			"    <title>Zak</title>\n"
			"\n"
			"    <link rel=\"stylesheet\" href=\"css/main.css\" />\n"
			"    <link rel=\"icon\" href=\"images/favicon.png\" />\n"
			"  </head>\n"
			"\n"
			"  <body>\n"
			"    <table id=\"zf\">\n"
			"      <tr>\n"
			"        <th>File</th>\n"
			"        <th>Type</th>\n"
			"        <th>Hz</th>\n"
			"        <th>Chip model</th>"
			"        <th>Flags</th>"
			"        <th>Song name</th>\n"
			"        <th>Author</th>\n"
			"        <th>Comment</th>\n"
			"      </tr>\n"
			);
	}

	char * songtype()
	{
		switch (chiptype)
		{
		case 0: return("SID"); 
		case 1: return("AY/YM");
		case 2: return("Turbosound");
		case 3: return("Turbosound Next");
		case 4: return("TED");
		case 5: return("2xSID");
		case 6: return("TED+2xSID");
		case 7: return("AdLib");
		}
		return "Unknown";
	}

	char * chipmodel()
	{
		switch (chiptype)
		{
		case 1:
		case 2:
		case 3:
			if (flags & 64) return("YM2149F "); 
			return("AY-3-8910 ");
		case 0:
		case 5:
		case 6:
			if ((flags & 48) == 0) return("6581-SID ");
			if ((flags & 48) == 16) return("8580-SID ");
			if ((flags & 48) == 32) return("8580DB-SID ");
			return("6581R1-SID ");
		}
		return "n/a";
	}

	void print_html(char *fn)
	{
		char chopfn[100];
		int i = 0;
		while (i < 20 && fn[i])
		{
			// just discard illegal characters..
			if (fn[i] > 32 && fn[i] < 126 && fn[i] != '<' && fn[i] != '>' && fn[i] != '&')
			{
				chopfn[i] = fn[i];
				i++;
			}
		}
		if (i == 20)
		{
			chopfn[i] = '.'; i++;
			chopfn[i] = '.'; i++;
			chopfn[i] = '.'; i++;
		}
		chopfn[i] = 0;

		printf(
			"      <tr>\n"
			"        <td><a href=\"");
		url_encode_print(html5, (unsigned char*)fn);
		printf("\">%s</a></td>\n"
			"        <td>%s</td>\n"
			"        <td>%d</td>\n"
			"        <td>%s</td>\n",
			chopfn,
			songtype(),
			emuspeed,
			chipmodel()
			);

		printf("        <td>");
		if (flags & 1) printf("Raw"); else printf("zx7");
		if (flags & 2) printf(" Digidrums");
		if (flags & 4) printf(" Signed-digidrum");
		if (flags & 8) printf(" ST4-digidrum");
		printf("</td>\n");
		printf("        <td>");
		char* stringptr = (char*)data + 28;
		printPascalString_html(stringptr);
		printf("</td>\n");
		printf("        <td>");
		stringptr += *(unsigned char*)stringptr + 1;
		printPascalString_html(stringptr);
		printf("</td>\n");
		printf("        <td>");
		stringptr += *(unsigned char*)stringptr + 1;
		printPascalString_html(stringptr);
		printf("</td>\n");

		printf("      </tr>\n");

	}

	void print_footer()
	{
		printf(
			"    </table>\n"
			"  </body>\n"
			"</html>\n"
		);
	}

	void print_info(char *fn)
	{
		printf("Filename     : %s\n", fn);
		printf("Chip type    : ");
		switch (chiptype)
		{
		case 0: printf("SID"); break;
		case 1: printf("AY/YM"); ay = 1;  break;
		case 2: printf("Turbosound"); ay = 1; break;
		case 3: printf("Turbosound Next"); ay = 1; break;
		case 4: printf("TED"); break;
		case 5: printf("2xSID"); break;
		case 6: printf("TED+2xSID"); break;
		case 7: printf("AdLib"); break;
		default:
			printf("Unknown");
		}
		printf("\nFlags        : ");
		if (flags & 1) printf("Uncompressed "); else printf("Compressed ");
		if (flags & 2) printf("Digidrums ");
		if (flags & 4) printf("Signed-digidrum ");
		if (flags & 8) printf("ST4-digidrum ");
		if ((flags & 48) == 0) printf("6581-SID ");
		if ((flags & 48) == 16) printf("8580-SID ");
		if ((flags & 48) == 32) printf("8580DB-SID ");
		if ((flags & 48) == 48) printf("6581R1-SID ");
		if (flags & 64) printf("YM2149F "); else printf("AY-3-8910 ");
		printf("\nHeader size  :%8d bytes\n", hdrsize);
		printf("1k chunks    :%8d\n", kchunks);
		printf("Last chunk   :%8d bytes\n", lastchunk);
		printf("Total size   :%8d bytes\n", totalsize);
		printf("Loop chunk   :%8d\n", loopchunk);
		printf("Loop byte    :%8d\n", loopbyte);
		printf("Loop ofs     :%8d\n", loopchunk * 1024 + loopbyte);
		printf("Emu speed    :%8d Hz\n", emuspeed);
		printf("Chip speed   :%8d Hz\n", chipspeed);
		char* stringptr = (char*)data + 28;
		printf("Song name    : ");
		printPascalString(stringptr);
		stringptr += *(unsigned char*)stringptr + 1;
		printf("\nAuthor       : ");
		printPascalString(stringptr);
		stringptr += *(unsigned char*)stringptr + 1;
		printf("\nComment      : ");
		printPascalString(stringptr);
		printf("\n\n");
	}

	int load(const char* fname)
	{
		int len;
		char* b = loadFile(fname, len);
		if (b == 0)
			return 0;

		if (((unsigned int*)b)[0] != 'PIHC' && ((unsigned int*)b)[1] != 'ENUT') // 'CHIP' 'TUNE'
		{
			printf("Unknown header in %s\n", fname);
			delete[] b;
			return -1;
		}


		hdrsize = *(unsigned short*)(b + 8);
		chiptype = *(unsigned char*)(b + 10);
		flags = *(unsigned char*)(b + 11);
		kchunks = *(unsigned short*)(b + 12);
		lastchunk = *(unsigned short*)(b + 14);
		loopchunk = *(unsigned short*)(b + 16);
		loopbyte = *(unsigned short*)(b + 18);
		emuspeed = *(unsigned int*)(b + 20);
		chipspeed = *(unsigned int*)(b + 24);
		totalsize = (kchunks - 1) * 1024 + lastchunk;
		data = b;

		return 1;
	}
};

int main(int parc, char ** pars)
{
	int header = 0, line = 0, footer = 0;
	url_encoder_rfc_tables_init();

	char* infilename = 0;

	int badparams = 0;

	for (int i = 1; i < parc; i++)
	{
		if (pars[i][0] == '-' && pars[i][2] == 0)
		{
			if (_stricmp(pars[i], "-h") == 0)
			{
				header = 1;
			}
			else
			if (_stricmp(pars[i], "-l") == 0)
			{
				line = 1;
			}
			else
			if (_stricmp(pars[i], "-f") == 0)
			{
				footer = 1;
			}
			else
			{
				printf("Unexpected parameter %s\n", pars[i]);
				badparams = 1;
			}
		}
		else
		{
			if (infilename == 0)
			{
				infilename = pars[i];
			}
			else
			{
				printf("Unexpected parameter %s\n", pars[i]);
				return 1;
			}
		}
	}

	Zak zak;

	if (!infilename)
	{
		if (header)
		{
			zak.print_header();
			return 0;
		}
		if (footer)
		{
			zak.print_footer();
			return 0;
		}
		badparams = 1;
	}

	if (badparams)
	{
		printf(
			"Zak info by Jari Komppa\n"
			"Usage:\n"
			"%s [options] infilename\n"
			"Options:\n"
			"-h output html header\n"
			"-l output html info line\n"
			"-f output html footer\n\n", pars[0]);
		return 1;
	}

	printf("\n");


	if (!zak.load(infilename))
	{
		printf("Unable to load %s\n", infilename);
		return 1;
	}

	if (header)
		zak.print_header();
	if (line)
		zak.print_html(infilename);
	if (footer)
		zak.print_footer();

	if (!header && !line && !footer)
		zak.print_info(infilename);

	return 0;
}