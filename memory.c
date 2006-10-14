 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Memory management
  *
  * (c) 1995 Bernd Schmidt
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "memory.h"
#include "ersatz.h"
#include "zfile.h"
#include "custom.h"
#include "events.h"
#include "newcpu.h"
#include "autoconf.h"
#include "savestate.h"
#include "ar.h"
#include "crc32.h"
#include "gui.h"
#include "cdtv.h"
#include "akiko.h"
#include "arcadia.h"
#include "enforcer.h"

int canbang;
#ifdef JIT
/* Set by each memory handler that does not simply access real memory. */
int special_mem;
#endif

int ersatzkickfile;

uae_u32 allocated_chipmem;
uae_u32 allocated_fastmem;
uae_u32 allocated_bogomem;
uae_u32 allocated_gfxmem;
uae_u32 allocated_z3fastmem;
uae_u32 allocated_a3000mem;

#if defined(CPU_64_BIT)
uae_u32 max_z3fastmem = 2048UL * 1024 * 1024;
#else
uae_u32 max_z3fastmem = 512 * 1024 * 1024;
#endif

static size_t chip_filepos;
static size_t bogo_filepos;
static size_t rom_filepos;

static struct romlist *rl;
static int romlist_cnt;

void romlist_add (char *path, struct romdata *rd)
{
    struct romlist *rl2;

    romlist_cnt++;
    rl = realloc (rl, sizeof (struct romlist) * romlist_cnt);
    rl2 = rl + romlist_cnt - 1;
    rl2->path = my_strdup (path);
    rl2->rd = rd;
}

char *romlist_get (struct romdata *rd)
{
    int i;

    if (!rd)
	return 0;
    for (i = 0; i < romlist_cnt; i++) {
	if (rl[i].rd == rd)
	    return rl[i].path;
    }
    return 0;
}

void romlist_clear (void)
{
    xfree (rl);
    rl = 0;
    romlist_cnt = 0;
}

static struct romdata roms[] = {
    { "Cloanto Amiga Forever ROM key", 0, 0, 0, 0, 0, 0x869ae1b1, 2069, 0, 0, 1, ROMTYPE_KEY },
    { "Cloanto Amiga Forever 2006 ROM key", 0, 0, 0, 0, 0, 0xb01c4b56, 750, 48, 0, 1, ROMTYPE_KEY },

    { "KS ROM v1.0 (A1000)(NTSC)", 1, 0, 1, 0, "A1000\0", 0x299790ff, 262144, 1, 0, 0, ROMTYPE_KICK },
    { "KS ROM v1.1 (A1000)(NTSC)", 1, 1, 31, 34, "A1000\0", 0xd060572a, 262144, 2, 0, 0, ROMTYPE_KICK },
    { "KS ROM v1.1 (A1000)(PAL)", 1, 1, 31, 34, "A1000\0", 0xec86dae2, 262144, 3, 0, 0, ROMTYPE_KICK },
    { "KS ROM v1.2 (A1000)", 1, 2, 33, 166, "A1000\0", 0x9ed783d0, 262144, 4, 0, 0, ROMTYPE_KICK },
    { "KS ROM v1.2 (A500,A1000,A2000)", 1, 2, 33, 180, "A500\0A1000\0A2000\0", 0xa6ce1636, 262144, 5, 0, 0, ROMTYPE_KICK },
    { "KS ROM v1.3 (A500,A1000,A2000)", 1, 3, 34, 5, "A500\0A1000\0A2000\0", 0xc4f0f55f, 262144, 6, 60, 0, ROMTYPE_KICK },
    { "KS ROM v1.3 (A3000)", 1, 3, 34, 5, "A3000\0", 0xe0f37258, 262144, 32, 0, 0, ROMTYPE_KICK },

    { "KS ROM v2.04 (A500+)", 2, 4, 37, 175, "A500+\0", 0xc3bdb240, 524288, 7, 0, 0, ROMTYPE_KICK },
    { "KS ROM v2.05 (A600)", 2, 5, 37, 299, "A600\0", 0x83028fb5, 524288, 8, 0, 0, ROMTYPE_KICK },
    { "KS ROM v2.05 (A600HD)", 2, 5, 37, 300, "A600HD\0A600\0", 0x64466c2a, 524288, 9, 0, 0, ROMTYPE_KICK },
    { "KS ROM v2.05 (A600HD)", 2, 5, 37, 350, "A600HD\0A600\0", 0x43b0df7b, 524288, 10, 0, 0, ROMTYPE_KICK },

    { "KS ROM v3.0 (A1200)", 3, 0, 39, 106, "A1200\0", 0x6c9b07d2, 524288, 11, 0, 0, ROMTYPE_KICK },
    { "KS ROM v3.0 (A4000)", 3, 0, 39, 106, "A4000\0", 0x9e6ac152, 524288, 12, 2, 0, ROMTYPE_KICK },
    { "KS ROM v3.1 (A4000)", 3, 1, 40, 70, "A4000\0", 0x2b4566f1, 524288, 13, 2, 0, ROMTYPE_KICK },
    { "KS ROM v3.1 (A500,A600,A2000)", 3, 1, 40, 63, "A500\0A600\0A2000\0", 0xfc24ae0d, 524288, 14, 0, 0, ROMTYPE_KICK },
    { "KS ROM v3.1 (A1200)", 3, 1, 40, 68, "A1200\0", 0x1483a091, 524288, 15, 1, 0, ROMTYPE_KICK },
    { "KS ROM v3.1 (A4000)(Cloanto)", 3, 1, 40, 68, "A4000\0", 0x43b6dd22, 524288, 31, 2, 1, ROMTYPE_KICK },
    { "KS ROM v3.1 (A4000)", 3, 1, 40, 68, "A4000\0", 0xd6bae334, 524288, 16, 2, 0, ROMTYPE_KICK },
    { "KS ROM v3.1 (A4000T)", 3, 1, 40, 70, "A4000T\0", 0x75932c3a, 524288, 17, 2, 0, ROMTYPE_KICK },
    { "KS ROM v3.X (A4000)(Cloanto)", 3, 10, 45, 57, "A4000\0", 0x08b69382, 524288, 46, 2, 0, ROMTYPE_KICK },

    { "CD32 KS ROM v3.1", 3, 1, 40, 60, "CD32\0", 0x1e62d4a5, 524288, 18, 1, 0, ROMTYPE_KICKCD32 },
    { "CD32 extended ROM", 3, 1, 40, 60, "CD32\0", 0x87746be2, 524288, 19, 1, 0, ROMTYPE_EXTCD32 },

    { "CDTV extended ROM v1.00", 1, 0, 1, 0, "CDTV\0", 0x42baa124, 262144, 20, 0, 0, ROMTYPE_EXTCDTV },
    { "CDTV extended ROM v2.30", 2, 30, 2, 30, "CDTV\0", 0x30b54232, 262144, 21, 0, 0, ROMTYPE_EXTCDTV },
    { "CDTV extended ROM v2.07", 2, 7, 2, 7, "CDTV\0", 0xceae68d2, 262144, 22, 0, 0, ROMTYPE_EXTCDTV },

    { "A1000 bootstrap ROM", 0, 0, 0, 0, "A1000\0", 0x62f11c04, 8192, 23, 0, 0, ROMTYPE_KICK },
    { "A1000 bootstrap ROM", 0, 0, 0, 0, "A1000\0", 0x0b1ad2d0, 65536, 24, 0, 0, ROMTYPE_KICK },

    { "Action Replay Mk I v1.50", 1, 50, 1, 50, "AR\0", 0xd4ce0675, 65536, 25, 0, 0, ROMTYPE_AR },
    { "Action Replay Mk II v2.05", 2, 5, 2, 5, "AR\0", 0x1287301f , 131072, 26, 0, 0, ROMTYPE_AR },
    { "Action Replay Mk II v2.12", 2, 12, 2, 12, "AR\0", 0x804d0361 , 131072, 27, 0, 0, ROMTYPE_AR },
    { "Action Replay Mk II v2.14", 2, 14, 2, 14, "AR\0", 0x49650e4f, 131072, 28, 0, 0, ROMTYPE_AR },
    { "Action Replay Mk III v3.09", 3, 9, 3, 9, "AR\0", 0x0ed9b5aa, 262144, 29, 0, 0, ROMTYPE_AR },
    { "Action Replay Mk III v3.17", 3, 17, 3, 17, "AR\0", 0xc8a16406, 262144, 30, 0, 0, ROMTYPE_AR },
    { "Action Replay 1200", 0, 0, 0, 0, "AR\0", 0x8d760101, 262144, 47, 0, 0, ROMTYPE_AR },

    { "Arcadia SportTime Table Hockey\0ar_airh", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 33, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia SportTime Bowling\0ar_bowl", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 34, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia World Darts\0ar_dart", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 35, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia Magic Johnson's Fast Break\0ar_fast", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 36, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia Leader Board Golf\0ar_ldrb", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 37, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia Leader Board Golf (alt)\0ar_ldrba", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 38, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia Ninja Mission\0ar_ninj", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 39, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia Road Wars\0ar_rdwr", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 40, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia Sidewinder\0ar_sdwr", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 41, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia Cool Spot\0ar_spot", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 42, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia Space Ranger\0ar_sprg", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 43, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia Xenon\0ar_xeon", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 44, 0, 0, ROMTYPE_ARCADIA },
    { "Arcadia World Trophy Soccer\0ar_socc", 0, 0, 0, 0, "ARCADIA\0", 0, 0, 45, 0, 0, ROMTYPE_ARCADIA },

    { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

};

struct romlist **getrombyident(int ver, int rev, int subver, int subrev, char *model, int all)
{
    int i, j, ok, out, max;
    struct romdata *rd;
    struct romlist **rdout, *rltmp;
    void *buf;
    static struct romlist rlstatic;
    
    for (i = 0; roms[i].name; i++);
    if (all)
	max = i;
    else
	max = romlist_cnt;
    buf = xmalloc((sizeof (struct romlist*) + sizeof (struct romlist)) * (i + 1));
    rdout = buf;
    rltmp = (struct romlist*)((uae_u8*)buf + (i + 1) * sizeof (struct romlist*));
    out = 0;
    for (i = 0; i < max; i++) {
        ok = 0;
	if (!all)
	    rd = rl[i].rd;
	else
	    rd = &roms[i];
	if (model && !strcmpi(model, rd->name))
	    ok = 2;
	if (rd->ver == ver && (rev < 0 || rd->rev == rev)) {
	    if (subver >= 0) {
		if (rd->subver == subver && (subrev < 0 || rd->subrev == subrev) && rd->subver > 0)
		    ok = 1;
	    } else {
		ok = 1;
	    }
	}
	if (!ok)
	    continue;
	if (model && ok < 2) {
	    char *p = rd->model;
	    ok = 0;
	    while (*p) {
		if (!strcmp(rd->model, model)) {
		    ok = 1;
		    break;
		}
		p = p + strlen(p) + 1;
	    }
	}
	if (!model && rd->type != ROMTYPE_KICK)
	    ok = 0;
	if (ok) {
	    if (all) {
		rdout[out++] = rltmp;
		rltmp->path = NULL;
		rltmp->rd = rd;
		rltmp++;
	    } else {
		rdout[out++] = &rl[i];
	    }
	}
    }
    if (out == 0) {
	xfree (rdout);
	return NULL;
    }
    for (i = 0; i < out; i++) {
	int v1 = rdout[i]->rd->subver * 1000 + rdout[i]->rd->subrev;
	for (j = i + 1; j < out; j++) {
	    int v2 = rdout[j]->rd->subver * 1000 + rdout[j]->rd->subrev;
	    if (v1 < v2) {
		struct romlist *rltmp = rdout[j];
		rdout[j] = rdout[i];
		rdout[i] = rltmp;
	    }
	}
    }
    rdout[out] = NULL;
    return rdout;
}

struct romdata *getarcadiarombyname (char *name)
{
    int i;
    for (i = 0; roms[i].name; i++) {
	if (roms[i].type == ROMTYPE_ARCADIA) {
	    char *p = roms[i].name;
	    p = p + strlen (p) + 1;
	    if (strlen (name) >= strlen (p) + 4) {
		char *p2 = name + strlen (name) - strlen (p) - 4;
		if (!memcmp (p, p2, strlen (p)) && !memcmp (p2 + strlen (p2) - 4, ".zip", 4))
		    return &roms[i];
	    }
	}
    }
    return NULL;
}

static int kickstart_checksum_do (uae_u8 *mem, int size)
{
    uae_u32 cksum = 0, prevck = 0;
    int i;
    for (i = 0; i < size; i+=4) {
	uae_u32 data = mem[i]*65536*256 + mem[i+1]*65536 + mem[i+2]*256 + mem[i+3];
	cksum += data;
	if (cksum < prevck)
	    cksum++;
	prevck = cksum;
    }
    return cksum == 0xffffffff;
}

#define ROM_KEY_NUM 3
struct rom_key {
    uae_u8 *key;
    int size;
};

static struct rom_key keyring[ROM_KEY_NUM];

int decode_cloanto_rom_do (uae_u8 *mem, int size, int real_size)
{
    int cnt, t, i, keysize;
    uae_u8 *key;

    for (i = ROM_KEY_NUM - 1; i >= 0; i--) {
        keysize = keyring[i].size;
	key = keyring[i].key;
	if (!key)
	    continue;
	for (t = cnt = 0; cnt < size; cnt++, t = (t + 1) % keysize)  {
	    mem[cnt] ^= key[t];
	    if (real_size == cnt + 1)
		t = keysize - 1;
	}
	if ((mem[2] == 0x4e && mem[3] == 0xf9) || (mem[0] == 0x11 && (mem[1] == 0x11 || mem[1] == 0x14)))
	    return 1;
	for (t = cnt = 0; cnt < size; cnt++, t = (t + 1) % keysize)  {
	    mem[cnt] ^= key[t];
	    if (real_size == cnt + 1)
		t = keysize - 1;
	}
    }
    return get_keyring();
}

static void addkey(int *pkeyid, uae_u8 *key, int size, const char *name)
{
    int keyid = *pkeyid;
    int i;

    if (key == NULL || size == 0 || keyid >= ROM_KEY_NUM) {
	xfree (key);
	return;
    }
    for (i = 0; i < keyid; i++) {
	if (keyring[i].key && keyring[i].size == size && !memcmp (keyring[i].key, key, size)) {
	    xfree (key);
	    return;
	}
    }
    keyring[keyid].key = key;
    keyring[keyid++].size = size;
    write_log ("ROM KEY '%s' %d bytes loaded\n", name, size);
    *pkeyid = keyid;
}

int get_keyring (void)
{
    int i, num = 0;
    for (i = 0; i < ROM_KEY_NUM; i++) {
	if (keyring[i].key)
	    num++;
    }
    return num;
}

int load_keyring (struct uae_prefs *p, char *path)
{
    struct zfile *f;
    uae_u8 *keybuf;
    int keysize;
    char tmp[MAX_PATH], *d;
    int keyids[] = { 0, 48, -1 };
    int keyid;
    int cnt, i;

    free_keyring();
    keyid = 0;
    keybuf = target_load_keyfile(p, path, &keysize, tmp);
    addkey(&keyid, keybuf, keysize, tmp);
    for (i = 0; keyids[i] >= 0 && keyid < ROM_KEY_NUM; i++) {
        struct romdata *rd = getromdatabyid (keyids[i]);
	char *s;
	if (rd) {
	    s = romlist_get (rd);
	    if (s) {
		f = zfile_fopen (s, "rb");
		if (f) {
		    zfile_fseek (f, 0, SEEK_END);
		    keysize = zfile_ftell (f);
		    if (keysize > 0) {
			zfile_fseek (f, 0, SEEK_SET);
			keybuf = xmalloc (keysize);
			zfile_fread (keybuf, 1, keysize, f);
			addkey(&keyid, keybuf, keysize, s);
		    }
		    zfile_fclose (f);
		}
	    }
	}
    }
    
    cnt = 0;
    for (;;) {
        keybuf = NULL;
	keysize = 0;
	tmp[0] = 0;
	switch (cnt)
	{
	case 0:
	if (path)
	    strcpy (tmp, path);
	break;
	case 1:
	    strcat (tmp, "rom.key");
	break;
	case 2:
	    if (p) {
		strcpy (tmp, p->path_rom);
		strcat (tmp, "rom.key");
	    }
	break;
	case 3:
	    strcpy (tmp, "roms/rom.key");
	break;
	case 4:
	    strcpy (tmp, start_path_data);
	    strcat (tmp, "rom.key");
	break;
	case 5:
	    sprintf (tmp, "%s../shared/rom/rom.key", start_path_data);
	break;
	case 6:
	    if (p) {
		for (i = 0; uae_archive_extensions[i]; i++) {
		    if (strstr(p->romfile, uae_archive_extensions[i]))
			break;
		}
		if (!uae_archive_extensions[i]) {
		    strcpy (tmp, p->romfile);
		    d = strrchr(tmp, '/');
		    if (!d)
			d = strrchr(tmp, '\\');
		    if (d)
			strcpy (d + 1, "rom.key");
		}
	    }
	break;
	case 7:
	return keyid;
	}
	cnt++;
	if (!tmp[0])
	    continue;
	f = zfile_fopen(tmp, "rb");
	if (!f)
	    continue;
	zfile_fseek (f, 0, SEEK_END);
	keysize = zfile_ftell (f);
	if (keysize > 0) {
	    zfile_fseek (f, 0, SEEK_SET);
	    keybuf = xmalloc (keysize);
	    zfile_fread (keybuf, 1, keysize, f);
	    addkey (&keyid, keybuf, keysize, tmp);
	}
	zfile_fclose (f);
    }
}
void free_keyring (void)
{
    int i;
    for (i = 0; i < ROM_KEY_NUM; i++)
	xfree (keyring[i].key);
    memset(keyring, 0, sizeof (struct rom_key) * ROM_KEY_NUM);
}

static int decode_cloanto_rom (uae_u8 *mem, int size, int real_size)
{
    if (!decode_cloanto_rom_do (mem, size, real_size)) {
	#ifndef	SINGLEFILE
	notify_user (NUMSG_NOROMKEY);
	#endif
	return 0;
    }
    return 1;
}

struct romdata *getromdatabyname (char *name)
{
    char tmp[MAX_PATH];
    int i = 0;
    while (roms[i].name) {
	getromname (&roms[i], tmp);
	if (!strcmp (tmp, name) || !strcmp (roms[i].name, name))
	    return &roms[i];
	i++;
    }
    return 0;
}

struct romdata *getromdatabyid (int id)
{
    int i = 0;
    while (roms[i].name) {
	if (id == roms[i].id)
	    return &roms[i];
	i++;
    }
    return 0;
}

struct romdata *getromdatabycrc (uae_u32 crc32)
{
    int i = 0;
    while (roms[i].name) {
	if (crc32 == roms[i].crc32)
	    return &roms[i];
	i++;
    }
    return 0;
}

struct romdata *getromdatabydata (uae_u8 *rom, int size)
{
    int i;
    uae_u32 crc32a, crc32b, crc32c;
    uae_u8 tmp[4];
    uae_u8 *tmpbuf = NULL;

    if (size > 11 && !memcmp (rom, "AMIROMTYPE1", 11)) {
	uae_u8 *tmpbuf = xmalloc (size);
	int tmpsize = size - 11;
	memcpy (tmpbuf, rom + 11, tmpsize);
	decode_cloanto_rom (tmpbuf, tmpsize, tmpsize);
	rom = tmpbuf;
	size = tmpsize;
    }
    crc32a = get_crc32 (rom, size);
    crc32b = get_crc32 (rom, size / 2);
     /* ignore AR IO-port range until we have full dump */
    memcpy (tmp, rom, 4);
    memset (rom, 0, 4);
    crc32c = get_crc32 (rom, size);
    memcpy (rom, tmp, 4);
    i = 0;
    while (roms[i].name) {
	if (roms[i].crc32) {
	    if (crc32a == roms[i].crc32 || crc32b == roms[i].crc32)
		return &roms[i];
	    if (crc32c == roms[i].crc32 && roms[i].type == ROMTYPE_AR)
		return &roms[i];
	}
	i++;
    }
    xfree (tmpbuf);
    return 0;
}

struct romdata *getromdatabyzfile (struct zfile *f)
{
    int pos, size;
    uae_u8 *p;
    struct romdata *rd;

    pos = zfile_ftell (f);
    zfile_fseek (f, 0, SEEK_END);
    size = zfile_ftell (f);
    p = xmalloc (size);
    if (!p)
	return 0;
    memset (p, 0, size);
    zfile_fseek (f, 0, SEEK_SET);
    zfile_fread (p, 1, size, f);
    zfile_fseek (f, pos, SEEK_SET);
    rd = getromdatabydata (p, size);
    xfree (p);
    return rd;
}

void getromname	(struct romdata *rd, char *name)
{
    name[0] = 0;
    strcat (name, rd->name);
    if (rd->subrev && rd->subrev != rd->rev)
	sprintf (name + strlen (name), " rev %d.%d", rd->subver, rd->subrev);
    if (rd->size > 0)
	sprintf (name + strlen (name), " (%dk)", (rd->size + 1023) / 1024);
}

addrbank *mem_banks[MEMORY_BANKS];

/* This has two functions. It either holds a host address that, when added
   to the 68k address, gives the host address corresponding to that 68k
   address (in which case the value in this array is even), OR it holds the
   same value as mem_banks, for those banks that have baseaddr==0. In that
   case, bit 0 is set (the memory access routines will take care of it).  */

uae_u8 *baseaddr[MEMORY_BANKS];

#ifdef NO_INLINE_MEMORY_ACCESS
__inline__ uae_u32 longget (uaecptr addr)
{
    return call_mem_get_func (get_mem_bank (addr).lget, addr);
}
__inline__ uae_u32 wordget (uaecptr addr)
{
    return call_mem_get_func (get_mem_bank (addr).wget, addr);
}
__inline__ uae_u32 byteget (uaecptr addr)
{
    return call_mem_get_func (get_mem_bank (addr).bget, addr);
}
__inline__ void longput (uaecptr addr, uae_u32 l)
{
    call_mem_put_func (get_mem_bank (addr).lput, addr, l);
}
__inline__ void wordput (uaecptr addr, uae_u32 w)
{
    call_mem_put_func (get_mem_bank (addr).wput, addr, w);
}
__inline__ void byteput (uaecptr addr, uae_u32 b)
{
    call_mem_put_func (get_mem_bank (addr).bput, addr, b);
}
#endif

uae_u32	chipmem_mask, chipmem_full_mask;
uae_u32 kickmem_mask, extendedkickmem_mask, bogomem_mask, a3000mem_mask;

static int illegal_count;
/* A dummy bank that only contains zeros */

static uae_u32 REGPARAM3 dummy_lget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 dummy_wget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 dummy_bget (uaecptr) REGPARAM;
static void REGPARAM3 dummy_lput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 dummy_wput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 dummy_bput (uaecptr, uae_u32) REGPARAM;
static int REGPARAM3 dummy_check (uaecptr addr, uae_u32 size) REGPARAM;

#define	MAX_ILG 200
#define NONEXISTINGDATA 0
//#define NONEXISTINGDATA 0xffffffff

static void dummylog(int rw, uaecptr addr, int size, uae_u32 val)
{
    if (illegal_count >= MAX_ILG)
	return;
    /* ignore Zorro3 expansion space */
    if (addr >= 0xff000000 && addr <= 0xff000200)
	return;
    /* extended rom */
    if (addr >= 0xf00000 && addr <= 0xf7ffff)
	return;
    if (MAX_ILG >= 0)
	illegal_count++;
    if (rw) {
	write_log ("Illegal %cput at %08lx=%08lx PC=%x\n",
	    size == 1 ? 'b' : size == 2 ? 'w' : 'l', addr, val, M68K_GETPC);
    } else {
	write_log ("Illegal %cget at %08lx PC=%x\n",
	    size == 1 ? 'b' : size == 2 ? 'w' : 'l', addr, M68K_GETPC);
    }
}

uae_u32 REGPARAM2 dummy_lget (uaecptr addr)
{
#ifdef JIT
    special_mem |= S_READ;
#endif
    if (currprefs.illegal_mem)
	dummylog(0, addr, 4, 0);
    if (currprefs.cpu_level >= 2)
	return NONEXISTINGDATA;
    return (regs.irc << 16) | regs.irc;
}

uae_u32 REGPARAM2 dummy_wget (uaecptr addr)
{
#ifdef JIT
    special_mem |= S_READ;
#endif
    if (currprefs.illegal_mem)
	dummylog(0, addr, 2, 0);
    if (currprefs.cpu_level >= 2)
	return NONEXISTINGDATA;
    return regs.irc;
}

uae_u32 REGPARAM2 dummy_bget (uaecptr addr)
{
#ifdef JIT
    special_mem |= S_READ;
#endif
    if (currprefs.illegal_mem)
	dummylog(0, addr, 1, 0);
    if (currprefs.cpu_level >= 2)
	return NONEXISTINGDATA;
    return (addr & 1) ? regs.irc : regs.irc >> 8;
}

void REGPARAM2 dummy_lput (uaecptr addr, uae_u32 l)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
   if (currprefs.illegal_mem)
       dummylog(1, addr, 4, l);
}
void REGPARAM2 dummy_wput (uaecptr addr, uae_u32 w)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
   if (currprefs.illegal_mem)
       dummylog(1, addr, 2, w);
}
void REGPARAM2 dummy_bput (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
   if (currprefs.illegal_mem)
       dummylog(1, addr, 1, b);
}

int REGPARAM2 dummy_check (uaecptr addr, uae_u32 size)
{
#ifdef JIT
    special_mem |= S_READ;
#endif
    if (currprefs.illegal_mem) {
	if (illegal_count < MAX_ILG || MAX_ILG < 0) {
	    if (MAX_ILG >= 0)
		illegal_count++;
	    write_log ("Illegal check at %08lx PC=%x\n", addr, M68K_GETPC);
	}
    }

    return 0;
}

#ifdef AUTOCONFIG

/* A3000 "motherboard resources" bank. */
static uae_u32 REGPARAM3 mbres_lget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 mbres_wget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 mbres_bget (uaecptr) REGPARAM;
static void REGPARAM3 mbres_lput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 mbres_wput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 mbres_bput (uaecptr, uae_u32) REGPARAM;
static int REGPARAM3 mbres_check (uaecptr addr, uae_u32 size) REGPARAM;

static int mbres_val = 0;

uae_u32 REGPARAM2 mbres_lget (uaecptr addr)
{
#ifdef JIT
    special_mem |= S_READ;
#endif
    if (currprefs.illegal_mem)
	write_log ("Illegal lget at %08lx\n", addr);

    return 0;
}

uae_u32 REGPARAM2 mbres_wget (uaecptr addr)
{
#ifdef JIT
    special_mem |= S_READ;
#endif
    if (currprefs.illegal_mem)
	write_log ("Illegal wget at %08lx\n", addr);

    return 0;
}

uae_u32 REGPARAM2 mbres_bget (uaecptr addr)
{
#ifdef JIT
    special_mem |= S_READ;
#endif
    if (currprefs.illegal_mem)
	write_log ("Illegal bget at %08lx\n", addr);

    return (addr & 0xFFFF) == 3 ? mbres_val : 0;
}

void REGPARAM2 mbres_lput (uaecptr addr, uae_u32 l)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    if (currprefs.illegal_mem)
	write_log ("Illegal lput at %08lx\n", addr);
}
void REGPARAM2 mbres_wput (uaecptr addr, uae_u32 w)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    if (currprefs.illegal_mem)
	write_log ("Illegal wput at %08lx\n", addr);
}
void REGPARAM2 mbres_bput (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    if (currprefs.illegal_mem)
	write_log ("Illegal bput at %08lx\n", addr);

    if ((addr & 0xFFFF) == 3)
	mbres_val = b;
}

int REGPARAM2 mbres_check (uaecptr addr, uae_u32 size)
{
    if (currprefs.illegal_mem)
	write_log ("Illegal check at %08lx\n", addr);

    return 0;
}

#endif

/* Chip memory */

uae_u8 *chipmemory;

static int REGPARAM3 chipmem_check (uaecptr addr, uae_u32 size) REGPARAM;
static uae_u8 *REGPARAM3 chipmem_xlate (uaecptr addr) REGPARAM;

#ifdef AGA

/* AGA ce-chipram access */

static void ce2_timeout (void)
{
    wait_cpu_cycle_read (0, -1);
}

uae_u32 REGPARAM2 chipmem_lget_ce2 (uaecptr addr)
{
    uae_u32 *m;

#ifdef JIT
    special_mem |= S_READ;
#endif
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u32 *)(chipmemory + addr);
    ce2_timeout ();
    return do_get_mem_long (m);
}

uae_u32 REGPARAM2 chipmem_wget_ce2 (uaecptr addr)
{
    uae_u16 *m;

#ifdef JIT
    special_mem |= S_READ;
#endif
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u16 *)(chipmemory + addr);
    ce2_timeout ();
    return do_get_mem_word (m);
}

uae_u32 REGPARAM2 chipmem_bget_ce2 (uaecptr addr)
{
#ifdef JIT
    special_mem |= S_READ;
#endif
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    ce2_timeout ();
    return chipmemory[addr];
}

void REGPARAM2 chipmem_lput_ce2 (uaecptr addr, uae_u32 l)
{
    uae_u32 *m;

#ifdef JIT
    special_mem |= S_WRITE;
#endif
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u32 *)(chipmemory + addr);
    ce2_timeout ();
    do_put_mem_long (m, l);
}

void REGPARAM2 chipmem_wput_ce2 (uaecptr addr, uae_u32 w)
{
    uae_u16 *m;

#ifdef JIT
    special_mem |= S_WRITE;
#endif
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u16 *)(chipmemory + addr);
    ce2_timeout ();
    do_put_mem_word (m, w);
}

void REGPARAM2 chipmem_bput_ce2 (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    ce2_timeout ();
    chipmemory[addr] = b;
}

#endif

uae_u32 REGPARAM2 chipmem_lget (uaecptr addr)
{
    uae_u32 *m;

    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u32 *)(chipmemory + addr);
    return do_get_mem_long (m);
}

uae_u32 REGPARAM2 chipmem_wget (uaecptr addr)
{
    uae_u16 *m;

    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u16 *)(chipmemory + addr);
    return do_get_mem_word (m);
}

uae_u32 REGPARAM2 chipmem_bget (uaecptr addr)
{
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    return chipmemory[addr];
}

void REGPARAM2 chipmem_lput (uaecptr addr, uae_u32 l)
{
    uae_u32 *m;

    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u32 *)(chipmemory + addr);
    do_put_mem_long (m, l);
}

void REGPARAM2 chipmem_wput (uaecptr addr, uae_u32 w)
{
    uae_u16 *m;

    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    m = (uae_u16 *)(chipmemory + addr);
    do_put_mem_word (m, w);
}

void REGPARAM2 chipmem_bput (uaecptr addr, uae_u32 b)
{
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    chipmemory[addr] = b;
}

uae_u32 REGPARAM2 chipmem_agnus_lget (uaecptr addr)
{
    uae_u32 *m;

    addr -= chipmem_start & chipmem_full_mask;
    addr &= chipmem_full_mask;
    m = (uae_u32 *)(chipmemory + addr);
    return do_get_mem_long (m);
}

uae_u32 REGPARAM2 chipmem_agnus_wget (uaecptr addr)
{
    uae_u16 *m;

    addr -= chipmem_start & chipmem_full_mask;
    addr &= chipmem_full_mask;
    m = (uae_u16 *)(chipmemory + addr);
    return do_get_mem_word (m);
}

uae_u32 REGPARAM2 chipmem_agnus_bget (uaecptr addr)
{
    addr -= chipmem_start & chipmem_full_mask;
    addr &= chipmem_full_mask;
    return chipmemory[addr];
}

void REGPARAM2 chipmem_agnus_lput (uaecptr addr, uae_u32 l)
{
    uae_u32 *m;

    addr -= chipmem_start & chipmem_full_mask;
    addr &= chipmem_full_mask;
    if (addr >= allocated_chipmem)
	return;
    m = (uae_u32 *)(chipmemory + addr);
    do_put_mem_long (m, l);
}

void REGPARAM2 chipmem_agnus_wput (uaecptr addr, uae_u32 w)
{
    uae_u16 *m;

    addr -= chipmem_start & chipmem_full_mask;
    addr &= chipmem_full_mask;
    if (addr >= allocated_chipmem)
	return;
    m = (uae_u16 *)(chipmemory + addr);
    do_put_mem_word (m, w);
}

void REGPARAM2 chipmem_agnus_bput (uaecptr addr, uae_u32 b)
{
    addr -= chipmem_start & chipmem_full_mask;
    addr &= chipmem_full_mask;
    if (addr >= allocated_chipmem)
	return;
    chipmemory[addr] = b;
}

int REGPARAM2 chipmem_check (uaecptr addr, uae_u32 size)
{
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    return (addr + size) <= allocated_chipmem;
}

uae_u8 *REGPARAM2 chipmem_xlate (uaecptr addr)
{
    addr -= chipmem_start & chipmem_mask;
    addr &= chipmem_mask;
    return chipmemory + addr;
}

/* Slow memory */

static uae_u8 *bogomemory;

static uae_u32 REGPARAM3 bogomem_lget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 bogomem_wget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 bogomem_bget (uaecptr) REGPARAM;
static void REGPARAM3 bogomem_lput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 bogomem_wput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 bogomem_bput (uaecptr, uae_u32) REGPARAM;
static int REGPARAM3 bogomem_check (uaecptr addr, uae_u32 size) REGPARAM;
static uae_u8 *REGPARAM3 bogomem_xlate (uaecptr addr) REGPARAM;

uae_u32 REGPARAM2 bogomem_lget (uaecptr addr)
{
    uae_u32 *m;
    addr -= bogomem_start & bogomem_mask;
    addr &= bogomem_mask;
    m = (uae_u32 *)(bogomemory + addr);
    return do_get_mem_long (m);
}

uae_u32 REGPARAM2 bogomem_wget (uaecptr addr)
{
    uae_u16 *m;
    addr -= bogomem_start & bogomem_mask;
    addr &= bogomem_mask;
    m = (uae_u16 *)(bogomemory + addr);
    return do_get_mem_word (m);
}

uae_u32 REGPARAM2 bogomem_bget (uaecptr addr)
{
    addr -= bogomem_start & bogomem_mask;
    addr &= bogomem_mask;
    return bogomemory[addr];
}

void REGPARAM2 bogomem_lput (uaecptr addr, uae_u32 l)
{
    uae_u32 *m;
    addr -= bogomem_start & bogomem_mask;
    addr &= bogomem_mask;
    m = (uae_u32 *)(bogomemory + addr);
    do_put_mem_long (m, l);
}

void REGPARAM2 bogomem_wput (uaecptr addr, uae_u32 w)
{
    uae_u16 *m;
    addr -= bogomem_start & bogomem_mask;
    addr &= bogomem_mask;
    m = (uae_u16 *)(bogomemory + addr);
    do_put_mem_word (m, w);
}

void REGPARAM2 bogomem_bput (uaecptr addr, uae_u32 b)
{
    addr -= bogomem_start & bogomem_mask;
    addr &= bogomem_mask;
    bogomemory[addr] = b;
}

int REGPARAM2 bogomem_check (uaecptr addr, uae_u32 size)
{
    addr -= bogomem_start & bogomem_mask;
    addr &= bogomem_mask;
    return (addr + size) <= allocated_bogomem;
}

uae_u8 *REGPARAM2 bogomem_xlate (uaecptr addr)
{
    addr -= bogomem_start & bogomem_mask;
    addr &= bogomem_mask;
    return bogomemory + addr;
}

#ifdef AUTOCONFIG

/* A3000 motherboard fast memory */

static uae_u8 *a3000memory;

static uae_u32 REGPARAM3 a3000mem_lget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 a3000mem_wget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 a3000mem_bget (uaecptr) REGPARAM;
static void REGPARAM3 a3000mem_lput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 a3000mem_wput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 a3000mem_bput (uaecptr, uae_u32) REGPARAM;
static int REGPARAM3 a3000mem_check (uaecptr addr, uae_u32 size) REGPARAM;
static uae_u8 *REGPARAM3 a3000mem_xlate (uaecptr addr) REGPARAM;

uae_u32 REGPARAM2 a3000mem_lget (uaecptr addr)
{
    uae_u32 *m;
    addr -= a3000mem_start & a3000mem_mask;
    addr &= a3000mem_mask;
    m = (uae_u32 *)(a3000memory + addr);
    return do_get_mem_long (m);
}

uae_u32 REGPARAM2 a3000mem_wget (uaecptr addr)
{
    uae_u16 *m;
    addr -= a3000mem_start & a3000mem_mask;
    addr &= a3000mem_mask;
    m = (uae_u16 *)(a3000memory + addr);
    return do_get_mem_word (m);
}

uae_u32 REGPARAM2 a3000mem_bget (uaecptr addr)
{
    addr -= a3000mem_start & a3000mem_mask;
    addr &= a3000mem_mask;
    return a3000memory[addr];
}

void REGPARAM2 a3000mem_lput (uaecptr addr, uae_u32 l)
{
    uae_u32 *m;
    addr -= a3000mem_start & a3000mem_mask;
    addr &= a3000mem_mask;
    m = (uae_u32 *)(a3000memory + addr);
    do_put_mem_long (m, l);
}

void REGPARAM2 a3000mem_wput (uaecptr addr, uae_u32 w)
{
    uae_u16 *m;
    addr -= a3000mem_start & a3000mem_mask;
    addr &= a3000mem_mask;
    m = (uae_u16 *)(a3000memory + addr);
    do_put_mem_word (m, w);
}

void REGPARAM2 a3000mem_bput (uaecptr addr, uae_u32 b)
{
    addr -= a3000mem_start & a3000mem_mask;
    addr &= a3000mem_mask;
    a3000memory[addr] = b;
}

int REGPARAM2 a3000mem_check (uaecptr addr, uae_u32 size)
{
    addr -= a3000mem_start & a3000mem_mask;
    addr &= a3000mem_mask;
    return (addr + size) <= allocated_a3000mem;
}

uae_u8 *REGPARAM2 a3000mem_xlate (uaecptr addr)
{
    addr -= a3000mem_start & a3000mem_mask;
    addr &= a3000mem_mask;
    return a3000memory + addr;
}

#endif

/* Kick memory */

uae_u8 *kickmemory;
uae_u16 kickstart_version;
int kickmem_size = 0x80000;

/*
 * A1000 kickstart RAM handling
 *
 * RESET instruction unhides boot ROM and disables write protection
 * write access to boot ROM hides boot ROM and enables write protection
 *
 */
static int a1000_kickstart_mode;
static uae_u8 *a1000_bootrom;
static void a1000_handle_kickstart (int mode)
{
    if (!a1000_bootrom)
	return;
    if (mode == 0) {
	a1000_kickstart_mode = 0;
	memcpy (kickmemory, kickmemory + 262144, 262144);
	kickstart_version = (kickmemory[262144 + 12] << 8) | kickmemory[262144 + 13];
    } else {
	a1000_kickstart_mode = 1;
	memset (kickmemory, 0, 262144);
	memcpy (kickmemory, a1000_bootrom, 65536);
	memcpy (kickmemory + 131072, a1000_bootrom, 65536);
	kickstart_version = 0;
    }
}

void a1000_reset (void)
{
    a1000_handle_kickstart (1);
}

static uae_u32 REGPARAM3 kickmem_lget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 kickmem_wget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 kickmem_bget (uaecptr) REGPARAM;
static void REGPARAM3 kickmem_lput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 kickmem_wput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 kickmem_bput (uaecptr, uae_u32) REGPARAM;
static int REGPARAM3 kickmem_check (uaecptr addr, uae_u32 size) REGPARAM;
static uae_u8 *REGPARAM3 kickmem_xlate (uaecptr addr) REGPARAM;

uae_u32 REGPARAM2 kickmem_lget (uaecptr addr)
{
    uae_u32 *m;
    addr -= kickmem_start & kickmem_mask;
    addr &= kickmem_mask;
    m = (uae_u32 *)(kickmemory + addr);
    return do_get_mem_long (m);
}

uae_u32 REGPARAM2 kickmem_wget (uaecptr addr)
{
    uae_u16 *m;
    addr -= kickmem_start & kickmem_mask;
    addr &= kickmem_mask;
    m = (uae_u16 *)(kickmemory + addr);
    return do_get_mem_word (m);
}

uae_u32 REGPARAM2 kickmem_bget (uaecptr addr)
{
    addr -= kickmem_start & kickmem_mask;
    addr &= kickmem_mask;
    return kickmemory[addr];
}

void REGPARAM2 kickmem_lput (uaecptr addr, uae_u32 b)
{
    uae_u32 *m;
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    if (a1000_kickstart_mode) {
	if (addr >= 0xfc0000) {
	    addr -= kickmem_start & kickmem_mask;
	    addr &= kickmem_mask;
	    m = (uae_u32 *)(kickmemory + addr);
	    do_put_mem_long (m, b);
	    return;
	} else
	    a1000_handle_kickstart (0);
    } else if (currprefs.illegal_mem)
	write_log ("Illegal kickmem lput at %08lx\n", addr);
}

void REGPARAM2 kickmem_wput (uaecptr addr, uae_u32 b)
{
    uae_u16 *m;
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    if (a1000_kickstart_mode) {
	if (addr >= 0xfc0000) {
	    addr -= kickmem_start & kickmem_mask;
	    addr &= kickmem_mask;
	    m = (uae_u16 *)(kickmemory + addr);
	    do_put_mem_word (m, b);
	    return;
	} else
	    a1000_handle_kickstart (0);
    } else if (currprefs.illegal_mem)
	write_log ("Illegal kickmem wput at %08lx\n", addr);
}

void REGPARAM2 kickmem_bput (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    if (a1000_kickstart_mode) {
	if (addr >= 0xfc0000) {
	    addr -= kickmem_start & kickmem_mask;
	    addr &= kickmem_mask;
	    kickmemory[addr] = b;
	    return;
	} else
	    a1000_handle_kickstart (0);
    } else if (currprefs.illegal_mem)
	write_log ("Illegal kickmem lput at %08lx\n", addr);
}

void REGPARAM2 kickmem2_lput (uaecptr addr, uae_u32 l)
{
    uae_u32 *m;
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    addr -= kickmem_start & kickmem_mask;
    addr &= kickmem_mask;
    m = (uae_u32 *)(kickmemory + addr);
    do_put_mem_long (m, l);
}

void REGPARAM2 kickmem2_wput (uaecptr addr, uae_u32 w)
{
    uae_u16 *m;
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    addr -= kickmem_start & kickmem_mask;
    addr &= kickmem_mask;
    m = (uae_u16 *)(kickmemory + addr);
    do_put_mem_word (m, w);
}

void REGPARAM2 kickmem2_bput (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    addr -= kickmem_start & kickmem_mask;
    addr &= kickmem_mask;
    kickmemory[addr] = b;
}

int REGPARAM2 kickmem_check (uaecptr addr, uae_u32 size)
{
    addr -= kickmem_start & kickmem_mask;
    addr &= kickmem_mask;
    return (addr + size) <= kickmem_size;
}

uae_u8 *REGPARAM2 kickmem_xlate (uaecptr addr)
{
    addr -= kickmem_start & kickmem_mask;
    addr &= kickmem_mask;
    return kickmemory + addr;
}

/* CD32/CDTV extended kick memory */

uae_u8 *extendedkickmemory;
static int extendedkickmem_size;
static uae_u32 extendedkickmem_start;

#define EXTENDED_ROM_CD32 1
#define EXTENDED_ROM_CDTV 2

static int extromtype (void)
{
    switch (extendedkickmem_size) {
    case 524288:
	return EXTENDED_ROM_CD32;
    case 262144:
	return EXTENDED_ROM_CDTV;
    }
    return 0;
}

static uae_u32 REGPARAM3 extendedkickmem_lget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 extendedkickmem_wget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 extendedkickmem_bget (uaecptr) REGPARAM;
static void REGPARAM3 extendedkickmem_lput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 extendedkickmem_wput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 extendedkickmem_bput (uaecptr, uae_u32) REGPARAM;
static int REGPARAM3 extendedkickmem_check (uaecptr addr, uae_u32 size) REGPARAM;
static uae_u8 *REGPARAM3 extendedkickmem_xlate (uaecptr addr) REGPARAM;

uae_u32 REGPARAM2 extendedkickmem_lget (uaecptr addr)
{
    uae_u32 *m;
    addr -= extendedkickmem_start & extendedkickmem_mask;
    addr &= extendedkickmem_mask;
    m = (uae_u32 *)(extendedkickmemory + addr);
    return do_get_mem_long (m);
}

uae_u32 REGPARAM2 extendedkickmem_wget (uaecptr addr)
{
    uae_u16 *m;
    addr -= extendedkickmem_start & extendedkickmem_mask;
    addr &= extendedkickmem_mask;
    m = (uae_u16 *)(extendedkickmemory + addr);
    return do_get_mem_word (m);
}

uae_u32 REGPARAM2 extendedkickmem_bget (uaecptr addr)
{
    addr -= extendedkickmem_start & extendedkickmem_mask;
    addr &= extendedkickmem_mask;
    return extendedkickmemory[addr];
}

void REGPARAM2 extendedkickmem_lput (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    if (currprefs.illegal_mem)
	write_log ("Illegal extendedkickmem lput at %08lx\n", addr);
}

void REGPARAM2 extendedkickmem_wput (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    if (currprefs.illegal_mem)
	write_log ("Illegal extendedkickmem wput at %08lx\n", addr);
}

void REGPARAM2 extendedkickmem_bput (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    if (currprefs.illegal_mem)
	write_log ("Illegal extendedkickmem lput at %08lx\n", addr);
}

int REGPARAM2 extendedkickmem_check (uaecptr addr, uae_u32 size)
{
    addr -= extendedkickmem_start & extendedkickmem_mask;
    addr &= extendedkickmem_mask;
    return (addr + size) <= extendedkickmem_size;
}

uae_u8 *REGPARAM2 extendedkickmem_xlate (uaecptr addr)
{
    addr -= extendedkickmem_start & extendedkickmem_mask;
    addr &= extendedkickmem_mask;
    return extendedkickmemory + addr;
}

/* Default memory access functions */

int REGPARAM2 default_check (uaecptr a, uae_u32 b)
{
    return 0;
}

static int be_cnt;

uae_u8 *REGPARAM2 default_xlate (uaecptr a)
{
    if (quit_program == 0) {
	/* do this only in 68010+ mode, there are some tricky A500 programs.. */
	if (currprefs.cpu_level > 0 || !currprefs.cpu_compatible) {
#if defined(ENFORCER)
	    enforcer_disable ();
#endif
	    if (be_cnt < 3) {
		int i, j;
		uaecptr a2 = a - 32;
		uaecptr a3 = m68k_getpc(&regs) - 32;
		write_log ("Your Amiga program just did something terribly stupid %p PC=%p\n", a, M68K_GETPC);
		m68k_dumpstate (0, 0);
		for (i = 0; i < 10; i++) {
		    write_log ("%08.8X ", i >= 5 ? a3 : a2);
		    for (j = 0; j < 16; j += 2) {
			write_log (" %04.4X", get_word (i >= 5 ? a3 : a2));
			if (i >= 5) a3 +=2; else a2 += 2;
		    }
		    write_log ("\n");
		}
	    }
	    be_cnt++;
	    if (be_cnt > 1000) {
		uae_reset (0);
		be_cnt = 0;
	    } else {
		regs.panic = 1;
		regs.panic_pc = m68k_getpc (&regs);
		regs.panic_addr = a;
		set_special (&regs, SPCFLAG_BRK);
	    }
	}
    }
    return kickmem_xlate (0); /* So we don't crash. */
}

/* Address banks */

addrbank dummy_bank = {
    dummy_lget, dummy_wget, dummy_bget,
    dummy_lput, dummy_wput, dummy_bput,
    default_xlate, dummy_check, NULL, NULL
};

#ifdef AUTOCONFIG
addrbank mbres_bank = {
    mbres_lget, mbres_wget, mbres_bget,
    mbres_lput, mbres_wput, mbres_bput,
    default_xlate, mbres_check, NULL, "MBRES"
};
#endif

addrbank chipmem_bank = {
    chipmem_lget, chipmem_wget, chipmem_bget,
    chipmem_lput, chipmem_wput, chipmem_bput,
    chipmem_xlate, chipmem_check, NULL, "Chip memory"
};

addrbank chipmem_agnus_bank = {
    chipmem_agnus_lget, chipmem_agnus_wget, chipmem_agnus_bget,
    chipmem_agnus_lput, chipmem_agnus_wput, chipmem_agnus_bput,
    chipmem_xlate, chipmem_check, NULL, "Chip memory"
};

#ifdef AGA
addrbank chipmem_bank_ce2 = {
    chipmem_lget_ce2, chipmem_wget_ce2, chipmem_bget_ce2,
    chipmem_lput_ce2, chipmem_wput_ce2, chipmem_bput_ce2,
    chipmem_xlate, chipmem_check, NULL, "Chip memory"
};
#endif

addrbank bogomem_bank = {
    bogomem_lget, bogomem_wget, bogomem_bget,
    bogomem_lput, bogomem_wput, bogomem_bput,
    bogomem_xlate, bogomem_check, NULL, "Slow memory"
};

#ifdef AUTOCONFIG
addrbank a3000mem_bank = {
    a3000mem_lget, a3000mem_wget, a3000mem_bget,
    a3000mem_lput, a3000mem_wput, a3000mem_bput,
    a3000mem_xlate, a3000mem_check, NULL, "A3000 memory"
};
#endif

addrbank kickmem_bank = {
    kickmem_lget, kickmem_wget, kickmem_bget,
    kickmem_lput, kickmem_wput, kickmem_bput,
    kickmem_xlate, kickmem_check, NULL, "Kickstart ROM"
};

addrbank kickram_bank = {
    kickmem_lget, kickmem_wget, kickmem_bget,
    kickmem2_lput, kickmem2_wput, kickmem2_bput,
    kickmem_xlate, kickmem_check, NULL, "Kickstart Shadow RAM"
};

addrbank extendedkickmem_bank = {
    extendedkickmem_lget, extendedkickmem_wget, extendedkickmem_bget,
    extendedkickmem_lput, extendedkickmem_wput, extendedkickmem_bput,
    extendedkickmem_xlate, extendedkickmem_check, NULL, "Extended Kickstart ROM"
};

static int kickstart_checksum (uae_u8 *mem, int size)
{
    if (!kickstart_checksum_do (mem, size)) {
#ifndef	SINGLEFILE
	notify_user (NUMSG_KSROMCRCERROR);
#endif
	return 0;
    }
    return 1;
}

static char *kickstring = "exec.library";
int read_kickstart (struct zfile *f, uae_u8 *mem, int size, int dochecksum, int *cloanto_rom)
{
    unsigned char buffer[20];
    int i, j;
    int cr = 0, kickdisk = 0;

    if (cloanto_rom)
	*cloanto_rom = 0;
    if (size < 0) {
	zfile_fseek (f, 0, SEEK_END);
	size = zfile_ftell (f) & ~0x3ff;
	zfile_fseek (f, 0, SEEK_SET);
    }
    i = zfile_fread (buffer, 1, 11, f);
    if (!memcmp(buffer, "KICK", 4)) {
	zfile_fseek (f, 512, SEEK_SET);
	kickdisk = 1;
    } else if (strncmp ((char *)buffer, "AMIROMTYPE1", 11) != 0) {
	zfile_fseek (f, 0, SEEK_SET);
    } else {
	cr = 1;
    }

    if (cloanto_rom)
	*cloanto_rom = cr;

    i = zfile_fread (mem, 1, size, f);
    if (kickdisk && i > 262144)
	i = 262144;
    zfile_fclose (f);
    if ((i != 8192 && i != 65536) && i != 131072 && i != 262144 && i != 524288) {
	notify_user (NUMSG_KSROMREADERROR);
	return 0;
    }
    if (i == size / 2)
	memcpy (mem + size / 2, mem, size / 2);

    if (cr) {
	if (!decode_cloanto_rom (mem, size, i))
	    return 0;
    }
    if (i == 8192 || i == 65536) {
	a1000_bootrom = xmalloc (65536);
	memcpy (a1000_bootrom, kickmemory, 65536);
	memset (kickmemory, 0, kickmem_size);
	a1000_handle_kickstart (1);
	i = 524288;
	dochecksum = 0;
    }
    for (j = 0; j < 256 && i >= 262144; j++) {
	if (!memcmp (kickmemory + j, kickstring, strlen (kickstring) + 1))
	    break;
    }
    if (j == 256 || i < 262144)
	dochecksum = 0;

    if (dochecksum)
	kickstart_checksum (mem, size);
    return i;
}

static int load_extendedkickstart (void)
{
    struct zfile *f;
    int size;

    if (strlen(currprefs.romextfile) == 0)
	return 0;
    f = zfile_fopen (currprefs.romextfile, "rb");
    if (!f) {
	notify_user (NUMSG_NOEXTROM);
	return 0;
    }
    zfile_fseek (f, 0, SEEK_END);
    size = zfile_ftell (f);
    if (size > 300000)
	extendedkickmem_size = 524288;
    else
	extendedkickmem_size = 262144;
    zfile_fseek (f, 0, SEEK_SET);
    switch (extromtype ()) {

    case EXTENDED_ROM_CDTV:
	extendedkickmemory = (uae_u8 *) mapped_malloc (extendedkickmem_size, "rom_f0");
	extendedkickmem_bank.baseaddr = (uae_u8 *) extendedkickmemory;
	break;
    case EXTENDED_ROM_CD32:
	extendedkickmemory = (uae_u8 *) mapped_malloc (extendedkickmem_size, "rom_e0");
	extendedkickmem_bank.baseaddr = (uae_u8 *) extendedkickmemory;
	break;
    }
    read_kickstart (f, extendedkickmemory, extendedkickmem_size,  0, 0);
    extendedkickmem_mask = extendedkickmem_size - 1;
    return 1;
}

static void kickstart_fix_checksum (uae_u8 *mem, int size)
{
    uae_u32 cksum = 0, prevck = 0;
    int i, ch = size == 524288 ? 0x7ffe8 : 0x3e;

    mem[ch] = 0;
    mem[ch + 1] = 0;
    mem[ch + 2] = 0;
    mem[ch + 3] = 0;
    for (i = 0; i < size; i+=4) {
	uae_u32 data = (mem[i] << 24) | (mem[i + 1] << 16) | (mem[i + 2] << 8) | mem[i + 3];
	cksum += data;
	if (cksum < prevck)
	    cksum++;
	prevck = cksum;
    }
    cksum ^= 0xffffffff;
    mem[ch++] = cksum >> 24;
    mem[ch++] = cksum >> 16;
    mem[ch++] = cksum >> 8;
    mem[ch++] = cksum >> 0;
}

static int patch_shapeshifter (uae_u8 *kickmemory)
{
    /* Patch Kickstart ROM for ShapeShifter - from Christian Bauer.
     * Changes 'lea $400,a0' and 'lea $1000,a0' to 'lea $3000,a0' for
     * ShapeShifter compatability.
    */
    int i, patched = 0;
    uae_u8 kickshift1[] = { 0x41, 0xf8, 0x04, 0x00 };
    uae_u8 kickshift2[] = { 0x41, 0xf8, 0x10, 0x00 };
    uae_u8 kickshift3[] = { 0x43, 0xf8, 0x04, 0x00 };

    for (i = 0x200; i < 0x300; i++) {
	if (!memcmp (kickmemory + i, kickshift1, sizeof (kickshift1)) ||
	!memcmp (kickmemory + i, kickshift2, sizeof (kickshift2)) ||
	!memcmp (kickmemory + i, kickshift3, sizeof (kickshift3))) {
	    kickmemory[i + 2] = 0x30;
	    write_log ("Kickstart KickShifted @%04.4X\n", i);
	    patched++;
	}
    }
    return patched;
}

/* disable incompatible drivers */
static int patch_residents (uae_u8 *kickmemory)
{
    int i, j, patched = 0;
    char *residents[] = { "NCR scsi.device", 0 };
    // "scsi.device", "carddisk.device", "card.resource" };
    uaecptr base = 0xf80000;

    for (i = 0; i < kickmem_size - 100; i++) {
	if (kickmemory[i] == 0x4a && kickmemory[i + 1] == 0xfc) {
	    uaecptr addr;
	    addr = (kickmemory[i + 2] << 24) | (kickmemory[i + 3] << 16) | (kickmemory[i + 4] << 8) | (kickmemory[i + 5] << 0);
	    if (addr != i + base)
		continue;
	    addr = (kickmemory[i + 14] << 24) | (kickmemory[i + 15] << 16) | (kickmemory[i + 16] << 8) | (kickmemory[i + 17] << 0);
	    if (addr >= base && addr < base + 524288) {
		j = 0;
		while (residents[j]) {
		    if (!memcmp (residents[j], kickmemory + addr - base, strlen (residents[j]) + 1)) {
			write_log ("KSPatcher: '%s' at %08.8X disabled\n", residents[j], i + base);
			kickmemory[i] = 0x4b; /* destroy RTC_MATCHWORD */
			patched++;
			break;
		    }
		    j++;
		}
	    }
	}
    }
    return patched;
}

static int load_kickstart (void)
{
    struct zfile *f = zfile_fopen (currprefs.romfile, "rb");
    char tmprom[MAX_DPATH], tmprom2[MAX_DPATH];
    int patched = 0;

    strcpy (tmprom, currprefs.romfile);
    if (f == NULL) {
	sprintf (tmprom2, "%s%s", start_path_data, currprefs.romfile);
	f = zfile_fopen (tmprom2, "rb");
	if (f == NULL) {
	    sprintf (currprefs.romfile, "%sroms/kick.rom", start_path_data);
	    f = zfile_fopen (currprefs.romfile, "rb");
	    if (f == NULL) {
		sprintf (currprefs.romfile, "%skick.rom", start_path_data);
		f = zfile_fopen (currprefs.romfile, "rb");
		if (f == NULL) {
		    sprintf (currprefs.romfile, "%s../shared/rom/kick.rom", start_path_data);
		    f = zfile_fopen (currprefs.romfile, "rb");
		    if (f == NULL) {
			sprintf (currprefs.romfile, "%s../System/rom/kick.rom", start_path_data);
			f = zfile_fopen (currprefs.romfile, "rb");
		    }
		}
	    }
	} else {
	    strcpy (currprefs.romfile, tmprom2);
	}
    }
    if( f == NULL ) { /* still no luck */
#if defined(AMIGA)||defined(__POS__)
#define USE_UAE_ERSATZ "USE_UAE_ERSATZ"
	if( !getenv(USE_UAE_ERSATZ))
	{
	    write_log ("Using current ROM. (create ENV:%s to "
		"use uae's ROM replacement)\n",USE_UAE_ERSATZ);
	    memcpy(kickmemory,(char*)0x1000000-kickmem_size,kickmem_size);
	    kickstart_checksum (kickmemory, kickmem_size);
	    goto chk_sum;
	}
#else
	goto err;
#endif
    }

    if (f != NULL) {
	int size = read_kickstart (f, kickmemory, 0x80000, 1, &cloanto_rom);
	if (size == 0)
	    goto err;
	kickmem_mask = size - 1;
    }

#if defined(AMIGA)
    chk_sum:
#endif

    kickstart_version = (kickmemory[12] << 8) | kickmemory[13];
    return 1;
err:
    strcpy (currprefs.romfile, tmprom);
    return 0;
}

#ifndef NATMEM_OFFSET

uae_u8 *mapped_malloc (size_t s, char *file)
{
    return xmalloc (s);
}

void mapped_free (uae_u8 *p)
{
    xfree (p);
}

#else

#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/mman.h>

shmpiece *shm_start;

static void dumplist(void)
{
    shmpiece *x = shm_start;
    write_log ("Start Dump:\n");
    while (x) {
	write_log ("this=%p,Native %p,id %d,prev=%p,next=%p,size=0x%08x\n",
		x, x->native_address, x->id, x->prev, x->next, x->size);
	x = x->next;
    }
    write_log ("End Dump:\n");
}

static shmpiece *find_shmpiece (uae_u8 *base)
{
    shmpiece *x = shm_start;

    while (x && x->native_address != base)
	x = x->next;
    if (!x) {
	write_log ("NATMEM: Failure to find mapping at %p\n",base);
	dumplist ();
	canbang = 0;
	return 0;
    }
    return x;
}

static void delete_shmmaps (uae_u32 start, uae_u32 size)
{
    if (!canbang)
	return;

    while (size) {
	uae_u8 *base = mem_banks[bankindex (start)]->baseaddr;
	if (base) {
	    shmpiece *x;
	    //base = ((uae_u8*)NATMEM_OFFSET)+start;

	    x = find_shmpiece (base);
	    if (!x)
		return;

	    if (x->size > size) {
		write_log ("NATMEM: Failure to delete mapping at %08x(size %08x, delsize %08x)\n",start,x->size,size);
		dumplist ();
		canbang = 0;
		return;
	    }
	    shmdt (x->native_address);
	    size -= x->size;
	    start += x->size;
	    if (x->next)
		x->next->prev = x->prev;	/* remove this one from the list */
	    if (x->prev)
		x->prev->next = x->next;
	    else
		shm_start = x->next;
	    xfree (x);
	} else {
	    size -= 0x10000;
	    start += 0x10000;
	}
    }
}

static void add_shmmaps (uae_u32 start, addrbank *what)
{
    shmpiece *x = shm_start;
    shmpiece *y;
    uae_u8 *base = what->baseaddr;

    if (!canbang)
	return;
    if (!base)
	return;

    x = find_shmpiece (base);
    if (!x)
	return;
    y = xmalloc (sizeof (shmpiece));
    *y = *x;
    base = ((uae_u8 *) NATMEM_OFFSET) + start;
    y->native_address = shmat (y->id, base, 0);
    if (y->native_address == (void *) -1) {
	write_log ("NATMEM: Failure to map existing at %08x(%p)\n",start,base);
	dumplist ();
	canbang = 0;
	return;
    }
    y->next = shm_start;
    y->prev = NULL;
    if (y->next)
	y->next->prev = y;
    shm_start = y;
}

uae_u8 *mapped_malloc (size_t s, char *file)
{
    int id;
    void *answer;
    shmpiece *x;

    if (!canbang)
	return xmalloc (s);

    id = shmget (IPC_PRIVATE, s, 0x1ff, file);
    if (id == -1) {
	canbang = 0;
	return mapped_malloc (s, file);
    }
    answer = shmat (id, 0, 0);
    shmctl (id, IPC_RMID, NULL);
    if (answer != (void *) -1) {
	x = xmalloc (sizeof (shmpiece));
	x->native_address = answer;
	x->id = id;
	x->size = s;
	x->next = shm_start;
	x->prev = NULL;
	if (x->next)
	    x->next->prev = x;
	shm_start = x;

	return answer;
    }
    canbang = 0;
    return mapped_malloc (s, file);
}

#endif

static void init_mem_banks (void)
{
    int i;
    for (i = 0; i < MEMORY_BANKS; i++)
	put_mem_bank (i << 16, &dummy_bank, 0);
#ifdef NATMEM_OFFSET
    delete_shmmaps (0, 0xFFFF0000);
#endif
}

void clearexec (void)
{
    if (chipmemory)
	memset (chipmemory + 4, 0,  4);
}

static void allocate_memory (void)
{
    if (allocated_chipmem != currprefs.chipmem_size) {
	int memsize;
	if (chipmemory)
	    mapped_free (chipmemory);
	chipmemory = 0;

	memsize = allocated_chipmem = currprefs.chipmem_size;
	chipmem_full_mask = chipmem_mask = allocated_chipmem - 1;
	if (memsize < 0x100000)
	    memsize = 0x100000;
	chipmemory = mapped_malloc (memsize, "chip");
	if (chipmemory == 0) {
	    write_log ("Fatal error: out of memory for chipmem.\n");
	    allocated_chipmem = 0;
	} else {
	    clearexec ();
	    if (memsize != allocated_chipmem)
		memset (chipmemory + allocated_chipmem, 0xff, memsize - allocated_chipmem);
	}
    }

    currprefs.chipset_mask = changed_prefs.chipset_mask;
    chipmem_full_mask = allocated_chipmem - 1;
    if ((currprefs.chipset_mask & CSMASK_ECS_AGNUS) && allocated_chipmem < 0x100000)
        chipmem_full_mask = 0x100000 - 1;

    if (allocated_bogomem != currprefs.bogomem_size) {
	if (bogomemory)
	    mapped_free (bogomemory);
	bogomemory = 0;

	allocated_bogomem = currprefs.bogomem_size;
	bogomem_mask = allocated_bogomem - 1;

	if (allocated_bogomem) {
	    bogomemory = mapped_malloc (allocated_bogomem, "bogo");
	    if (bogomemory == 0) {
		write_log ("Out of memory for bogomem.\n");
		allocated_bogomem = 0;
	    }
	}
	clearexec ();
    }
#ifdef AUTOCONFIG
    if (allocated_a3000mem != currprefs.a3000mem_size) {
	if (a3000memory)
	    mapped_free (a3000memory);
	a3000memory = 0;

	allocated_a3000mem = currprefs.a3000mem_size;
	a3000mem_mask = allocated_a3000mem - 1;

	if (allocated_a3000mem) {
	    a3000memory = mapped_malloc (allocated_a3000mem, "a3000");
	    if (a3000memory == 0) {
		write_log ("Out of memory for a3000mem.\n");
		allocated_a3000mem = 0;
	    }
	}
	clearexec ();
    }
#endif
    if (savestate_state == STATE_RESTORE) {
	restore_ram (chip_filepos, chipmemory);
	if (allocated_bogomem > 0)
	    restore_ram (bogo_filepos, bogomemory);
    }
    chipmem_bank.baseaddr = chipmemory;
#ifdef AGA
    chipmem_bank_ce2.baseaddr = chipmemory;
#endif
    bogomem_bank.baseaddr = bogomemory;
}

void map_overlay (int chip)
{
    int i = allocated_chipmem > 0x200000 ? (allocated_chipmem >> 16) : 32;
    addrbank *cb;

    cb = &chipmem_bank;
#ifdef AGA
    if (currprefs.cpu_cycle_exact && currprefs.cpu_level >= 2)
	cb = &chipmem_bank_ce2;
#endif
    if (chip)
	map_banks (cb, 0, i, allocated_chipmem);
    else
	map_banks (&kickmem_bank, 0, i, 0x80000);
    if (savestate_state != STATE_RESTORE && savestate_state != STATE_REWIND)
	m68k_setpc(&regs, m68k_getpc(&regs));
}

void memory_reset (void)
{
    int custom_start, bnk;

    be_cnt = 0;
    currprefs.chipmem_size = changed_prefs.chipmem_size;
    currprefs.bogomem_size = changed_prefs.bogomem_size;
    currprefs.a3000mem_size = changed_prefs.a3000mem_size;

    init_mem_banks ();
    allocate_memory ();

    if (strcmp (currprefs.romfile, changed_prefs.romfile) != 0
	|| strcmp (currprefs.romextfile, changed_prefs.romextfile) != 0)
    {
	ersatzkickfile = 0;
	a1000_handle_kickstart (0);
	xfree (a1000_bootrom);
	a1000_bootrom = 0;
	a1000_kickstart_mode = 0;
	memcpy (currprefs.romfile, changed_prefs.romfile, sizeof currprefs.romfile);
	memcpy (currprefs.romextfile, changed_prefs.romextfile, sizeof currprefs.romextfile);
	if (savestate_state != STATE_RESTORE)
	    clearexec ();
	xfree (extendedkickmemory);
	extendedkickmemory = 0;
	extendedkickmem_size = 0;
	load_extendedkickstart ();
	kickmem_mask = 524288 - 1;
	if (!load_kickstart ()) {
	    if (strlen (currprefs.romfile) > 0) {
		write_log ("%s\n", currprefs.romfile);
		notify_user (NUMSG_NOROM);
	    }
#ifdef AUTOCONFIG
	    init_ersatz_rom (kickmemory);
	    ersatzkickfile = 1;
#else
	    uae_restart (-1, NULL);
#endif
	} else {
	    struct romdata *rd = getromdatabydata (kickmemory, kickmem_size);
	    if (rd) {
		if (rd->cpu == 1 && changed_prefs.cpu_level < 2) {
		    notify_user (NUMSG_KS68EC020);
		    uae_restart (-1, NULL);
		} else if (rd->cpu == 2 && (changed_prefs.cpu_level < 2 || changed_prefs.address_space_24)) {
		    notify_user (NUMSG_KS68020);
		    uae_restart (-1, NULL);
		}
		if (rd->cloanto)
		    cloanto_rom = 1;
	    }
	}
	if (kickmem_size >= 524288) {
	    int patched = 0;
	    if (currprefs.kickshifter)
		patched += patch_shapeshifter (kickmemory);
	    patched += patch_residents (kickmemory);
	    if (patched)
		kickstart_fix_checksum (kickmemory, kickmem_size);
	}
    }

    if (cloanto_rom)
	currprefs.maprom = changed_prefs.maprom = 0;

    custom_start = 0xC0;
    map_banks (&custom_bank, custom_start, 0xE0 - custom_start, 0);
    map_banks (&cia_bank, 0xA0, 32, 0);

    /* map "nothing" to 0x200000 - 0xa00000 */
    bnk = allocated_chipmem >> 16;
    if (bnk < 0x20 + (currprefs.fastmem_size >> 16))
	bnk = 0x20 + (currprefs.fastmem_size >> 16);
    map_banks (&dummy_bank, bnk, 0xA0 - bnk, 0);
    /*map_banks (&mbres_bank, 0xDE, 1); */

    if (bogomemory != 0) {
	int t = allocated_bogomem >> 16;
	if (t > 0x1C)
	    t = 0x1C;
	if (t > 0x10 && ((currprefs.chipset_mask & CSMASK_AGA) || currprefs.cpu_level >= 2))
	    t = 0x10;
	map_banks (&bogomem_bank, 0xC0, t, 0);
    }
    if ((currprefs.chipset_mask & CSMASK_AGA) || currprefs.cpu_level >= 2)
	map_banks (&gayle_bank, 0xD8, 7, 0);
    map_banks (&clock_bank, 0xDC, 1, 0);

#ifdef AUTOCONFIG
    if (a3000memory != 0)
	map_banks (&a3000mem_bank, a3000mem_start >> 16, allocated_a3000mem >> 16,
		   allocated_a3000mem);
    if (nr_units (currprefs.mountinfo) > 0)
	map_banks (&rtarea_bank, RTAREA_BASE >> 16, 1, 0);
#endif

    map_banks (&kickmem_bank, 0xF8, 8, 0);
    if (currprefs.maprom)
	map_banks (&kickram_bank, currprefs.maprom >> 16, 8, 0);
    /* map beta Kickstarts to 0x200000 */
    if (kickmemory[2] == 0x4e && kickmemory[3] == 0xf9 && kickmemory[4] == 0x00) {
	uae_u32 addr = kickmemory[5];
	if (addr == 0x20 && currprefs.chipmem_size <= 0x200000 && currprefs.fastmem_size == 0)
	    map_banks (&kickmem_bank, addr, 8, 0);
    }

    if (a1000_bootrom)
	a1000_handle_kickstart (1);
#ifdef AUTOCONFIG
    map_banks (&expamem_bank, 0xE8, 1, 0);
#endif

    /* Map the chipmem into all of the lower 8MB */
    map_overlay (1);

#ifdef CDTV
    cdtv_enabled = 0;
#endif
#ifdef CD32
    cd32_enabled = 0;
#endif

    switch (extromtype ()) {

#ifdef CDTV
    case EXTENDED_ROM_CDTV:
	map_banks (&extendedkickmem_bank, 0xF0, 4, 0);
	//extendedkickmemory[0x61a2] = 0x60;
	//extendedkickmemory[0x61a3] = 0x00;
	//extendedkickmemory[0x61a4] = 0x01;
	//extendedkickmemory[0x61a5] = 0x1a;
	cdtv_enabled = 1;
	break;
#endif
#ifdef CD32
    case EXTENDED_ROM_CD32:
	map_banks (&extendedkickmem_bank, 0xE0, 8, 0);
	cd32_enabled = 1;
	break;
#endif
    default:
	if (cloanto_rom && !currprefs.maprom)
	    map_banks (&kickmem_bank, 0xE0, 8, 0);
    }

#ifdef ARCADIA
    is_arcadia_rom (currprefs.cartfile);
    if (arcadia_rom) {
	if (strcmp (currprefs.cartfile, changed_prefs.cartfile) != 0) {
	    memcpy (currprefs.cartfile, changed_prefs.cartfile, sizeof currprefs.cartfile);
	    arcadia_unmap ();
	}
	arcadia_map_banks ();
    }
#endif

#ifdef ACTION_REPLAY
#ifdef ARCADIA
    if (!arcadia_rom) {
#endif
    action_replay_memory_reset();
#ifdef ARCADIA
    }
#endif
#endif
}

void memory_init (void)
{
    allocated_chipmem = 0;
    allocated_bogomem = 0;
    kickmemory = 0;
    extendedkickmemory = 0;
    extendedkickmem_size = 0;
    chipmemory = 0;
#ifdef AUTOCONFIG
    allocated_a3000mem = 0;
    a3000memory = 0;
#endif
    bogomemory = 0;

    kickmemory = mapped_malloc (kickmem_size, "kick");
    memset (kickmemory, 0, kickmem_size);
    kickmem_bank.baseaddr = kickmemory;
    strcpy (currprefs.romfile, "<none>");
    currprefs.romextfile[0] = 0;
#ifdef AUTOCONFIG
    init_ersatz_rom (kickmemory);
    ersatzkickfile = 1;
#endif

#ifdef ACTION_REPLAY
    action_replay_load();
    action_replay_init(1);
#ifdef ACTION_REPLAY_HRTMON
    hrtmon_load();
#endif
#endif

    init_mem_banks ();
}

void memory_cleanup (void)
{
#ifdef AUTOCONFIG
    if (a3000memory)
	mapped_free (a3000memory);
    a3000memory = 0;
#endif
    if (bogomemory)
	mapped_free (bogomemory);
    if (kickmemory)
	mapped_free (kickmemory);
    if (a1000_bootrom)
	xfree (a1000_bootrom);
    if (chipmemory)
	mapped_free (chipmemory);

    bogomemory = 0;
    kickmemory = 0;
    a1000_bootrom = 0;
    a1000_kickstart_mode = 0;
    chipmemory = 0;

    #ifdef ACTION_REPLAY
    action_replay_cleanup();
    #endif
    #ifdef ARCADIA
    arcadia_unmap ();
    #endif
}

void map_banks (addrbank *bank, int start, int size, int realsize)
{
    int bnr;
    unsigned long int hioffs = 0, endhioffs = 0x100;
    addrbank *orgbank = bank;
    uae_u32 realstart = start;

    flush_icache (1);		/* Sure don't want to keep any old mappings around! */
#ifdef NATMEM_OFFSET
    delete_shmmaps (start << 16, size << 16);
#endif

    if (!realsize)
	realsize = size << 16;

    if ((size << 16) < realsize) {
	gui_message ("Broken mapping, size=%x, realsize=%x\nStart is %x\n",
	    size, realsize, start);
    }

#ifndef ADDRESS_SPACE_24BIT
    if (start >= 0x100) {
	int real_left = 0;
	for (bnr = start; bnr < start + size; bnr++) {
	    if (!real_left) {
		realstart = bnr;
		real_left = realsize >> 16;
#ifdef NATMEM_OFFSET
		add_shmmaps (realstart << 16, bank);
#endif
	    }
	    put_mem_bank (bnr << 16, bank, realstart << 16);
	    real_left--;
	}
	return;
    }
#endif
    if (currprefs.address_space_24)
	endhioffs = 0x10000;
#ifdef ADDRESS_SPACE_24BIT
    endhioffs = 0x100;
#endif
    for (hioffs = 0; hioffs < endhioffs; hioffs += 0x100) {
	int real_left = 0;
	for (bnr = start; bnr < start + size; bnr++) {
	    if (!real_left) {
		realstart = bnr + hioffs;
		real_left = realsize >> 16;
#ifdef NATMEM_OFFSET
		add_shmmaps (realstart << 16, bank);
#endif
	    }
	    put_mem_bank ((bnr + hioffs) << 16, bank, realstart << 16);
	    real_left--;
	}
    }
}

#ifdef SAVESTATE

/* memory save/restore code */

uae_u8 *save_cram (int *len)
{
    *len = allocated_chipmem;
    return chipmemory;
}

uae_u8 *save_bram (int *len)
{
    *len = allocated_bogomem;
    return bogomemory;
}

void restore_cram (int len, size_t filepos)
{
    chip_filepos = filepos;
    changed_prefs.chipmem_size = len;
}

void restore_bram (int len, size_t filepos)
{
    bogo_filepos = filepos;
    changed_prefs.bogomem_size = len;
}

uae_u8 *restore_rom (uae_u8 *src)
{
    uae_u32 crc32, mem_start, mem_size, mem_type, version;
    int i;

    mem_start = restore_u32 ();
    mem_size = restore_u32 ();
    mem_type = restore_u32 ();
    version = restore_u32 ();
    crc32 = restore_u32 ();
    for (i = 0; i < romlist_cnt; i++) {
	if (rl[i].rd->crc32 == crc32 && crc32) {
	    switch (mem_type)
	    {
		case 0:
		strncpy (changed_prefs.romfile, rl[i].path, 255);
		break;
		case 1:
		strncpy (changed_prefs.romextfile, rl[i].path, 255);
		break;
	    }
	    break;
	}
    }
    src += strlen (src) + 1;
    if (zfile_exists(src)) {
	switch (mem_type)
	{
	    case 0:
	    strncpy (changed_prefs.romfile, src, 255);
	    break;
	    case 1:
	    strncpy (changed_prefs.romextfile, src, 255);
	    break;
	}
    }
    src += strlen (src) + 1;
    return src;
}

uae_u8 *save_rom (int first, int *len, uae_u8 *dstptr)
{
    static int count;
    uae_u8 *dst, *dstbak;
    uae_u8 *mem_real_start;
    uae_u32 version;
    char *path;
    int mem_start, mem_size, mem_type, saverom;
    int i;
    char tmpname[1000];

    version = 0;
    saverom = 0;
    if (first)
	count = 0;
    for (;;) {
	mem_type = count;
	mem_size = 0;
	switch (count) {
	case 0: /* Kickstart ROM */
	    mem_start = 0xf80000;
	    mem_real_start = kickmemory;
	    mem_size = kickmem_size;
	    path = currprefs.romfile;
	    /* 256KB or 512KB ROM? */
	    for (i = 0; i < mem_size / 2 - 4; i++) {
		if (longget (i + mem_start) != longget (i + mem_start + mem_size / 2))
		    break;
	    }
	    if (i == mem_size / 2 - 4) {
		mem_size /= 2;
		mem_start += 262144;
	    }
	    version = longget (mem_start + 12); /* version+revision */
	    sprintf (tmpname, "Kickstart %d.%d", wordget (mem_start + 12), wordget (mem_start + 14));
	    break;
	case 1: /* Extended ROM */
	    if (!extendedkickmem_size)
		break;
	    mem_start = extendedkickmem_start;
	    mem_real_start = extendedkickmemory;
	    mem_size = extendedkickmem_size;
	    path = currprefs.romextfile;
	    sprintf (tmpname, "Extended");
	    break;
	default:
	    return 0;
	}
	count++;
	if (mem_size)
	    break;
    }
    if (dstptr)
	dstbak = dst = dstptr;
    else
	dstbak = dst = xmalloc (4 + 4 + 4 + 4 + 4 + 256 + 256 + mem_size);
    save_u32 (mem_start);
    save_u32 (mem_size);
    save_u32 (mem_type);
    save_u32 (version);
    save_u32 (get_crc32 (mem_real_start, mem_size));
    strcpy (dst, tmpname);
    dst += strlen (dst) + 1;
    strcpy (dst, path);/* rom image name */
    dst += strlen(dst) + 1;
    if (saverom) {
	for (i = 0; i < mem_size; i++)
	    *dst++ = byteget (mem_start + i);
    }
    *len = dst - dstbak;
    return dstbak;
}

#endif /* SAVESTATE */
