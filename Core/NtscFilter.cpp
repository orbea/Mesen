#include "stdafx.h"
#include "NtscFilter.h"
#include "PPU.h"
#include "EmulationSettings.h"

NtscFilter::NtscFilter()
{
	_ntscData = new nes_ntsc_t();
	_ntscSetup = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	nes_ntsc_init(_ntscData, &_ntscSetup);
}

FrameInfo NtscFilter::GetFrameInfo()
{
	OverscanDimensions overscan = GetOverscan();
	int overscanLeft = overscan.Left > 0 ? NES_NTSC_OUT_WIDTH(overscan.Left) : 0;
	int overscanRight = overscan.Right > 0 ? NES_NTSC_OUT_WIDTH(overscan.Right) : 0;
	return { NES_NTSC_OUT_WIDTH(PPU::ScreenWidth) - overscanLeft - overscanRight, (PPU::ScreenHeight - overscan.Top - overscan.Bottom) * 2, 4 };
}

void NtscFilter::ApplyFilter(uint16_t *ppuOutputBuffer)
{
	static bool oddFrame = false;
	oddFrame = !oddFrame;

	uint32_t* ntscBuffer = new uint32_t[NES_NTSC_OUT_WIDTH(256) * 240];
	nes_ntsc_blit(_ntscData, ppuOutputBuffer, PPU::ScreenWidth, oddFrame ? 0 : 1, PPU::ScreenWidth, 240, ntscBuffer, NES_NTSC_OUT_WIDTH(PPU::ScreenWidth)*4);
	DoubleOutputHeight(ntscBuffer);
	delete[] ntscBuffer;
}

void NtscFilter::DoubleOutputHeight(uint32_t *ntscBuffer)
{
	uint32_t* outputBuffer = (uint32_t*)GetOutputBuffer();
	OverscanDimensions overscan = GetOverscan();
	int overscanLeft = overscan.Left > 0 ? NES_NTSC_OUT_WIDTH(overscan.Left) : 0;
	int overscanRight = overscan.Right > 0 ? NES_NTSC_OUT_WIDTH(overscan.Right) : 0;
	int rowWidth = NES_NTSC_OUT_WIDTH(PPU::ScreenWidth);
	int rowWidthOverscan = rowWidth - overscanLeft - overscanRight;

	for(int y = PPU::ScreenHeight - 1 - overscan.Bottom; y >= (int)overscan.Top; y--) {
		uint32_t const* in = ntscBuffer + y * rowWidth;
		uint32_t* out = outputBuffer + (y - overscan.Top) * 2 * rowWidthOverscan;

		for(int x = 0; x < rowWidthOverscan; x++) {
			uint32_t prev = in[overscanLeft];
			uint32_t next = y < 239 ? in[overscanLeft + rowWidth] : 0;
			
			/* mix 24-bit rgb without losing low bits */
			uint64_t mixed = prev + next + ((prev ^ next) & 0x030303);
			/* darken color */
			*out = prev;
			*(out + rowWidthOverscan) = (uint32_t)((mixed >> 1) - (mixed >> 4 & 0x0F0F0F));
			in++;
			out++;
		}
	}
}

NtscFilter::~NtscFilter()
{
	delete _ntscData;
}