/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define NEED_BYTESWAP
#include "headers.h"
#include "fmt.h"

#include "mplink.h"
#include "it_defs.h"

/* --------------------------------------------------------------------- */
int fmt_its_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        ITSAMPLESTRUCT *its;

        if (!(length > 80 && memcmp(data, "IMPS", 4) == 0))
                return 0;

        its = (ITSAMPLESTRUCT *)data;
        file->smp_length = bswapLE32(its->length);
        file->smp_flags = 0;

        if (its->flags & 2) {
                file->smp_flags |= SAMP_16_BIT;
        }
        if (its->flags & 16) {
                file->smp_flags |= SAMP_LOOP;
                if (its->flags & 64)
                        file->smp_flags |= SAMP_LOOP_PINGPONG;
        }
        if (its->flags & 32) {
                file->smp_flags |= SAMP_SUSLOOP;
                if (its->flags & 128)
                        file->smp_flags |= SAMP_SUSLOOP_PINGPONG;
        }

        if (its->dfp & 128) file->smp_flags |= SAMP_PANNING;
        if (its->flags & 4) file->smp_flags |= SAMP_STEREO;

        file->smp_defvol = its->vol;
        file->smp_gblvol = its->gvl;
        file->smp_vibrato_speed = its->vis;
        file->smp_vibrato_depth = its->vid & 0x7f;
        file->smp_vibrato_rate = its->vir;

        file->smp_loop_start = bswapLE32(its->loopbegin);
        file->smp_loop_end = bswapLE32(its->loopend);
        file->smp_speed = bswapLE32(its->C5Speed);
        file->smp_sustain_start = bswapLE32(its->susloopbegin);
        file->smp_sustain_end = bswapLE32(its->susloopend);

        file->smp_filename = (char *)mem_alloc(13);
        memcpy(file->smp_filename, its->filename, 12);
        file->smp_filename[12] = 0;

        file->description = "Impulse Tracker Sample";
        file->title = (char *)mem_alloc(26);
        memcpy(file->title, data + 20, 25);
        file->title[25] = 0;
        file->type = TYPE_SAMPLE_EXTD;

        return 1;
}

int load_its_sample(const uint8_t *header, const uint8_t *data, size_t length, song_sample *smp, char *title)
{
        ITSAMPLESTRUCT *its = (ITSAMPLESTRUCT *)header;
        uint32_t format;
        uint32_t bp;

        if (length < 80 || strncmp((const char *) header, "IMPS", 4) != 0)
                return 0;
        /* alright, let's get started */
        smp->length = bswapLE32(its->length);
        if ((its->flags & 1) == 0) {
                // sample associated with header
                return 0;
        }

        // endianness (always little)
        format = SF_LE;
        if (its->flags & 8) {
                // no such thing as compressed stereo
                // (TODO perhaps test with various players to see how this is implemented)
                format |= SF_M;
                // compression algorithm
                format |= (its->cvt & 4) ? SF_IT215 : SF_IT214;
        } else {
                // channels
                format |= (its->flags & 4) ? SF_SS : SF_M;
                // signedness (or delta?)
                format |= (its->cvt & 4) ? SF_PCMD : (its->cvt & 1) ? SF_PCMS : SF_PCMU;
        }
        // bit width
        format |= (its->flags & 2) ? SF_16 : SF_8;

        smp->global_volume = its->gvl;
        if (its->flags & 16) {
                smp->flags |= SAMP_LOOP;
                if (its->flags & 64)
                        smp->flags |= SAMP_LOOP_PINGPONG;
        }
        if (its->flags & 32) {
                smp->flags |= SAMP_SUSLOOP;
                if (its->flags & 128)
                        smp->flags |= SAMP_SUSLOOP_PINGPONG;
        }
        smp->volume = its->vol * 4;
        strncpy(title, (const char *) its->name, 25);
        smp->panning = (its->dfp & 127) * 4;
        if (its->dfp & 128)
                smp->flags |= SAMP_PANNING;
        smp->loop_start = bswapLE32(its->loopbegin);
        smp->loop_end = bswapLE32(its->loopend);
        smp->speed = bswapLE32(its->C5Speed);
        smp->sustain_start = bswapLE32(its->susloopbegin);
        smp->sustain_end = bswapLE32(its->susloopend);

        int vibs[] = {VIB_SINE, VIB_RAMP_DOWN, VIB_SQUARE, VIB_RANDOM};
        smp->vib_type = vibs[its->vit & 3];
        smp->vib_rate = its->vir;
        smp->vib_depth = its->vid;
        smp->vib_speed = its->vis;

        // sanity checks
        // (I should probably have more of these in general)
        if (smp->loop_start > smp->length) {
                smp->loop_start = smp->length;
                smp->flags &= ~(SAMP_LOOP | SAMP_LOOP_PINGPONG);
        }
        if (smp->loop_end > smp->length) {
                smp->loop_end = smp->length;
                smp->flags &= ~(SAMP_LOOP | SAMP_LOOP_PINGPONG);
        }
        if (smp->sustain_start > smp->length) {
                smp->sustain_start = smp->length;
                smp->flags &= ~(SAMP_SUSLOOP | SAMP_SUSLOOP_PINGPONG);
        }
        if (smp->sustain_end > smp->length) {
                smp->sustain_end = smp->length;
                smp->flags &= ~(SAMP_SUSLOOP | SAMP_SUSLOOP_PINGPONG);
        }

        bp = bswapLE32(its->samplepointer);

        // dumb casts :P
        return csf_read_sample((SONGSAMPLE *) smp, format,
                        (const char *) (data + bp),
                        (uint32_t) (length - bp));
}

int fmt_its_load_sample(const uint8_t *data, size_t length, song_sample *smp, char *title)
{
        return load_its_sample(data,data,length,smp,title);
}

void save_its_header(disko_t *fp, song_sample *smp, char *title)
{
        ITSAMPLESTRUCT its;

        memset(&its, 0, sizeof(its));
        its.id = bswapLE32(0x53504D49); // IMPS
        strncpy((char *) its.filename, (char *) smp->filename, 12);
        its.gvl = smp->global_volume;
        if (smp->data && smp->length)
                its.flags |= 1;
        if (smp->flags & SAMP_16_BIT)
                its.flags |= 2;
        if (smp->flags & SAMP_STEREO)
                its.flags |= 4;
        if (smp->flags & SAMP_LOOP)
                its.flags |= 16;
        if (smp->flags & SAMP_SUSLOOP)
                its.flags |= 32;
        if (smp->flags & SAMP_LOOP_PINGPONG)
                its.flags |= 64;
        if (smp->flags & SAMP_SUSLOOP_PINGPONG)
                its.flags |= 128;
        its.vol = smp->volume / 4;
        strncpy((char *) its.name, title, 25);
        its.name[25] = 0;
        its.cvt = 1;                    // signed samples
        its.dfp = smp->panning / 4;
        if (smp->flags & SAMP_PANNING)
                its.dfp |= 0x80;
        its.length = bswapLE32(smp->length);
        its.loopbegin = bswapLE32(smp->loop_start);
        its.loopend = bswapLE32(smp->loop_end);
        its.C5Speed = bswapLE32(smp->speed);
        its.susloopbegin = bswapLE32(smp->sustain_start);
        its.susloopend = bswapLE32(smp->sustain_end);
        //its.samplepointer = 42; - this will be filled in later
        its.vis = smp->vib_speed;
        its.vir = smp->vib_rate;
        its.vid = smp->vib_depth;
        switch (smp->vib_type) {
                case VIB_RANDOM:    its.vit = 3; break;
                case VIB_SQUARE:    its.vit = 2; break;
                case VIB_RAMP_DOWN: its.vit = 1; break;
                default:
                case VIB_SINE:      its.vit = 0; break;
        }

        disko_write(fp, &its, sizeof(its));
}

int fmt_its_save_sample(disko_t *fp, song_sample *smp, char *title)
{
        save_its_header(fp, smp, title);
        save_sample_data_LE(fp, smp, 1);

        /* Write the sample pointer. In an ITS file, the sample data is right after the header,
        so its position in the file will be the same as the size of the header. */
        unsigned int tmp = bswapLE32(sizeof(ITSAMPLESTRUCT));
        disko_seek(fp, 0x48, SEEK_SET);
        disko_write(fp, &tmp, 4);

        return 1;
}
