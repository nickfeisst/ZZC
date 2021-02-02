/*
 * Adopted from https://github.com/surge-synthesizer/surge
 */

#include "Wavetable.hpp"
#include <assert.h>
#include <cstring>
#include <iostream>

#if ARCH_WIN
#include <intrin.h>
#endif

using namespace std;

const int FIRipolI16_N = 8;
const int FIRoffsetI16 = FIRipolI16_N >> 1;

#define vt_read_int32LE vt_write_int32LE
#define vt_read_int32BE vt_write_int32BE
#define vt_read_int16LE vt_write_int16LE
#define vt_read_float32LE vt_write_float32LE

inline int vt_write_int32LE(int t) { return t; }

inline float vt_write_float32LE(float f) { return f; }

inline int vt_write_int32BE(int t)
{
#if (ARCH_LIN || ARCH_MAC)
    // this was `swap_endian`:
    return ((t << 24) & 0xff000000) | ((t << 8) & 0x00ff0000) | ((t >> 8) & 0x0000ff00) |
           ((t >> 24) & 0x000000ff);
#else
    // return _byteswap_ulong(t);
    return __builtin_bswap32(t);
#endif
}

unsigned int limit_range(unsigned int x, unsigned int l, unsigned int h)
{
    return std::max(std::min(x, h), l);
}

inline short vt_write_int16LE(short t) { return t; }

inline void vt_copyblock_W_LE(short *dst, const short *src, size_t count)
{
    memcpy(dst, src, count * sizeof(short));
}

inline void vt_copyblock_DW_LE(int *dst, const int *src, size_t count)
{
    memcpy(dst, src, count * sizeof(int));
}

void float2i15_block(float *f, short *s, int n)
{
    for (int i = 0; i < n; i++)
    {
        s[i] = (short)(int)limit_range((int)((float)f[i] * 16384.f), -16384, 16383);
    }
}

void i152float_block(short *s, float *f, int n)
{
    const float scale = 1.f / 16384.f;
    for (int i = 0; i < n; i++)
    {
        f[i] = (float)s[i] * scale;
    }
}

void i16toi15_block(short *s, short *o, int n)
{
    for (int i = 0; i < n; i++)
    {
        o[i] = s[i] >> 1;
    }
}

int min_F32_tables = 3;

#if ARCH_MAC || ARCH_LIN
bool _BitScanReverse(unsigned int *result, unsigned int bits)
{
    *result = __builtin_ctz(bits);
    return true;
}
#endif

//! Calculate the worst-case scenario of the needed samples for a specific wavetable and see if it
//! fits
size_t RequiredWTSize(int TableSize, int TableCount)
{
    int Size = 0;

    TableCount += 3; // for sample padding. Should match the "3" in the AppendSilence block below.
    while (TableSize > 0)
    {
        Size += TableCount * (TableSize + FIRoffsetI16 + FIRipolI16_N);

        TableSize = TableSize >> 1;
    }
    return Size;
}

int GetWTIndex(int WaveIdx, int WaveSize, int NumWaves, int MipMap, int Padding = 0)
{
    int Index = WaveIdx * ((WaveSize >> MipMap) + Padding);
    int Offset = NumWaves * WaveSize;
    for (int i = 0; i < MipMap; i++)
    {
        Index += Offset >> i;
        Index += Padding * NumWaves;
    }
    assert((Index + WaveSize - 1) < max_wtable_samples);
    return Index;
}

Wavetable::Wavetable()
{
    dataSizes = 35000;
    TableF32Data = (float *)malloc(dataSizes * sizeof(float));
    TableI16Data = (short *)malloc(dataSizes * sizeof(short));
    memset(TableF32Data, 0, dataSizes * sizeof(float));
    memset(TableI16Data, 0, dataSizes * sizeof(short));
    memset(TableF32WeakPointers, 0, sizeof(TableF32WeakPointers));
    memset(TableI16WeakPointers, 0, sizeof(TableI16WeakPointers));
    current_id = -1;
    queue_id = -1;
    refresh_display = true; // I have never been drawn so assume I need refresh if asked
}

Wavetable::~Wavetable()
{
    free(TableF32Data);
    free(TableI16Data);
}

void Wavetable::allocPointers(size_t newSize)
{
    free(TableF32Data);
    free(TableI16Data);
    dataSizes = newSize;
    TableF32Data = (float *)malloc(dataSizes * sizeof(float));
    TableI16Data = (short *)malloc(dataSizes * sizeof(short));
    memset(TableF32Data, 0, dataSizes * sizeof(float));
    memset(TableI16Data, 0, dataSizes * sizeof(short));
}

bool Wavetable::BuildWT(void *wdata, wt_header &wh, bool AppendSilence)
{
    assert(wdata);

    std::cout << "Flags: " << wh.flags << std::endl;

    flags = vt_read_int16LE(wh.flags);
    n_tables = vt_read_int16LE(wh.n_tables);
    size = vt_read_int32LE(wh.n_samples);

    size_t req_size = RequiredWTSize(size, n_tables);

    if (req_size > dataSizes)
    {
        allocPointers(req_size);
    }

    int wdata_tables = n_tables;

    if (AppendSilence)
    {
        n_tables += 3; // this "3" should match the "3" in RequiredWTSize
    }

#if WINDOWS
    unsigned long MSBpos;
    _BitScanReverse(&MSBpos, size);
#else
    unsigned int MSBpos;
    _BitScanReverse(&MSBpos, size);
#endif

    size_po2 = MSBpos;

    dt = 1.0f / size;

    for (int i = 0; i < max_mipmap_levels; i++)
    {
        for (int j = 0; j < max_subtables; j++)
        {
            // TODO WARNING: Crashes here with patchbyte!
            /*free(wt->TableF32WeakPointers[i][j]);
            free(wt->TableI16WeakPointers[i][j]);
            wt->TableF32WeakPointers[i][j] = 0;
            wt->TableI16WeakPointers[i][j] = 0;*/
        }
    }
    for (int j = 0; j < this->n_tables; j++)
    {
        TableF32WeakPointers[0][j] = TableF32Data + GetWTIndex(j, size, n_tables, 0);
        TableI16WeakPointers[0][j] =
            TableI16Data + GetWTIndex(j, size, n_tables, 0,
                                      FIRipolI16_N); // + padding for a "non-wrapping" interpolator
    }
    for (int j = this->n_tables; j < min_F32_tables;
         j++) // W-TABLE need at least 3 tables to work properly
    {
        unsigned int s = this->size;
        int l = 0;
        while (s && (l < max_mipmap_levels))
        {
            TableF32WeakPointers[l][j] = TableF32Data + GetWTIndex(j, size, n_tables, l);
            memset(TableF32WeakPointers[l][j], 0, s * sizeof(float));
            s = s >> 1;
            l++;
        }
    }

    if (this->flags & wtf_int16)
    {
        for (int j = 0; j < wdata_tables; j++)
        {
            vt_copyblock_W_LE(&this->TableI16WeakPointers[0][j][FIRoffsetI16],
                              &((short *)wdata)[this->size * j], this->size);
            if (this->flags & wtf_int16_is_16)
            {
                i16toi15_block(&this->TableI16WeakPointers[0][j][FIRoffsetI16],
                               &this->TableI16WeakPointers[0][j][FIRoffsetI16], this->size);
            }
            i152float_block(&this->TableI16WeakPointers[0][j][FIRoffsetI16],
                            this->TableF32WeakPointers[0][j], this->size);
        }
    }
    else
    {
        for (int j = 0; j < wdata_tables; j++)
        {
            vt_copyblock_DW_LE((int *)this->TableF32WeakPointers[0][j],
                               &((int *)wdata)[this->size * j], this->size);
            float2i15_block(this->TableF32WeakPointers[0][j],
                            &this->TableI16WeakPointers[0][j][FIRoffsetI16], this->size);
        }
    }

    // clear any appended tables (not read, but included in table for post-silence)
    for (int j = wdata_tables; j < this->n_tables; j++)
    {
        memset(this->TableF32WeakPointers[0][j], 0, this->size * sizeof(float));
        memset(this->TableI16WeakPointers[0][j], 0, this->size * sizeof(short));
    }

    for (int j = 0; j < wdata_tables; j++)
    {
        memcpy(&this->TableI16WeakPointers[0][j][this->size + FIRoffsetI16],
               &this->TableI16WeakPointers[0][j][FIRoffsetI16], FIRoffsetI16 * sizeof(short));
        memcpy(&this->TableI16WeakPointers[0][j][0], &this->TableI16WeakPointers[0][j][this->size],
               FIRoffsetI16 * sizeof(short));
    }

    return true;
}
