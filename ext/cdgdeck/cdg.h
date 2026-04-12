// cdg.h
//
// CD+G decoder from cdgdeck by Isaac Brodsky.
// Bundled under BSD-3-Clause license, see LICENSE.cdgdeck for details.
//
// Original source: https://github.com/isaacbrodsky/cdgdeck
//

#ifndef CDG_H
#define CDG_H

#include <iosfwd>

#include <stdint.h>

//***************************************************************************
// Constants and compilation options.
//***************************************************************************

#define SHRINK_CDG					// use space saving memory layout.

#define BYTES_PER_SECOND (96 * 75)	// number of bytes of subcode information
									// per second of audio.

#define CDG_NUM_COLORS 16
#define CDG_NUM_COLOR_CHANNELS 4

#define CDG_WIDTH 300				// size in pixels
#define CDG_HEIGHT 216				//

#define ROW_MULT 12					// Size of each discrete block ("font")
#define COL_MULT 6					// of CDG data.
									// Bit masks
#define LOWER_6_BITS 0x3F			// 63
#define LOWER_5_BITS 0x1F			// (LOWER_6_BITS >> 1)
#define LOWER_4_BITS 0x0F			// (LOWER_6_BITS >> 2)
#define LOWER_3_BITS 0x07			// (LOWER_6_BITS >> 3)
#define LOWER_2_BITS 0x03			// (LOWER_6_BITS >> 4)

//***************************************************************************
// Represents the mode a seek operation will use.
//***************************************************************************
enum SeekMode
{
	SEEK_DIRECT,
	SEEK_ENHANCED
};

//***************************************************************************
// Represents possible Subcode commands that this program can process
//***************************************************************************
enum SubCode_Command : char
{
	SCCMD_NONE			= 0x00,
	SCCMD_CDG			= 0x09
};

//***************************************************************************
// This type represents possible CDG command codes.
//***************************************************************************
enum CDG_Instruction : char
{
	CDG_MEMORYPRESET	=  1,
	CDG_BORDERPRESET	=  2,
	CDG_TILEBLOCK		=  6,
	CDG_SCROLLPRESET	= 20,
	CDG_SCROLLCOPY		= 24,
	CDG_TRANSPARENT		= 28,
	CDG_LOADCTLOW		= 30,
	CDG_LOADCTHIGH		= 31,
	CDG_TILEBLOCKXOR	= 38
};

#pragma pack(push, 1)

//***************************************************************************
// Structure of the data field in MEMORYPRESET CDG commands.
//***************************************************************************
struct CDG_MemPreset
{
	char	color;
	char	repeat;
	char	padding[14];
};

//***************************************************************************
// Structure of the data field in BORDERPRESET CDG commands.
//***************************************************************************
struct CDG_BorderPreset
{
	char	color;
	char	padding[15];
};

//***************************************************************************
// Structure of the data field in FONT, and FONTXOR CDG commands.
//***************************************************************************
struct CDG_Tile
{
	char	color0;
	char	color1;
	char	row;
	char	column;
	char	tilePixels[12];
};

//***************************************************************************
// Structure of the data field in SCROLLCOPY and SCROLLPRESET CDG commands.
//***************************************************************************
struct CDG_Scroll
{
	char	color;
	char	hScroll;
	char	vScroll;
	char	padding[13];
};

//***************************************************************************
// Structure of the data field in TRANSPARENCY CDG commands.
//***************************************************************************
struct CDG_Transparent
{
	char	alphaChannel[16];
};

//***************************************************************************
// Structure of the data field in LOADCLUTHIGH and LOADCLUTLOW CDG commands.
//***************************************************************************
struct CDG_LoadCLUT
{
	char colorSpec[16];
};

//***************************************************************************
// Structure of a single CDG command. These are stored four in a packet on
// disk. This subcode data is stored in the R-W subcode channels of digital
// audio CDs.
//
// Only the lower six bits of any byte in this structure should be used.
//***************************************************************************
struct SubCode
{
	SubCode_Command		command;
	CDG_Instruction		instruction;
	char				parityQ[2];
	union
	{
		char				data[16];
		CDG_MemPreset		memDat;
		CDG_BorderPreset	borderDat;
		CDG_Tile			tileDat;
		CDG_Scroll			scrollDat;
		CDG_Transparent		transparentDat;
		CDG_LoadCLUT		clutDat;
	} data;
	char				parityP[4];
};

#pragma pack(pop)

static_assert(sizeof(SubCode) == 24, "SubCode must be exactly 24 bytes to match CDG file format");

//***************************************************************************
// Represents a CD+G decoder.
//***************************************************************************
class CDG
{
private:
	uint8_t colorTable[CDG_NUM_COLORS][CDG_NUM_COLOR_CHANNELS];
#ifdef SHRINK_CDG
	uint8_t screen[CDG_WIDTH][CDG_HEIGHT/2];
#else
	uint8_t screen[CDG_WIDTH][CDG_HEIGHT];
#endif

	uint8_t ph, pv;
	uint8_t border;
	uint8_t channel;

	static bool readNext(std::istream &in, SubCode &out);

	void swapPixels(int x1, int y1, int x2, int y2);
	void rotateV(int cmd);
	void rotateH(int cmd);
	void loadColor(int idx, char byte0, char byte1);

	void fillPixels(int x1, int y1, int x2, int y2, uint8_t color);
	void putPixel(int x, int y, uint8_t color, bool isXor = false);

	void execLoadct(const SubCode&);
	void execMemoryPreset(const SubCode&);
	void execBorderPreset(const SubCode&);
	void execScroll(const SubCode&);
	void execTransparent(const SubCode&);
	void execTile(const SubCode&);
public:

	CDG();

	bool operator==(const CDG&) const;

	void getColor(uint8_t code, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) const;
	uint8_t getPixel(int x, int y) const;
	void getPointers(uint8_t &v, uint8_t &h) const;
	uint8_t getBorderColor() const;

	uint8_t getChannel() const;
	void setChannel(uint8_t);

	void execNext(const SubCode&);
	void execNext(const SubCode&, int&);
	int execCount(std::istream &in, int count, int &dirty);
	void seekTo(std::istream &in, int loc, SeekMode mode);

	static int sizeToSeconds(int sz);
	static int percentToSecond(double percent, int total);
};

#endif
