// cdg.cpp
//
// CD+G decoder from cdgdeck by Isaac Brodsky.
// Bundled under BSD-3-Clause license, see LICENSE.cdgdeck for details.
//
// Original source: https://github.com/isaacbrodsky/cdgdeck
//

#include <iostream>

using namespace std;

#include "cdg.h"

CDG::CDG()
{
	for (int i = 0; i < CDG_NUM_COLORS; i++)
		for (int channel = 0; channel < CDG_NUM_COLOR_CHANNELS; channel++)
			colorTable[i][channel] = 0x00;

	for (int x = 0; x < CDG_WIDTH; x++)
		for (int y = 0; y < CDG_HEIGHT; y++)
			putPixel(x, y, 0);

	ph = pv = 0;
	border = 0;
	channel = 1;
}

bool CDG::operator==(const CDG& other) const
{
	bool equal = true;

	for (int i = 0; i < CDG_NUM_COLORS; i++)
	{
		for (int channel = 0; channel < CDG_NUM_COLOR_CHANNELS; channel++)
		{
			if (colorTable[i][channel] != other.colorTable[i][channel])
			{
				equal = false;
				break;
			}
		}
		if (!equal)
			break;
	}

	equal &= (ph == other.ph);
	equal &= (pv == other.pv);
	equal &= (border == other.border);

	if (equal)
	{
		for (int x = 0; x < CDG_WIDTH; x++)
		{
			for (int y = 0; y < CDG_HEIGHT; y++)
			{
				if (getPixel(x, y) != other.getPixel(x, y))
				{
					equal = false;
					break;
				}
			}
			if (!equal)
				break;
		}
	}

	return equal;
}

bool CDG::readNext(istream &in, SubCode &out)
{
	bool success = true;

	out.command = SubCode_Command::SCCMD_NONE;

	in.read((char*)&out, sizeof(SubCode));
	if (!in.good())
	{
		success = false;
	}

	return success;
}

void CDG::loadColor(int idx, char byte0, char byte1)
{
	uint8_t high = byte0 & LOWER_6_BITS;
	uint8_t low = byte1 & LOWER_6_BITS;

	uint8_t r = (high >> 2);
	uint8_t g = (((high & LOWER_2_BITS) << 2) | (low >> 4));
	uint8_t b = (low & LOWER_4_BITS);

	r *= 17;
	g *= 17;
	b *= 17;

	if (idx >= 0 && idx < CDG_NUM_COLORS)
	{
		colorTable[idx][0] = r;
		colorTable[idx][1] = g;
		colorTable[idx][2] = b;
	}
}

void CDG::execLoadct(const SubCode &subCode)
{
	int offset = ((subCode.instruction & LOWER_6_BITS) == CDG_LOADCTHIGH) ? 8 : 0;

	for (int i = 0; i < 8; i++)
		loadColor(i + offset, subCode.data.clutDat.colorSpec[2 * i], subCode.data.clutDat.colorSpec[2 * i + 1]);
}

void CDG::execTransparent(const SubCode &subCode)
{
	for (int i = 0; i < 16; i++)
	{
		colorTable[i][3] =
			((subCode.data.transparentDat.alphaChannel[i] & LOWER_6_BITS)
			<< 2);
	}
}

void CDG::fillPixels(int xs, int ys, int xe, int ye, uint8_t color)
{
	for (int x = xs; x < xe; x++)
	{
		for (int y = ys; y < ye; y++)
		{
			putPixel(x, y, color);
		}
	}
}

void CDG::putPixel(int x, int y, uint8_t color, bool isXor)
{
	if (x < CDG_WIDTH && x >= 0
		&& y < CDG_HEIGHT && y >= 0)
	{
#ifdef SHRINK_CDG
		uint8_t newcolor;
		if (isXor)
		{
			newcolor = getPixel(x, y);
			newcolor ^= color;
		}
		else
		{
			newcolor = color;
		}

		if (y % 2 == 1)
			screen[x][y / 2] = (screen[x][y / 2] & LOWER_4_BITS) | (newcolor << 4);
		else
			screen[x][y / 2] = (screen[x][y / 2] & ~LOWER_4_BITS) | newcolor;
#else
		if (isXor)
			screen[x][y] ^= color;
		else
			screen[x][y] = color;
#endif
	}
}

void CDG::getColor(uint8_t code, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) const
{
	if (code >= CDG_NUM_COLORS || code < 0)
	{
		r = g = b = a = 0;
	}
	else
	{
		r = colorTable[code][0];
		g = colorTable[code][1];
		b = colorTable[code][2];
		a = colorTable[code][3];
	}
}

uint8_t CDG::getPixel(int x, int y) const
{
	uint8_t ret;

	if (x < CDG_WIDTH && x >= 0
		&& y < CDG_HEIGHT && y >= 0)
	{
#ifdef SHRINK_CDG
		if (y % 2 == 1)
			ret = screen[x][y / 2] >> 4;
		else
			ret = screen[x][y / 2] & LOWER_4_BITS;
#else
		ret = screen[x][y];
#endif
	}
	else
	{
		ret = 0;
	}

	return ret;
}

void CDG::getPointers(uint8_t &v, uint8_t &h) const
{
	v = pv;
	h = ph;
}

uint8_t CDG::getBorderColor() const
{
	return border;
}

uint8_t CDG::getChannel() const {
	return channel;
}

void CDG::setChannel(uint8_t newChannel) {
	channel = newChannel;
}

void CDG::execMemoryPreset(const SubCode &subCode)
{
	int color = subCode.data.memDat.color & LOWER_4_BITS;

	fillPixels(0, 0, CDG_WIDTH, CDG_HEIGHT, color);
	ph = pv = 0;
}

void CDG::execBorderPreset(const SubCode &subCode)
{
	border = subCode.data.borderDat.color & LOWER_4_BITS;
}

void CDG::execTile(const SubCode &subCode)
{
	uint8_t color[2];
	uint8_t channel;

	bool useXor = ((subCode.instruction & LOWER_6_BITS) == CDG_TILEBLOCKXOR);
	int point;

	int row = (subCode.data.tileDat.row & LOWER_5_BITS) * ROW_MULT;
	int col = (subCode.data.tileDat.column & LOWER_6_BITS) * COL_MULT;

	color[0] = subCode.data.tileDat.color0 & LOWER_4_BITS;
	color[1] = subCode.data.tileDat.color1 & LOWER_4_BITS;

	channel = (subCode.data.tileDat.color0 & LOWER_6_BITS) >> 4;
	channel = (channel << 2) | ((subCode.data.tileDat.color1 & LOWER_6_BITS) >> 4);

	if (channel != 0 && channel != this->channel) {
		return;
	}

	if (row < CDG_HEIGHT && col < CDG_WIDTH)
	{
		for (int y = 0; y < ROW_MULT; y++)
		{
			for (int x = 0; x < COL_MULT; x++)
			{
				point = ((subCode.data.tileDat.tilePixels[y] & LOWER_6_BITS) >> (5 - x))
							& 1;

				putPixel((col+x), (row+y), color[point], useXor);
			}
		}
	}
}

void CDG::swapPixels(int x1, int y1, int x2, int y2)
{
	uint8_t p1 = getPixel(x1, y1);
	uint8_t p2 = getPixel(x2, y2);
	putPixel(x2, y2, p1);
	putPixel(x1, y1, p2);
}

void CDG::rotateV(int cmd)
{
	int next;

	if (cmd == 2)
	{
		for (int y = 0; y < CDG_HEIGHT - ROW_MULT; y++)
		{
			next = y - ROW_MULT;
			if (next < 0)
				next += CDG_HEIGHT;

			for (int x = 0; x < CDG_WIDTH; x++)
				swapPixels(x, y, x, next);
		}
	}
	else if (cmd == 1)
	{
		for (int y = CDG_HEIGHT - 1; y >= ROW_MULT; y--)
		{
			next = y + ROW_MULT;
			if (next >= CDG_HEIGHT)
				next -= CDG_HEIGHT;

			for (int x = 0; x < CDG_WIDTH; x++)
				swapPixels(x, y, x, next);
		}
	}
}

void CDG::rotateH(int cmd)
{
	int next;

	if (cmd == 2)
	{
		for (int x = 0; x < CDG_WIDTH - COL_MULT; x++)
		{
			next = x - COL_MULT;
			if (next < 0)
				next += CDG_WIDTH;

			for (int y = 0; y < CDG_HEIGHT; y++)
				swapPixels(x, y, next, y);
		}
	}
	else if (cmd == 1)
	{
		for (int x = CDG_WIDTH - 1; x >= COL_MULT; x--)
		{
			next = x + COL_MULT;
			if (next >= CDG_WIDTH)
				next -= CDG_WIDTH;

			for (int y = 0; y < CDG_HEIGHT; y++)
				swapPixels(x, y, next, y);
		}
	}
}

void CDG::execScroll(const SubCode &subCode)
{
	uint8_t color = subCode.data.scrollDat.color & LOWER_4_BITS;
	uint8_t scrollH = subCode.data.scrollDat.hScroll & LOWER_6_BITS,
		scrollV = subCode.data.scrollDat.vScroll & LOWER_6_BITS;
	uint8_t cmdH = (scrollH & 0x30) >> 4;
	uint8_t offsetH = (scrollH & 0x07);
	uint8_t cmdV = (scrollV & 0x30) >> 4;
	uint8_t offsetV = (scrollV & 0x0F);

	ph = offsetH;
	pv = offsetV;

	if (cmdH)
		rotateH(cmdH);
	if (cmdV)
		rotateV(cmdV);

	if ((subCode.instruction & LOWER_6_BITS) == CDG_SCROLLPRESET)
	{
		if (cmdH == 1)
			fillPixels(0, 0, COL_MULT, CDG_HEIGHT, color);
		if (cmdH == 2)
			fillPixels(CDG_WIDTH - COL_MULT, 0, COL_MULT, CDG_HEIGHT, color);
		if (cmdV == 1)
			fillPixels(0, 0, CDG_WIDTH, ROW_MULT, color);
		if (cmdV == 2)
			fillPixels(0, CDG_HEIGHT - ROW_MULT, CDG_WIDTH, ROW_MULT, color);
	}
}

int CDG::execCount(istream &in, int count, int &dirty)
{
	bool success = true;
	SubCode code;

	for (int i = 0; i < count; i++)
	{
		if (readNext(in, code))
		{
			execNext(code, dirty);
		}
		else
		{
			success = false;
			break;
		}
	}

	return success;
}

void CDG::execNext(const SubCode &subCode)
{
	int i = 0;
	execNext(subCode, i);
}

void CDG::execNext(const SubCode &subCode, int &dirty)
{
	if ((subCode.command & LOWER_6_BITS) == SubCode_Command::SCCMD_CDG)
	{
		switch (subCode.instruction & LOWER_6_BITS)
		{
		case CDG_MEMORYPRESET:
			execMemoryPreset(subCode);
			break;
		case CDG_BORDERPRESET:
			execBorderPreset(subCode);
			break;
		case CDG_TILEBLOCK:
			execTile(subCode);
			break;
		case CDG_SCROLLPRESET:
			execScroll(subCode);
			break;
		case CDG_SCROLLCOPY:
			execScroll(subCode);
			break;
		case CDG_TRANSPARENT:
			execTransparent(subCode);
			break;
		case CDG_LOADCTLOW:
			execLoadct(subCode);
			break;
		case CDG_LOADCTHIGH:
			execLoadct(subCode);
			break;
		case CDG_TILEBLOCKXOR:
			execTile(subCode);
			break;
		default:
			break;
		}
		dirty++;
	}
}

void CDG::seekTo(istream &in, int loc, SeekMode mode)
{
	int dirty = 0;
	switch (mode)
	{
	case SEEK_ENHANCED:
		in.seekg(0, ios::beg);
		pv = ph = 0;
		execCount(in, (loc / sizeof(SubCode)), dirty);
		break;
	case SEEK_DIRECT:
	default:
		in.seekg(loc, ios::beg);
		break;
	}
}

int CDG::sizeToSeconds(int f)
{
	f /= 96;
	f /= 75;
	return f;
}

int CDG::percentToSecond(double percent, int total)
{
	int max = int(percent * total);
	int ret = 0;

	while (ret < max)
	{
		ret += BYTES_PER_SECOND;
	}

	return ret - BYTES_PER_SECOND;
}
