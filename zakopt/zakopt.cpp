#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

extern "C"
{
#include "zx7.h"
}
#define ZX7DECOMPRESS_IMPLEMENTATION
#include "zx7decompress.h"

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
int force_50hz = 0;

void write_short(unsigned short v)
{
    outbuf[outbuf_idx] = v;
    outbuf_idx++;
}

int write_delay(int delay)
{
    int writes = 0;
    while (delay >= 0x7fff * 2)
    {
        write_short(0x8000);
        writes++;
        delay -= 0x7fff * 2;
    }
    while (delay)
    {
        unsigned short d = 0x7fff;
        if (delay < d)
            d = delay;
        delay -= d;
        d |= 0x8000;
        write_short(d);
        writes++;
    }
    return writes;
}

void printPascalString(char* s)
{
    int len = *(unsigned char*)s;    
    s++;
    printf("\"");
    while (len)
    {
		if (*s > 31 && *s < 126)
			printf("%c", *s);
		else
			printf("?");
        s++;
        len--;
    }
    printf("\"");
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
    int min_delay;
    int changed;

    unsigned char* header;    
    unsigned char* data;
	unsigned char* orig_compressed;
    unsigned int* wide;
    int regwrites;
	int orig_datasize;
    int datasize;
    int origlen;
    int frame_freq;
    unsigned int frame_bins;
    int frame_count;
    int loop_time;    

    Zak() { header = 0; data = 0; orig_compressed = 0; wide = 0; changed = 0; }

    int count_regwrites()
    {
        int count = 0;
        unsigned short* d = (unsigned short*)data;
        for (int i = 0; i < totalsize / 2; i++)
        {
            if (!(d[i] & 0x8000)) count++;
        }
        return count;
    }

    void convert_to_wide()
    {
        int delay = 0;
        int pos = 0;
        delete[] wide;
        regwrites = count_regwrites();
        wide = new unsigned int[regwrites * 2];
        int rw = 0;
        unsigned short* d = (unsigned short*)data;
        for (int i = 0; i < totalsize / 2; i++)
        {
            if (i == (loopchunk * 1024 + loopbyte) / 2)
            {
                loop_time = pos;
            }
            if (d[i] & 0x8000)
            {
                if (d[i] == 0x8000)
                {
                    //printf("Empty delay?\n");
                    delay += 0x7fff * 2;
                }
                else
                {
                    delay += d[i] ^ 0x8000;
                }
            }
            else
            {
                pos += delay;
                wide[rw * 2 + 0] = pos;
                wide[rw * 2 + 1] = d[i];
                rw++;
                delay = 0;
            }
        }
    }

    void convert_from_wide()
    {
        unsigned short* d = (unsigned short*)data;
        int pos = 0;
        totalsize = 0;
        for (int i = 0; i < regwrites; i++)
        {
            while (wide[i * 2 + 0] - pos)
            {
                int t = wide[i * 2 + 0] - pos;
                if (t > 0x7fff)
                    t = 0x7fff;
                *d = t | 0x8000;
                d++;
                totalsize += 2;
                pos += t;
                if (loop_time < pos)
                {
                    loopchunk = (totalsize) / 1024;
                    loopbyte = (totalsize) % 1024;
                    *(unsigned short*)(header + 16) = loopchunk;
                    *(unsigned short*)(header + 18) = loopbyte;
                }
            }
            *d = wide[i * 2 + 1];
            d++;
            totalsize += 2;
        }
    }

    void print_info()
    {
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
        if (!ay)
        {
            if ((flags & 48) == 0) printf("6581-SID ");
            if ((flags & 48) == 16) printf("8580-SID ");
            if ((flags & 48) == 32) printf("8580DB-SID ");
            if ((flags & 48) == 48) printf("6581R1-SID ");
        }
        else
        {
            if (flags & 64) printf("YM2149F "); else printf("AY-3-8910 ");
        }
        printf("\nHeader size  :%8d bytes\n", hdrsize);
        printf("1k chunks    :%8d\n", kchunks);
        printf("Last chunk   :%8d bytes\n", lastchunk);
        printf("Total size   :%8d bytes\n", totalsize);
        printf("Loop chunk   :%8d\n", loopchunk);
        printf("Loop byte    :%8d\n", loopbyte);
        printf("Loop ofs     :%8d\n", loopchunk * 1024 + loopbyte);
        printf("Emu speed    :%8d Hz\n", emuspeed);
        printf("Chip speed   :%8d Hz\n", chipspeed);
        char* stringptr = (char*)header + 28;
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
            printf("Unknown header (not 'CHIPTUNE').\n");
            delete[] b;
            return 0;
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
        ay = 0;

        header = new unsigned char[hdrsize];
        memcpy(header, b, hdrsize);
        data = new unsigned char[len - hdrsize];
        memcpy(data, b + hdrsize, len - hdrsize);
        delete[] b;
        datasize = len - hdrsize;
        origlen = datasize;

#define HDRCHECK(x) if (x) { printf("Input file corrupted:" #x "\n"); return 0;}
		
//		HDRCHECK(totalsize < datasize);
		HDRCHECK(hdrsize < 0);
		HDRCHECK(hdrsize > 2048);
		HDRCHECK(totalsize < 0);
		HDRCHECK(datasize < 0);
		HDRCHECK(kchunks < 0);
		HDRCHECK(lastchunk < 0);
		HDRCHECK(lastchunk > 1024);
		HDRCHECK(loopchunk < 0);
		HDRCHECK(loopbyte < 0);
		HDRCHECK(loopbyte > 1024);
		HDRCHECK(emuspeed < 1);
		HDRCHECK(chipspeed < 1);

        return 1;
    }

    void decompress()
    {
        if (flags & 1)
        {
            printf("- Uncompressed data\n");
            return;
        }
        printf("- Decompressing\n");
        unsigned char* u = new unsigned char[kchunks * 1024];

        int inputofs = 0;
        for (int i = 0; i < kchunks; i++)
        {
            inputofs += zx7_decompress(data + inputofs, u + 1024 * i);
        }
		orig_compressed = data;
		orig_datasize = datasize;
        data = u;
        datasize = (kchunks - 1) * 1024 + lastchunk;
        flags |= 1;
    }

    void compress()
    {
        if ((flags & 1) == 0)
        {
            printf("- Already compressed\n");
            return;
        }

        printf("- Compressing\n");

		// skip compression if nothing changed
		if (orig_compressed)
		{
			printf("- Nothing changed, so we'll use original compressed data.\n");
			delete[] data;
			data = orig_compressed;
			datasize = orig_datasize;
			return;
		}

        Optimal* o;
        unsigned char* cd = 0;
        unsigned char* ob = new unsigned char[kchunks * 2048]; // should be enough in all cases
        int newdatasize = 0;
        size_t sz;
        long dt;
        int i;
        for (i = 0; i < kchunks; i++)
        {
            if (debug)
            {
                printf("- Chunk%4d/%d", i, kchunks);
            }
            else
            {
                printf("\r- Chunk%4d/%d", i, kchunks);
            }
            if (i == 0)
            {
                o = ::optimize(data, 1024, 0);
                cd = ::compress(o, data, 1024, 0, &sz, &dt);
            }
            else
            {
                o = ::optimize(data + ((i - 1) * 1024), 1024 * 2, 1024);
                cd = ::compress(o, data + ((i - 1) * 1024), 1024 * 2, 1024, &sz, &dt);
            }
            if (debug) printf(" %d bytes\n", (int)sz);
            memcpy(ob + datasize, cd, sz);
            newdatasize += sz;
            free(o);
            free(cd);
        }
        if (datasize <= newdatasize)
        {
            printf("\r- Data did not compress, saving as uncompressed\n");
            delete[] ob;
            return;
        }
        datasize = newdatasize;
        delete[] data;
        data = ob;
        changed = 1;
        flags = flags & ~(1); // remove uncompressed flag
        *(unsigned char*)(header + 11) = flags;
    }


    void draw_map()
    {
        int delay = 0;

        printf("- Drawing a map..\n");

        int songlength = 60 * 4;
        int maxpos = (emuspeed / 50) * 50 * songlength;
        int w = 1024;
        int h = 50 * songlength;
        int pixels = w * h;
        unsigned int* map = new unsigned int[pixels];
        for (int i = 0; i < pixels; i++)
        {
            if (i % 32 == 0)
                map[i] = 0xff7f0000;
            else
                map[i] = 0xff000000;
        }
        int pos = 0;

        unsigned short* d = (unsigned short*)data;
        for (int i = 0; i < totalsize / 2; i++)
        {
            if (d[i] & 0x8000)
            {
                if (d[i] == 0x8000)
                {
                    //printf("Empty delay?\n");
                    delay += 0x7fff * 2;
                }
                else
                {
                    delay += d[i] ^ 0x8000;
                }
            }
            else
            {
                pos += delay;
                int x = pos % (emuspeed / 50);
                int y = pos / (emuspeed / 50);
                if (pos < maxpos)
                    map[(x * w / (emuspeed / 50)) + y * w] = 0xff8000ff | (d[i] << 8);
                delay = 0;
            }
        }
        stbi_write_png("zakmap.png", w, h, 4, map, w * 4);
        delete[] map;
    }

    void save(const char* filename)
    {
        FILE* f = fopen(filename, "wb");
        if (!f)
        {
            printf("Unable to save %s\n", filename);
            exit(-1);
        }
        fwrite(header, 1, hdrsize, f);
        fwrite(data, 1, datasize, f);
        fclose(f);
    }

    void print_final_stats()
    {
        printf("\r- %d->%d bytes (%d%%)\n", origlen, datasize, (datasize * 100) / origlen);
    }

    void find_frame_freq()
    {
        // Find frame frequency by binning reg writes and 
        // picking the frequency which uses least bins.
        int best = 50000;
        int best_bins = 32;
        int step = 100;

        // special case: if everything is in the first frame, assume 50hz
        if (wide[(regwrites - 1) * 2] < (unsigned int)(emuspeed / 50))
        {
            printf("- Single-frame song, setting as 50hz\n");
            frame_freq = 5000;
            frame_count = 1;
            frame_bins = 1;
            return;
        }

        do
        {
            best = 50000;
            best_bins = 32;
            if (step == 10) printf("- Trying 1/10th frequencies..\n");
            if (step == 1) printf("- Trying 1/100th frequencies..\n");
            // if the freq is too low, we might fold with a higher one and
            // delay every second frame. If the freq is too high, it'll be
            // (more) impossible to play. Capping off at 100hz for now.
            for (int freq = 2600; freq < 30000; freq += step)
            {
                // skip first 10% of regwrites
                unsigned int bins = 0;
                for (int i = regwrites / 10; i < regwrites; i++)
                {
                    int wp = (wide[i * 2 + 0]) % ((emuspeed * 100) / freq);
                    int bin = (wp * 32) / ((emuspeed * 100) / freq);
                    bins |= 1 << bin;
                }
                int bincount = 0;
                for (int i = 0; i < 32; i++)
                    if (bins & (1 << i))
                        bincount++;
                if (bincount < best_bins)
                {
                    best = freq;
                    best_bins = bincount;
                    frame_bins = bins;
                }
            }
            step /= 10;
        } while (step > 0 && best > 10000);
        printf("- Frame frequency looks like %3.2fhz (%d 1/32 bins)\n", best / 100.0f, best_bins);
        if (best_bins == 32)
        {
            printf("- Unable to find a sane frequency\n");
            exit(-1);
        }
        frame_freq = best;
        frame_count = wide[(regwrites - 1) * 2 + 0] / ((emuspeed *100) / frame_freq);
        printf("- Therefore, we have %d frames\n", frame_count);
    }

    // this is really bad perf-wise if we go long, but
    // fine for the first few frames..
    unsigned int find_frame_bins(int frame)
    {
        unsigned int starttime = frame * ((emuspeed * 100)/ frame_freq);
        unsigned int endtime = starttime + ((emuspeed * 100) / frame_freq);
        unsigned int bins = 0;
        for (int i = 0; i < regwrites; i++)
        {
            if (wide[i * 2 + 0] > endtime)
            {
                return bins;
            }
            if (wide[i * 2 + 0] != 0                // skip collapsed start writes
                && wide[i * 2 + 0] >= starttime
                && wide[i * 2 + 0] < endtime)
            {
                int wp = (wide[i * 2 + 0] - starttime);
                int bin = (wp * 32) / ((emuspeed * 100) / frame_freq);
                bins |= 1 << bin;
            }
        }
        return bins;
    }

    void calculate_song_bins()
    {
        int framebin = 0;
        unsigned int bincount[32];
        for (int i = 0; i < 32; i++)
            bincount[i] = 0;

        unsigned int t = 0;
        for (int i = 0; i < regwrites; i++)
        {
            if (wide[i * 2 + 0] != 0)               // skip collapsed start writes
            {
                int wp = wide[i * 2 + 0] % ((emuspeed * 100) / frame_freq);
                int bin = (wp * 32) / ((emuspeed * 100) / frame_freq);
                bincount[bin]++;
            }
        }
        unsigned int bins = 0;
        for (int i = 0; i < 32; i++)
            if (bincount[i])
                bins |= 1 << i;

        frame_bins = bins;
    }

    void collapse_start()
    {
        int last_initframe = -1;
        unsigned int probable_bins = frame_bins | (frame_bins << 1) | (frame_bins >> 1);        
        if (frame_bins & (1 << 31)) probable_bins |= 1;
        // Look for frames with odd timings for regwrites.
        // We assume these to be init frames.
        for (int i = 0; i < frame_count / 10; i++)
        {
            int fb = find_frame_bins(i);
            if ((fb | probable_bins) != probable_bins)
            {
                last_initframe = i;
            }
        }
        last_initframe++;
        if (last_initframe > 0)
        {
            changed = 1;
            printf("- We seem to have %d init frame(s), collapsing..\n", last_initframe);
            unsigned int endtime = last_initframe * ((emuspeed * 100) / frame_freq);
            for (int i = 0; i < regwrites; i++)
            {
                if (wide[i * 2 + 0] > endtime)
                {
                    return;
                }
                wide[i * 2 + 0] = 0;
            }
        }
        // recalculate bins after collapsing
        calculate_song_bins();
    }

    void remove_empty_frames()
    {
        int count = 0;
        while (count < frame_count && find_frame_bins(count) == 0) count++;
        if (count > 0)
        {
            printf("- Found %d initial empty frames, removing..\n", count);
            int offset = count * ((emuspeed * 100) / frame_freq);
            for (int i = 0; i < regwrites; i++)
            {
                if (wide[i * 2 + 0] > 0)
                    wide[i * 2 + 0] -= offset;
            }
        }
        changed = 1;
    }

    void reduce_emuspeed()
    {
        int target_rate = frame_freq / 100 + ((frame_freq % 100 >= 50)? 1 : 0);
        int divisor = emuspeed / target_rate;

        if (divisor != 1)
        {
            changed = 1;
            printf("- Reducing emu rate to %dhz (divisor %d)\n", target_rate, divisor);
            loop_time /= divisor;
            for (int i = 0; i < regwrites; i++)
            {
                wide[i * 2 + 0] /= divisor;
            }
            emuspeed = target_rate;
            *(unsigned int*)(header + 20) = emuspeed;
        }
    }

#define NUKE_FROM_ORBIT 0x10000

    void optimize()
    {        
        // find shadowed writes
        int t = 0;
        int regs[256];
        int shadowed = 0;
        for (int i = regwrites; i >= 0; i--)
        {
            if (wide[i * 2 + 0] != t)
            {
                t = wide[i * 2 + 0];
                for (int j = 0; j < 256; j++)
                    regs[j] = 0;
            }
            if (regs[(wide[i * 2 + 1] >> 8) & 0xff])
            {
                wide[i * 2 + 1] = NUKE_FROM_ORBIT;// mark for deletion
                shadowed++;
            }
            else
            {
                regs[(wide[i * 2 + 1] >> 8) & 0xff] = 1;
            }
        }
        printf("- %d shadowed writes found (regs written more than once in a trigger)\n", shadowed);
        // find unnecessary writes
        for (int j = 0; j < 256; j++)
            regs[j] = -1;
        int unnecessary = 0;
        for (int i = 0; i < regwrites; i++)
        {
            if (wide[i * 2 + 1] != NUKE_FROM_ORBIT)
            {
                int reg = (wide[i * 2 + 1] >> 8) & 0xff;
                int val = (wide[i * 2 + 1] >> 0) & 0xff;
                if (regs[reg] == val) // if unchanged..
                {
                    if (!(ay && ((reg & 0xf) == 13))) // ..and we're not ay & reg 13
                    {
                        wide[i * 2 + 1] = NUKE_FROM_ORBIT;// mark for deletion
                        unnecessary++;
                    }
                }
                regs[reg] = val;
            }
        }
        printf("- %d unnecessary writes found (register already held value)\n", unnecessary);
        if (unnecessary || shadowed)
        {
            int nregwrites = regwrites - (unnecessary + shadowed);
            printf("- Total of %d regwrites removed\n", regwrites - nregwrites);
            unsigned int* nwide = new unsigned int[nregwrites * 2];
            int rw = 0;
            for (int i = 0; i < regwrites; i++)
            {
                if (wide[i * 2 + 1] != NUKE_FROM_ORBIT)
                {
                    nwide[rw * 2 + 0] = wide[i * 2 + 0];
                    nwide[rw * 2 + 1] = wide[i * 2 + 1];
                    rw++;
                }
            }
            delete[] wide;
            wide = nwide;
            regwrites = nregwrites;
            changed = 1;
        }
    }
};

int main(int parc, char ** pars)
{
    int compressed = 1;
    printf("Zak optimizer by Jari Komppa\n");

    char* infilename = 0;
    char* outfilename = 0;

    int badparams = 0;
	int skip_opt = 0;
	int just_hdrcheck = 0;
    int just_map = 0;
    int postmap = 0;

    for (int i = 1; i < parc; i++)
    {
        if (pars[i][0] == '-' && pars[i][2] == 0)
        {
            if (_stricmp(pars[i], "-u") == 0)
            {
                compressed = 0;
            }
            else
            if (_stricmp(pars[i], "-d") == 0)
            {
                debug = 1;
            }
            else
            if (_stricmp(pars[i], "-o") == 0)
            {
                skip_opt = 1;
            }
            else
            if (_stricmp(pars[i], "-s") == 0)
            {
                just_hdrcheck = 1;
            }
            else
            if (_stricmp(pars[i], "-m") == 0)
            {
                just_map = 1;
            }
            else
            if (_stricmp(pars[i], "-p") == 0)
            {
                postmap = 1;
            }
            else
            if (_stricmp(pars[i], "-5") == 0)
            {
                force_50hz = 1;
            }
            else
            {
                badparams = 1;
            }
        }
        else
        {
            if (infilename == 0)
            {
                infilename = pars[i];
            } else
                if (outfilename == 0)
                {
                    outfilename = pars[i];
                }
                else
                {
                    printf("Unexpected parameter %s\n", pars[i]);
                    return 1;
                }
        }
    }

    if (outfilename == 0)
    {
        badparams = 1;
    }

    if (badparams && just_hdrcheck == 0 && just_map == 0)
    {
        printf(
            "Usage:\n"
            "%s [options] infilename outfilename\n"
            "Options:\n"
			"-o don't optimize, just (de)compress\n"
			"-5 do your best to make this 50hz, even if it breaks\n"
			"-s just check if file is corrupt, then return\n"
            "-u output uncompressed song data\n"
            "-m output png file with regwrite/delay map\n"
            "-p output png file with regwrite/delay map after optimization\n"
            "-d print even more diagnostic info\n\n", pars[0]);
        return 1;
    }

    printf("\n");

    Zak zak;

    if (!zak.load(infilename))
    {
        printf("Unable to load %s\n", infilename);
        return 1;
    }
	if (just_hdrcheck)
		return 0;
 
	printf("Input        : %s\n", infilename);
	printf("Output       : %s\n", outfilename);

    zak.print_info();

    zak.decompress();

 
    if (just_map)
    {
        zak.draw_map();
        return 0;
    }

	if (!skip_opt)
	{
        // convert to wide format for easier handling
        zak.convert_to_wide();
 
        if (zak.emuspeed < 16 * 75)
        {
            printf("- Emuspeed already low, assuming frame freq, not cpu freq\n");
            zak.frame_freq = zak.emuspeed;
        }
        else
        {
            // find frequency
            zak.find_frame_freq();
            // handle first frame(s)
            zak.collapse_start();
            // change emuspeed
            zak.reduce_emuspeed();
        }
        // remove initial empty frames
        zak.remove_empty_frames();
        // eliminate shadowed and unchanged writes
        zak.optimize();
        
        zak.convert_from_wide();
	}

    if (postmap)
    {
        zak.draw_map();
        return 0;
    }

    if (compressed)
        zak.compress();

    if (!zak.changed && _stricmp(infilename, outfilename) == 0)
    {
        printf("- No changes, input and output are the same, not saving.\n");
        return 0;
    }

    zak.save(outfilename);

    zak.print_final_stats();
 
    return 0;
}