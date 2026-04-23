/*
 MKCDM.C - Preprocess .cue/.bin files for SHSUCDHD
 Generates a binary track table for use by SHSUCDHD
 Usage: mkcdm input.cue output.cdm

 The BIN filename is read from the CUE file's FILE directive.

 Binary format (all little-endian):
   Header (32 bytes):
     unsigned short  num_tracks       (number of tracks)
     unsigned short  first_track      (first track number, usually 1)
     unsigned short  last_track       (last track number)
     unsigned short  bin_sector_size  (raw sector size in BIN, typically 2352)
     unsigned long   lead_out_lba     (LBA of lead-out = total sectors)
     unsigned long   lead_out_redbook (Red Book of lead-out, precomputed)
     unsigned long   data_start_lba   (start LBA of first data track)
     unsigned long   data_end_lba     (end LBA of first data track = start + length)
     unsigned short  data_offset      (user data offset within raw sector for data track)
     unsigned short  bin_name_len     (length of BIN filename without null)
     unsigned long   reserved         (must be 0)
   Per track (20 bytes each):
     unsigned char   track_number     (1-based)
     unsigned char   type             (0=data, 1=audio)
     unsigned short  sector_size      (raw sector size in BIN for this track)
     unsigned short  data_offset      (offset of user data within raw sector)
     unsigned short  user_data_size   (cooked user data size, e.g. 2048)
     unsigned long   start_lba        (sector index from start of BIN file)
     unsigned long   length           (in sectors)
     unsigned long   start_redbook    (Red Book of track start, precomputed)
   After track table:
     char[]          bin_filename     (null-terminated BIN basename)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_TRACKS 99

/* Track types */
#define TRACK_TYPE_DATA  0
#define TRACK_TYPE_AUDIO 1

/* Track table entry */
#pragma pack(push, 1)
typedef struct {
    uint8_t   track_number;     /* 1-based */
    uint8_t   type;             /* 0=data, 1=audio */
    uint16_t  sector_size;      /* raw sector size in BIN file */
    uint16_t  data_offset;      /* byte offset of user data within raw sector */
    uint16_t  user_data_size;   /* cooked user data size */
    uint32_t  start_lba;        /* sector index from start of BIN */
    uint32_t  length;           /* length in sectors */
    uint32_t  start_redbook;    /* precomputed Red Book (frame, sec, min, 0) */
} rdbk_track_entry;

/* File header (32 bytes) */
typedef struct {
    char      magic[4];
    uint16_t  num_tracks;
    uint16_t  first_track;
    uint16_t  last_track;
    uint16_t  bin_sector_size;
    uint32_t  lead_out_lba;
    uint32_t  lead_out_redbook; /* precomputed Red Book (frame, sec, min, 0) */
    uint32_t  data_start_lba;   /* start LBA of first data track */
    uint32_t  data_end_lba;     /* end LBA of first data track (start + length) */
    uint16_t  data_offset;      /* user data offset for data track (e.g. 16) */
    uint16_t  bin_name_len;     /* length of BIN filename (without null) */
} rdbk_header;
#pragma pack(pop)

/* C11 stuff
_Static_assert(sizeof(rdbk_header) == 32, "rdbk_header must be 32 bytes");
_Static_assert(sizeof(rdbk_track_entry) == 20, "rdbk_track_entry must be 20 bytes");
*/
typedef char _check_hdr_size [(sizeof(rdbk_header) == 32) ? 1 : -1];
typedef char _check_trk_size [(sizeof(rdbk_track_entry) == 20) ? 1 : -1];

rdbk_track_entry tracks[MAX_TRACKS];
int num_tracks = 0;
char bin_files[MAX_TRACKS][256];
int num_bin_files = 0;

/* Convert LBA to Red Book packed format: byte 0=frame, byte 1=second, byte 2=minute, byte 3=0 */
uint32_t lba_to_redbook(uint32_t lba) {
    uint32_t abs_frame = lba + 150;
    uint8_t f = (uint8_t)(abs_frame % 75);
    uint8_t s = (uint8_t)((abs_frame / 75) % 60);
    uint8_t m = (uint8_t)((abs_frame / 75) / 60);
    return (uint32_t)f | ((uint32_t)s << 8) | ((uint32_t)m << 16);
}

/* Mode info: raw sector size, user data offset, user data size */
typedef struct {
    unsigned short sector_size;
    unsigned short data_offset;
    unsigned short user_data_size;
} mode_info;

/* Parse CUE track mode string to sector geometry */
mode_info parse_mode(const char *mode) {
    mode_info mi;
    if (strstr(mode, "AUDIO")) {
        mi.sector_size = 2352; mi.data_offset = 0; mi.user_data_size = 2352;
    } else if (strstr(mode, "MODE1/2048")) {
        mi.sector_size = 2048; mi.data_offset = 0; mi.user_data_size = 2048;
    } else if (strstr(mode, "MODE1/2352")) {
        mi.sector_size = 2352; mi.data_offset = 16; mi.user_data_size = 2048;
    } else if (strstr(mode, "MODE2/2336")) {
        mi.sector_size = 2336; mi.data_offset = 0; mi.user_data_size = 2336;
    } else if (strstr(mode, "MODE2/2352")) {
        mi.sector_size = 2352; mi.data_offset = 24; mi.user_data_size = 2048;
    } else {
        /* Unknown mode, default to raw 2352 */
        mi.sector_size = 2352; mi.data_offset = 0; mi.user_data_size = 2352;
    }
    return mi;
}

/* Parse MM:SS:FF to frame count (sector index).
   CUE INDEX values are file-relative frame positions. No -150 adjustment. */
long msf_to_frame(const char *s) {
    int m, sec, f;
    if (sscanf(s, "%d:%d:%d", &m, &sec, &f) != 3) return -1;
    return (long)m * 60 * 75 + (long)sec * 75 + (long)f;
}

/* Trim leading and trailing whitespace in-place */
void trim(char *s) {
    size_t len;
    char *end;
    char *start;

    len = strlen(s);
    if (len == 0) return;
    /* Trim trailing */
    end = s + len - 1;
    while (end >= s && isspace((unsigned char)*end)) *end-- = '\0';
    /* Trim leading by shifting */
    start = s;
    while (*start && isspace((unsigned char)*start)) ++start;
    if (start != s) {
        len = strlen(start);
        memmove(s, start, len + 1);
    }
}

/* Check if line starts with a keyword (case-insensitive, after trimming) */
int starts_with(const char *line, const char *keyword) {
    size_t klen = strlen(keyword);
    size_t i;
    if (strlen(line) < klen) return 0;
    for (i = 0; i < klen; ++i) {
        if (toupper((unsigned char)line[i]) != toupper((unsigned char)keyword[i]))
            return 0;
    }
    /* Must be followed by whitespace or end of string */
    return line[klen] == '\0' || isspace((unsigned char)line[klen]);
}

/* Get size of a BIN file, resolving path relative to CUE directory */
long get_bin_file_size(const char *cue_dir, const char *bin_name) {
    static char path[512];
    FILE *f;
    long sz;
    size_t dlen = strlen(cue_dir);
    if (dlen > 0 && dlen + strlen(bin_name) < sizeof(path)) {
        memcpy(path, cue_dir, dlen);
        strcpy(path + dlen, bin_name);
    } else {
        strncpy(path, bin_name, sizeof(path)-1);
        path[sizeof(path) - 1] = '\0';
    }
    f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fclose(f);
    return sz;
}

int main(int argc, char *argv[]) {
    FILE *fcue;
    FILE *fout;
    FILE *fmerge;
    static char line[512];
    static char cue_dir[512];
    static char rdbk_bin_name[256];
    static char merge_path[512];
    int current_track;
    uint16_t bin_sect_size;
    uint32_t total_sectors;
    uint32_t cumulative_lba;
    uint32_t current_file_start_lba;
    uint16_t current_file_sect_size;
    int is_multi_file;
    rdbk_header hdr;
    mode_info mi;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.cue output.cdi\n", argv[0]);
        fprintf(stderr, "  BIN filename is read from the CUE file.\n");
        return 1;
    }

    current_track = -1;
    cumulative_lba = 0;
    current_file_start_lba = 0;
    current_file_sect_size = 2352;
    memset(rdbk_bin_name, 0, sizeof(rdbk_bin_name));
    memset(cue_dir, 0, sizeof(cue_dir));

    /* Extract CUE directory for resolving BIN paths */
    {
        const char *last_sep = strrchr(argv[1], '\\');
        if (last_sep) {
            size_t dlen = (size_t)(last_sep - argv[1] + 1);
            if (dlen < sizeof(cue_dir)) {
                memcpy(cue_dir, argv[1], dlen);
                cue_dir[dlen] = '\0';
            }
        }
    }
    
    fcue = fopen(argv[1], "r");
    if (!fcue) { perror(argv[1]); return 1; }

    while (fgets(line, sizeof(line), fcue)) {
        trim(line);

        if (starts_with(line, "FILE")) {
            if (num_bin_files > 0 && num_tracks > 0) {
                long prev_sz;
                uint32_t prev_sectors;
                prev_sz = get_bin_file_size(cue_dir, bin_files[num_bin_files-1]);
                if (prev_sz < 0) { fclose(fcue); return 1; }
                prev_sectors = (uint32_t)(prev_sz / current_file_sect_size);
                if (tracks[num_tracks - 1].length == 0) {
                    tracks[num_tracks - 1].length =
                        (current_file_start_lba + prev_sectors)
                        - tracks[num_tracks - 1].start_lba;
                }
                cumulative_lba += prev_sectors;
            }
            current_file_start_lba = cumulative_lba;
            /* Parse: FILE "filename" BINARY */
            {
                char *q1 = strchr(line, '"');
                if (q1) {
                    char *q2 = strchr(q1 + 1, '"');
                    if (q2) {
                        size_t namelen = (size_t)(q2 - q1 - 1);
                        if (namelen >= sizeof(bin_files[0]))
                            namelen = sizeof(bin_files[0]) - 1;
                        memcpy(bin_files[num_bin_files], q1+1, namelen);
                        bin_files[num_bin_files][namelen] = '\0';
                        num_bin_files++;
                    }
                }
            }
        } else if (starts_with(line, "TRACK")) {
            int tnum;
            char ttype[32];
            if (sscanf(line, "%*s %d %31s", &tnum, ttype) == 2) {
                if (num_tracks >= MAX_TRACKS) {
                    fprintf(stderr, "Error: too many tracks (max %d)\n", MAX_TRACKS);
                    fclose(fcue);
                    return 1;
                }
                current_track = tnum;
                mi = parse_mode(ttype);
                tracks[num_tracks].track_number = (uint8_t)tnum;
                tracks[num_tracks].type = strstr(ttype, "AUDIO")
                    ? TRACK_TYPE_AUDIO : TRACK_TYPE_DATA;
                tracks[num_tracks].sector_size = mi.sector_size;
                tracks[num_tracks].data_offset = mi.data_offset;
                tracks[num_tracks].user_data_size = mi.user_data_size;
                current_file_sect_size = mi.sector_size;
            }
        } else if (starts_with(line, "INDEX")) {
            int idx;
            char msf[16];
            if (sscanf(line, "%*s %d %15s", &idx, msf) == 2) {
                if (idx == 1 && current_track > 0) {
                    long frame = msf_to_frame(msf);
                    if (frame < 0) {
                        fprintf(stderr, "Error: invalid MSF '%s' for track %d\n",
                                msf, current_track);
                        fclose(fcue);
                        return 1;
                    }
                    tracks[num_tracks].start_lba =
                    current_file_start_lba + (uint32_t)frame;
                    /* Length of previous track = gap between starts */
                    if (num_tracks > 0 && tracks[num_tracks - 1].length == 0) {
                        tracks[num_tracks - 1].length =
                            tracks[num_tracks].start_lba
                            - tracks[num_tracks - 1].start_lba;
                    }
                    ++num_tracks;
                }
            }
        }
        /* Other CUE directives (PREGAP, POSTGAP, REM, etc.) are ignored */
    }
    fclose(fcue);

    /* Close out the last BIN file */
    if (num_bin_files > 0) {
        long last_sz;
        uint32_t last_sectors;
        last_sz = get_bin_file_size(cue_dir, bin_files[num_bin_files - 1]);
        if (last_sz < 0) return 1;
        last_sectors = (uint32_t)(last_sz / current_file_sect_size);
        if (num_tracks > 0 && tracks[num_tracks - 1].length == 0) {
            tracks[num_tracks - 1].length =
                (current_file_start_lba + last_sectors)
                - tracks[num_tracks - 1].start_lba;
        }
        cumulative_lba += last_sectors;
    }
    total_sectors = cumulative_lba;
    is_multi_file = (num_bin_files > 1);

    if (num_tracks == 0) {
        fprintf(stderr, "Error: no tracks found in CUE file.\n");
        return 1;
    }

    if (num_bin_files == 0) {
        fprintf(stderr, "Error: no FILE directive found in CUE file.\n");
        return 1;
    }
    
    bin_sect_size = tracks[0].sector_size;

    /* Determine BIN name */
    if (is_multi_file) {
        /* Derive from CUE filename: strip extension, add .BIN */
        const char *base;
        const char *dot;
        const char *ls;
        size_t blen;
        ls = strrchr(argv[1], '\\');
        base = ls ? ls + 1 : argv[1];
        dot = strrchr(base, '.');
        blen = dot ? (size_t)(dot - base) : strlen(base);
        if (blen + 4 >= sizeof(rdbk_bin_name)) blen = sizeof(rdbk_bin_name) - 5;
        memcpy(rdbk_bin_name, base, blen);
        memcpy(rdbk_bin_name + blen, ".BIN", 5);
    } else {
        strncpy(rdbk_bin_name, bin_files[0], sizeof(rdbk_bin_name) - 1);
    }

    /* Auto-merge multi-file BINs if merged file doesn't exist */
    if (is_multi_file) {
        memset(merge_path, 0, sizeof(merge_path));
        if (cue_dir[0]) {
            strncpy(merge_path, cue_dir, sizeof(merge_path) - 1);
            strncat(merge_path, rdbk_bin_name,
                    sizeof(merge_path) - strlen(merge_path) - 1);
        } else {
            strncpy(merge_path, rdbk_bin_name, sizeof(merge_path) - 1);
        }
        fmerge = fopen(merge_path, "rb");
        if(fmerge) {
            fclose(fmerge);
            printf("Merged BIN already exists: %s\n", merge_path);
        } else {
            int fi;
            printf("Creating merged BIN: %s\n", merge_path);
            fmerge = fopen(merge_path, "wb");
            if (!fmerge) { perror(merge_path); return 1; }
            for (fi = 0; fi < num_bin_files; ++fi) {
                static char src_path[512];
                FILE *fsrc;
                static char buf[4096];
                size_t nr;
                if (cue_dir[0]) {
                    strncpy(src_path, cue_dir, sizeof(src_path) - 1);
                    src_path[sizeof(src_path) - 1] = '\0';
                    strncat(src_path, bin_files[fi],
                            sizeof(src_path) - strlen(src_path) - 1);
                } else {
                    strncpy(src_path, bin_files[fi], sizeof(src_path) - 1);
                    src_path[sizeof(src_path) - 1] = '\0';
                }
                fsrc = fopen(src_path, "rb");
                if (!fsrc) {
                    perror(src_path);
                    fclose(fmerge);
                    return 1;
                }
                while ((nr = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
                    fwrite(buf, 1, nr, fmerge);
                }
                fclose(fsrc);
                printf(" + %s\n", bin_files[fi]);
            }
            fclose(fmerge);
            printf("Merged %d files -> %s\n", num_bin_files, rdbk_bin_name);
        }
    }

    /* Precompute Red Book addresses for all tracks */
    {
        int i;
        for (i = 0; i < num_tracks; ++i) {
            tracks[i].start_redbook = lba_to_redbook(tracks[i].start_lba);
        }
    }
    /* Build header */
    memset(&hdr, 0, sizeof(hdr));
    hdr.num_tracks = (uint16_t)num_tracks;
    hdr.first_track = tracks[0].track_number;
    hdr.last_track = tracks[num_tracks - 1].track_number;
    hdr.bin_sector_size = bin_sect_size;
    hdr.lead_out_lba = total_sectors;
    hdr.lead_out_redbook = lba_to_redbook(total_sectors);

    /* Find first data track and precompute its bounds for the header */
    hdr.data_start_lba = 0;
    hdr.data_end_lba = 0;
    hdr.data_offset = 0;
    {
        int i;
        for (i = 0; i < num_tracks; ++i) {
            if (tracks[i].type == TRACK_TYPE_DATA) {
                hdr.data_start_lba = tracks[i].start_lba;
                hdr.data_end_lba = tracks[i].start_lba + tracks[i].length;
                hdr.data_offset = tracks[i].data_offset;
                break;
            }
        }
    }

    /* Store BIN basename length (just the filename, not the path) */
    hdr.bin_name_len = (uint16_t)strlen(rdbk_bin_name);
    memcpy(hdr.magic, "RDBK", 4);

    fout = fopen(argv[2], "wb");
    if (!fout) { perror(argv[2]); return 1; }

    /* Write header (32 bytes) */
    fwrite(&hdr, sizeof(hdr), 1, fout);
    /* Write track entries */
    fwrite(tracks, sizeof(rdbk_track_entry), num_tracks, fout);
    /* Write BIN filename (null-terminated) after track table */
    fwrite(rdbk_bin_name, 1, strlen(rdbk_bin_name) + 1, fout);
    fclose(fout);

    /* Display results */
    printf("CUE:  %s\n", argv[1]);
    if (is_multi_file) {
        printf("BIN: %d files -> merged as %s\n", num_bin_files, rdbk_bin_name);
    } else {
        printf("BIN: %s (%lu bytes)\n", bin_files[0],
            (unsigned long)total_sectors * bin_sect_size);
    }
    printf("Out:  %s\n", argv[2]);
    printf("Tracks: %d (%d-%d), Total sectors: %lu, BIN sector size: %u\n",
           num_tracks, hdr.first_track, hdr.last_track,
           (unsigned long)total_sectors, bin_sect_size);
    printf("Lead-out: LBA %lu, Red Book %02u:%02u:%02u\n",
           (unsigned long)total_sectors,
           (unsigned)((hdr.lead_out_redbook >> 16) & 0xFF),
           (unsigned)((hdr.lead_out_redbook >> 8) & 0xFF),
           (unsigned)(hdr.lead_out_redbook & 0xFF));
    printf("Data track: LBA %lu-%lu, offset %u, BIN name: %s\n",
           (unsigned long)hdr.data_start_lba,
           (unsigned long)hdr.data_end_lba,
           hdr.data_offset, rdbk_bin_name);
    printf("Header: %u bytes, %d entries x %u bytes = %u bytes, name %u+1 bytes\n",
           (unsigned)sizeof(hdr),
           num_tracks, (unsigned)sizeof(rdbk_track_entry),
           (unsigned)(sizeof(hdr) + num_tracks * sizeof(rdbk_track_entry)
                      + strlen(rdbk_bin_name) + 1),
           (unsigned)strlen(rdbk_bin_name));

    if (is_multi_file) {
        printf("\nMulti-file CUE: %d BIN files merged into %s\n",
                num_bin_files, rdbk_bin_name);
    }
                      
    printf("\n");
    printf("  # Type   SectSz DataOfs UserSz  StartLBA  Length  RedBook\n");
    printf("  - -----  ------ ------- ------  --------  ------  --------\n");
    {
        int i;
        for (i = 0; i < num_tracks; ++i) {
            unsigned long rb = tracks[i].start_redbook;
            printf(" %2u %-5s  %5u  %5u   %5u   %8lu  %6lu  %02u:%02u:%02u\n",
                tracks[i].track_number,
                tracks[i].type == TRACK_TYPE_AUDIO ? "AUDIO" : "DATA",
                tracks[i].sector_size,
                tracks[i].data_offset,
                tracks[i].user_data_size,
                (unsigned long)tracks[i].start_lba,
                (unsigned long)tracks[i].length,
                (unsigned)((rb >> 16) & 0xFF),
                (unsigned)((rb >> 8) & 0xFF),
                (unsigned)(rb & 0xFF));
        }
    }
    return 0;
}

