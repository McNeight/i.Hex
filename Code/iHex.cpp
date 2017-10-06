/*hdr
**	FILE:			iHex.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			7/5/2002
**	DESCRIPTION:	Hex viewer/editor
**
**	Copyright (C) 2002, Matthew Allen
**		fret@memecode.com
*/


/*

Hex view line format:

	Hex: [2ch hex][:][8ch hex][2ch space][3*16=48ch hex bytes][2ch space][16ch ascii]
	Bin: [11ch decimal       ][2ch space][3*16=48ch hex bytes][2ch space][16ch ascii]

*/

#define _WIN32_WINNT 0x0400
#include "iHex.h"
#include "GToken.h"
#include "GAbout.h"
#include "resdefs.h"
#include "GCombo.h"
#include "GScrollBar.h"
#include "GProgressDlg.h"
#include "GDisplayString.h"
#ifdef WIN32
#include "wincrypt.h"
#endif
#include "GClipBoard.h"
#include "Diff.h"
#include "LgiRes.h"
#include "iHexView.h"

///////////////////////////////////////////////////////////////////////////////////////////////
// Application identification
const char *AppName = "i.Hex";
const char *Untitled = "Untitled Document";
bool CancelSearch = false;

#define DEBUG_COVERAGE_CHECK		0

#define ColourSelectionFore			Rgb24(255, 255, 0)
#define ColourSelectionBack			Rgb24(0, 0, 255)
#define	CursorColourBack			Rgb24(192, 192, 192)

#define HEX_COLUMN					13 // characters, location of first files hex column
#define TEXT_COLUMN					(HEX_COLUMN + (3 * BytesPerLine) + GAP_HEX_ASCII)
#define GAP_HEX_ASCII				2 // characters, this is the gap between the hex and ascii columns
#define GAP_FILES					6 // characters, this is the gap between 2 files when comparing

#define FILE_BUFFER_SIZE			1024
#define	UI_UPDATE_SPEED				500 // ms

GColour ChangedFore(0xf1, 0xe2, 0xad);
GColour ChangedBack(0xef, 0xcb, 0x05);
GColour DeletedBack(0xc0, 0xc0, 0xc0);

///////////////////////////////////////////////////////////////////////////////////////////////
class RandomData : public GStream
{
	#ifdef WIN32
	typedef BOOLEAN (APIENTRY *RtlGenRandom)(void*, ULONG);
	HCRYPTPROV	phProv;
	HMODULE		hADVAPI32;
	RtlGenRandom GenRandom;
	#endif

public:
	RandomData()
	{
		#ifdef WIN32
		phProv = 0;
		hADVAPI32 = 0;
		GenRandom = 0;

		if (!CryptAcquireContext(&phProv, 0, 0, PROV_RSA_FULL, 0))
		{
			// f***ing windows... try a different strategy.
			hADVAPI32 = LoadLibrary("ADVAPI32.DLL");
			if (hADVAPI32)
			{
				GenRandom = (RtlGenRandom) GetProcAddress(hADVAPI32, "SystemFunction036");
			}
		}			
		#endif
	}

	~RandomData()
	{
		#ifdef WIN32
		if (phProv)
			CryptReleaseContext(phProv, 0);
		else if (hADVAPI32)
			FreeLibrary(hADVAPI32);
		#endif
	}

	ssize_t Read(void *Ptr, ssize_t Len, int Flags = 0)
	{
		#ifdef WIN32
		if (phProv)
		{
			if (CryptGenRandom(phProv, Len, (uchar*)Ptr))
				return Len;
		}
		else if (GenRandom)
		{
			if (GenRandom(Ptr, Len))
				return Len;
		}
		#endif
		return 0;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
class ChangeSizeDlg : public GDialog
{
	int OldUnits;

public:
	int64 Size;

	ChangeSizeDlg(AppWnd *app, int64 size)
	{
		SetParent(app);
		if (LoadFromResource(IDD_CHANGE_FILE_SIZE))
		{
			MoveToCenter();

			GCombo *Units = dynamic_cast<GCombo*>(FindControl(IDC_UNITS));
			if (Units)
			{
				Units->Insert("bytes");
				Units->Insert("KB");
				Units->Insert("MB");
				Units->Insert("GB");

				if (size < 10 << 10)
				{
					SetCtrlValue(IDC_UNITS, 0);
				}
				else if (size < 1 << 20)
				{
					SetCtrlValue(IDC_UNITS, 1);
				}
				else if (size < 1 << 30)
				{
					SetCtrlValue(IDC_UNITS, 2);
				}
				else
				{
					SetCtrlValue(IDC_UNITS, 3);
				}

				SetBytes(size);
				OnNotify(FindControl(IDC_NUMBER), 0);
			}
		}
	}

	void SetBytes(int64 size)
	{
		switch (GetCtrlValue(IDC_UNITS))
		{
			case 0:
			{
				char s[64];
				sprintf(s, LGI_PrintfInt64, size);
				SetCtrlName(IDC_NUMBER, s);
				break;
			}
			case 1:
			{
				char s[64];
				double d = (double)size / 1024.0;
				sprintf(s, "%f", d);
				SetCtrlName(IDC_NUMBER, s);
				break;
			}
			case 2:
			{
				char s[64];
				double d = (double)size / 1024.0 / 1024.0;
				sprintf(s, "%f", d);
				SetCtrlName(IDC_NUMBER, s);
				break;
			}
			case 3:
			{
				char s[64];
				double d = (double)size / 1024.0 / 1024.0 / 1024.0;
				sprintf(s, "%f", d);
				SetCtrlName(IDC_NUMBER, s);
				break;
			}
		}

		OldUnits = GetCtrlValue(IDC_UNITS);
	}

	int64 GetBytes(int Units = -1)
	{
		int64 n = 0;
		char *s = GetCtrlName(IDC_NUMBER);
		if (s)
		{
			switch (Units >= 0 ? Units : GetCtrlValue(IDC_UNITS))
			{
				case 0: // bytes
				{
					n = atoi64(s);
					break;
				}
				case 1: // KB
				{
					n = (int64) (atof(s) * 1024.0);
					break;
				}
				case 2: // MB
				{
					n = (int64) (atof(s) * 1024.0 * 1024.0);
					break;
				}
				case 3: // GB
				{
					n = (int64) (atof(s) * 1024.0 * 1024.0 * 1024.0);
					break;
				}
			}
		}

		return n;
	}

	int OnNotify(GViewI *c, int f)
	{
		if (!c) return 0;
		switch (c->GetId())
		{
			case IDC_UNITS:
			{
				SetBytes(Size);
				break;
			}
			case IDC_NUMBER:
			{
				Size = GetBytes();

				char s[64];
				sprintf(s, "("LGI_PrintfInt64" bytes)", GetBytes());
				SetCtrlName(IDC_BYTE_SIZE, s);
				break;
			}
			case IDOK:
			case IDCANCEL:
			{
				EndModal(c->GetId() == IDOK);
				break;
			}
		}

		return 0;
	}
};

class IHexBar : public GLayout, public GLgiRes
{
	friend class AppWnd;

	AppWnd *App;
	GHexView *View;
	int Y;

public:
	bool NotifyOff;

	IHexBar(AppWnd *a, int y);
	~IHexBar();

	int64 GetOffset(int IsHex = -1, bool *Select = 0);
	void SetOffset(int64 Offset);
	bool IsSigned();
	bool IsLittleEndian();

	bool Pour(GRegion &r);
	void OnPaint(GSurface *pDC);
	int OnNotify(GViewI *c, int f);
};

/////////////////////////////////////////////////////////////////////////////////////
IHexBar::IHexBar(AppWnd *a, int y)
{
	App = a;
	View = 0;
	Y = y;
	_IsToolBar = true;
	NotifyOff = false;
	Attach(App);

	LoadFromResource(IDD_HEX, this);
	for (GViewI *c=Children.First(); c; c=Children.Next())
	{
		GRect r = c->GetPos();
		r.Offset(1, 4);
		c->SetPos(r);
	}
	AttachChildren();

	GVariant v;
	if (!a->GetOptions() || !a->GetOptions()->GetValue("IsHex", v))
		v = true;
	SetCtrlValue(IDC_IS_HEX, v.CastInt32());

	if (!a->GetOptions() || !a->GetOptions()->GetValue("LittleEndian", v))
		v = true;
	SetCtrlValue(IDC_LITTLE, v.CastInt32());

	SetCtrlValue(IDC_OFFSET, 0);
}

IHexBar::~IHexBar()
{
	if (App && App->GetOptions())
	{
		GVariant v;
		App->GetOptions()->SetValue("IsHex", v = GetCtrlValue(IDC_IS_HEX));
		App->GetOptions()->SetValue("LittleEndian", v = GetCtrlValue(IDC_LITTLE));
	}
}

bool IHexBar::Pour(GRegion &r)
{
	GRect *Best = FindLargestEdge(r, GV_EDGE_TOP);
	if (Best)
	{
		GRect r = *Best;
		if (r.Y() != Y)
		{
			r.y2 = r.y1 + Y - 1;
		}

		SetPos(r, true);

		return true;
	}
	return false;
}

void IHexBar::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();
	LgiThinBorder(pDC, r, DefaultRaisedEdge);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);

	#define Divider(x) \
		pDC->Colour(LC_LOW, 24); \
		pDC->Line(x, 1, x, pDC->Y()); \
		pDC->Colour(LC_WHITE, 24); \
		pDC->Line(x+1, 1, x+1, pDC->Y());

	Divider(134);
	Divider(268);
}

bool IHexBar::IsSigned()
{
	return GetCtrlValue(IDC_SIGNED);
}

bool IHexBar::IsLittleEndian()
{
	return GetCtrlValue(IDC_LITTLE);
}

void IHexBar::SetOffset(int64 Offset)
{
	if (GetCtrlValue(IDC_IS_HEX))
	{
		char s[64];
		
		#if 1
		int ch = 0;
		if (Offset >> 32)
			ch += sprintf_s(s + ch, sizeof(s) - ch, "%x", (unsigned) (Offset >> 32));
		ch += sprintf_s(s + ch, sizeof(s) - ch, "%x", (unsigned)Offset);
		#else
		// What is the point of this code?
		char *c = s;
		for (int i=0; i<16; i++)
		{
			int n = (Offset >> ((15 - i) * 4)) & 0xf;
			if (n || c > s)
			{
				c += sprintf(c, "%01.1X", n);
			}
		}
		*c++ = 0;
		#endif

		SetCtrlName(IDC_OFFSET, s);
	}
	else
	{
		SetCtrlValue(IDC_OFFSET, Offset);
	}
}

int64 IHexBar::GetOffset(int IsHex, bool *Select)
{
	int64 c = -1;
	GString s = GetCtrlName(IDC_OFFSET);
	GString Src;
	Src.Printf("return %s;", s.Get());
	
	GScriptEngine Eng(this, NULL, NULL);
	
	GVariant Ret;
	GCompiledCode Code;
	GExecutionStatus Status = Eng.RunTemporary(&Code, Src, &Ret);
	if (Status != ScriptError)
	{
		c = Ret.CastInt64();
	}

	return c;
}

int IHexBar::OnNotify(GViewI *c, int f)
{
	if (NotifyOff)
		return 0;

	switch (c->GetId())
	{
		case IDC_SIGNED:
		case IDC_LITTLE:
		{
			if (View)
			{
				View->DoInfo();
				View->Focus(true);
			}
			break;
		}
		case IDC_OFFSET:
		{
			if (View &&
				f == VK_RETURN)
			{
				// Set the cursor
				bool Select = false;
				int64 Off = GetOffset(-1, &Select);

				View->SetCursor(NULL, Select ? Off - 1 : Off, Select, Select);

				// Return focus to the view
				View->Focus(true);
			}
			break;
		}
		case IDC_IS_HEX:
		{
			if (View)
			{
				bool IsHex = GetCtrlValue(IDC_IS_HEX);

				// Change format of the edit box
				SetOffset(GetOffset(!IsHex));

				// Tell the hex view
				View->SetIsHex(IsHex);

				// Return focus to the view
				View->Focus(true);
			}
			break;
		}
		case IDC_BIT7:
		{
			if (View) View->SetBit(0x80, c->Value());
			break;
		}
		case IDC_BIT6:
		{
			if (View) View->SetBit(0x40, c->Value());
			break;
		}
		case IDC_BIT5:
		{
			if (View) View->SetBit(0x20, c->Value());
			break;
		}
		case IDC_BIT4:
		{
			if (View) View->SetBit(0x10, c->Value());
			break;
		}
		case IDC_BIT3:
		{
			if (View) View->SetBit(0x08, c->Value());
			break;
		}
		case IDC_BIT2:
		{
			if (View) View->SetBit(0x04, c->Value());
			break;
		}
		case IDC_BIT1:
		{
			if (View) View->SetBit(0x02, c->Value());
			break;
		}
		case IDC_BIT0:
		{
			if (View) View->SetBit(0x01, c->Value());
			break;
		}
		case IDC_DEC_1:
		{
			if (View && f == VK_RETURN) View->SetByte(atoi(c->Name()));
			break;
		}
		case IDC_HEX_1:
		{
			if (View && f == VK_RETURN) View->SetByte(htoi(c->Name()));
			break;
		}
		case IDC_DEC_2:
		{
			if (View && f == VK_RETURN) View->SetShort(atoi(c->Name()));
			break;
		}
		case IDC_HEX_2:
		{
			if (View && f == VK_RETURN) View->SetShort(htoi(c->Name()));
			break;
		}
		case IDC_DEC_4:
		{
			if (View && f == VK_RETURN) View->SetInt(atoi(c->Name()));
			break;
		}
		case IDC_HEX_4:
		{
			if (View && f == VK_RETURN) View->SetInt(htoi(c->Name()));
			break;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////
bool GHexBuffer::Save()
{
	if (!File ||
		!File->IsOpen())
	{
		LgiTrace("%s:%i - No open file.\n", _FL);
		return false;
	}

	if (File->SetPos(BufPos) != BufPos)
	{
		LgiTrace("%s:%i - Failed to set pos: " LGI_PrintfInt64 ".\n", _FL, BufPos);
		return false;
	}

	int Wr = File->Write(Buf, BufUsed);
	if (Wr != BufUsed)
	{
		LgiTrace("%s:%i - Failed to write %i bytes: %i.\n", _FL, BufUsed, Wr);
		return false;
	}

	IsDirty = false;
	return true;
}

void GHexBuffer::SetDirty(bool Dirty)
{
	if (IsDirty ^ Dirty)
	{
		IsDirty = Dirty;
		if (Dirty)
		{
			View->App->SetDirty(true);
		}
	}
}

bool GHexBuffer::GetData(int64 Start, int Len)
{
	static bool IsAsking = false;
	bool Status = false;

	// is the range outside the buffer's bounds?
	if (Start < 0 || Start + Len > Size)
	{
		return false;
	}

	if (!IsAsking) //  && File && File->IsOpen()
	{
		// is the buffer allocated
		if (!Buf)
		{
			BufLen = FILE_BUFFER_SIZE << 10;
			BufPos = 1;
			Buf = new uchar[BufLen];
			LgiAssert(Buf);
		}

		// is the cursor outside the buffer?
		if (Start < BufPos || (Start + Len) > (BufPos + BufLen))
		{
			// clear changes
			IsAsking = true;
			bool IsClean = View->App->SetDirty(false);
			IsAsking = false;

			if (IsClean)
			{
				// move buffer to cover cursor pos
				int Half = BufLen >> 1;
				BufPos = Start - (Half ? (Start % Half) : 0);
				if (File)
				{
					if (File->Seek(BufPos, SEEK_SET) == BufPos)
					{
						memset(Buf, 0xcc, BufLen);
						BufUsed = File->Read(Buf, BufLen);
						Status =	(Start >= BufPos) &&
									((Start + Len) < (BufPos + BufUsed));
					}
					else
					{
						BufUsed = 0;
					}
				}
				else
				{
					// In memory doc?
					Status = true;
				}
			}
		}
		else
		{
			Status = (Start >= BufPos) && (Start + Len <= BufPos + BufUsed);
		}
	}

	return Status;
}

bool GHexBuffer::GetLocationOfByte(GArray<GRect> &Loc, int64 Offset, const char16 *LineBuf)
{
	if (Offset < 0)
		return false;

	int64 X = Offset % View->BytesPerLine;
	int64 Y = Offset / View->BytesPerLine;
	
	int64 YPos = View->VScroll ? View->VScroll->Value() : 0;
	int64 YPx = (Y - YPos) * View->CharSize.y;
	int64 XHexPx = 0, XAsciiPx = 0;
	
	char16 s[128];
	int HexLen = X * 3;
	int AsciiLen = (View->BytesPerLine * 3) + GAP_HEX_ASCII + X;
	if (!LineBuf)
	{
		LineBuf = s;
		for (unsigned i=0; i<128; i++)
			s[i] = ' ';
	}
	
	{
		GDisplayString ds(View->Font, LineBuf, HexLen);
		XHexPx = ds.X();
	}
	{
		GDisplayString ds(View->Font, LineBuf, AsciiLen);
		XAsciiPx = ds.X();
	}
	
	GRect &rcHex = Loc.New();
	rcHex.ZOff((View->CharSize.x<<1) - 1, View->CharSize.y-1);
	rcHex.Offset(XHexPx + Pos.x1, YPx + Pos.y1);

	GRect &rcAscii = Loc.New();
	rcAscii.ZOff(View->CharSize.x - 1, View->CharSize.y-1);
	rcAscii.Offset(XAsciiPx + Pos.x1, YPx + Pos.y1);
	
	return true;
}

enum ColourFlags
{
	ForeCol = 0,
	BackCol = 1,
	SelectedCol = 2,
	ChangedCol = 4,
	CursorCol = 8,
};

void GHexBuffer::OnPaint(GSurface *pDC, int64 Start, int64 Len, GHexBuffer *Compare)
{
	// First position the layout
	int BufOff = Start - BufPos;
	int Bytes = MIN(Len, BufUsed - BufOff);
	int Lines = (Bytes + View->BytesPerLine - 1) / View->BytesPerLine;

	// Colour setup
	bool SelectedBuf = View->Cursor.Buf == this;
	GColour WkSp(LC_WORKSPACE, 24);
	float Mix = 0.85f;
	COLOUR Colours[16];
	// memset(&Colours, 0xaa, sizeof(Colours));
	Colours[ForeCol] = LC_TEXT;
	Colours[BackCol] = LC_WORKSPACE;
	Colours[ForeCol | SelectedCol] = SelectedBuf ? ColourSelectionFore : LC_TEXT;
	Colours[BackCol | SelectedCol] = SelectedBuf ? ColourSelectionBack : GColour(ColourSelectionBack, 24).Mix(WkSp, Mix).c24();
	Colours[ForeCol | ChangedCol] = LC_TEXT;
	Colours[BackCol | ChangedCol] = Rgb24(239, 203, 5);
	Colours[ForeCol | ChangedCol | SelectedCol] = GColour(Colours[ForeCol | SelectedCol], 24).Mix(GColour(Colours[ForeCol | ChangedCol], 24)).c24();
	Colours[BackCol | ChangedCol | SelectedCol] = GColour(Colours[BackCol | SelectedCol], 24).Mix(GColour(Colours[BackCol | ChangedCol], 24)).c24();
	Colours[ForeCol | CursorCol] = LC_TEXT;
	Colours[BackCol | CursorCol] = Rgb24(192, 192, 192);
	for (int i = 10; i < 16; i++)
	{
		GColour a(Colours[i - 8], 24), b;
		if (a.GetGray() > 0x80)
			b = GColour::Black.Mix(a, 0.75);
		else 
			b = GColour::White.Mix(a, 0.5);
		Colours[i | CursorCol] = b.c24();
	}

	#if 0
	static bool First = true;
	if (First)
	{
		First = false;
		for (int i=0; i<CountOf(Colours); i++)
		{
			LgiTrace("%i: %s %s %s %s = <div style='background:#%02.2x%02.2x%02.2x;width: 50px'>&nbsp;</div><br>\n",
				i,
				i & BackCol ? "Back" : "Fore",
				i & SelectedCol ? "Selected" : "Unselected",
				i & ChangedCol ? "Changed" : "Unchanged",
				i & CursorCol ? "Cursor" : "NonCursor",
				R24(Colours[i]),
				G24(Colours[i]),
				B24(Colours[i]));
		}
	}
	#endif

		
	// Now draw the layout data
	char s[256] = {0};
	uint8 ForeFlags[256];
	uint8 BackFlags[256];
	int EndY = Pos.y1 + (Lines * View->CharSize.y);
	EndY = MAX(EndY, Pos.y1);
	
	for (int Line=0; Line<Lines; Line++)
	{
		int CurY = Pos.y1 + (Line * View->CharSize.y);
		int Ch = 0;
		int64 LineStart = BufOff + (Line * View->BytesPerLine);
		
		// Setup comparison stuff
		uint8 *CompareBuf = NULL;
		int CompareLen = 0;
		if (Compare &&
			Compare->GetData(LineStart, View->BytesPerLine))
		{
			CompareBuf = Compare->Buf + (LineStart - Compare->BufPos);
			CompareLen = View->BytesPerLine;
		}
			
		// Clear the colours for this line
		memset(&ForeFlags, ForeCol, sizeof(ForeFlags));
		memset(&BackFlags, BackCol, sizeof(BackFlags));

		// Print the hex bytes to the line
		int64 n;
		int64 FromStart = BufOff + (Line * View->BytesPerLine);
		int64 From = FromStart, To = FromStart + View->BytesPerLine;
		for (n=From; n<To; n++)
		{
			if (n < BufUsed)
			{
				if (CompareBuf)
				{
					int64 Idx = n - FromStart;
					if (Buf[n] != CompareBuf[Idx])
					{
						BackFlags[Ch] |= ChangedCol;
						BackFlags[Ch+1] |= ChangedCol;
						if (n < To-1)
							BackFlags[Ch+2] |= ChangedCol;
					}
				}

				Ch += sprintf_s(s + Ch, sizeof(s) - Ch, "%02.2X ", Buf[n]);
			}
			else
			{
				Ch += sprintf_s(s + Ch, sizeof(s) - Ch, "   ");
			}
		}

		// Separator between hex/ascii
		Ch += sprintf_s(s + Ch, sizeof(s) - Ch, "  ");

		// Print the ascii characters to the line
		char *p = s + Ch;
		int StartOfAscii = Ch;
		for (n=From; n<To; n++)
		{
			if (n < BufUsed)
			{
				uchar c = Buf[n];

				if (CompareBuf)
				{
					int64 Idx = n - FromStart;
					if (Buf[n] != CompareBuf[Idx])
					{
						BackFlags[p - s] |= ChangedCol;
					}
				}

				*p++ = (c >= ' ' && c < 0x7f) ? c : '.';
			}
			else
			{
				*p++ = ' ';
			}
		}
		*p++ = 0;

		int CursorOff = -1;
		if (View->Cursor.Buf == this)
		{
			if (View->Cursor.Index >= BufPos &&
				View->Cursor.Index < BufPos + BufUsed)
			{
				CursorOff = View->Cursor.Index - BufPos;
				if ((CursorOff >= From) && (CursorOff < To))
					CursorOff -= From;
				else
					CursorOff = -1;
			}
		}

		// Draw text
		GFont *Font = View->Font;
		Font->Colour(LC_TEXT, LC_WORKSPACE);
		
		char16 *Wide = (char16*)LgiNewConvertCp(LGI_WideCharset, s, "iso-8859-1");
		if (Wide)
		{
			// Paint the selection into the colour buffers
			int64 DocPos = BufPos + LineStart;
			int64 Min = View->HasSelection() ? min(View->Selection.Index, View->Cursor.Index) : -1;
			int64 Max = View->HasSelection() ? max(View->Selection.Index, View->Cursor.Index) : -1;
			if (Min < DocPos + View->BytesPerLine &&
				Max >= DocPos)
			{
				// Part or all of this line is selected
				int64 s = ((View->Selection.Index - DocPos) * 3) + View->Selection.Nibble;
				int64 e = ((View->Cursor.Index - DocPos) * 3) + View->Cursor.Nibble;
				if (s > e)
				{
					int64 i = s;
					s = e;
					e = i;
				}
				if (s < 0)
					s = 0;
				if (e > View->BytesPerLine * 3 - 2)
					e = View->BytesPerLine * 3 - 2;

				for (int i=s; i<=e; i++)
				{
					ForeFlags[i] |= SelectedCol;
					BackFlags[i] |= SelectedCol;
				}
				for (int i=(s/3)+StartOfAscii; i<=(e/3)+StartOfAscii; i++)
				{
					ForeFlags[i] |= SelectedCol;
					BackFlags[i] |= SelectedCol;
				}
			}

			// Colour the back of the cursor gray...
			if (CursorOff >= 0 && /*View->Selection.Index < 0 && */View->Cursor.Flash)
			{
				BackFlags[(CursorOff * 3) + View->Cursor.Nibble] |= CursorCol;
				BackFlags[StartOfAscii + CursorOff] |= CursorCol;
			}

			// Go through the colour buffers, painting in runs of similar colour
			GRect r;
			int CxF = Pos.x1 << GDisplayString::FShift;
			int Len = p - s;
			for (int i=0; i<Len; )
			{
				// Find the end of the similarly coloured region...
				int e = i;
				while (e < Len)
				{
					if (ForeFlags[e] != ForeFlags[i] ||
						BackFlags[e] != BackFlags[i])
						break;
					e++;
				}

				// Paint a run of characters that have the same fore/back colour
				int Run = e - i;
				GDisplayString Str(Font, s + i, Run);
					
				r.x1 = CxF;
				r.y1 = CurY << GDisplayString::FShift;
				r.x2 = CxF + Str.FX();
				r.y2 = (CurY + Str.Y()) << GDisplayString::FShift;
					
				Font->Colour(Colours[ForeFlags[i]], Colours[BackFlags[i]]);
					
				Str.FDraw(pDC, CxF, CurY<<GDisplayString::FShift, &r);
					
				CxF += Str.FX();
				i = e;
			}

			int Cx = CxF >> GDisplayString::FShift;
			if (Cx < Pos.x2)
			{
				pDC->Colour(LC_WORKSPACE, 24);
				pDC->Rectangle(Cx, CurY, Pos.x2, CurY+View->CharSize.y);
			}
				
			DeleteArray(Wide);
		}

		if (CursorOff >= 0)
		{
			// Draw cursor
			GetLocationOfByte(View->Cursor.Pos, View->Cursor.Index, Wide);

			pDC->Colour(View->Focus() ? LC_TEXT : LC_LOW, 24);
			for (unsigned i=0; i<View->Cursor.Pos.Length(); i++)
			{
				GRect r = View->Cursor.Pos[i];

				r.y1 = r.y2;
				if (i == 0)
				{
					// Hex side..
					if (View->Cursor.Nibble)
						r.x1 += View->CharSize.x;
					else
						r.x2 -= View->CharSize.x;

					if (View->Cursor.Pane == HexPane)
						r.y1--;
				}
				else if (View->Cursor.Pane == AsciiPane)
				{
					r.y1--;
				}
						
				pDC->Rectangle(&r);
			}
		}
	}
	
	if (EndY < Pos.y2)
	{
		GRect r(Pos.x1, EndY, Pos.x2, Pos.y2);
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle(&r);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
GHexView::GHexView(AppWnd *app, IHexBar *bar)
{
	// Init
	App = app;
	Bar = bar;
	Font = 0;
	CharSize.x = 8;
	CharSize.y = 16;
	IsHex = true;
	
	BytesPerLine = 16;
	IntWidth = 1;
	
	SetId(IDC_HEX_VIEW);

	// Font
	GFontType Type;
	if (Type.GetSystemFont("Fixed"))
	{
		Font = Type.Create();
		if (Font)
		{
			GDisplayString ds(Font, "A");
			CharSize.x = ds.X();
			CharSize.y = ds.Y();
		}
	}
	else LgiAssert(0);

	Attach(App);
	Name("GHexView");
	SetScrollBars(false, true);
	if (VScroll)
	{
		VScroll->SetNotify(this);
	}
}

GHexView::~GHexView()
{
	DeleteObj(Font);
	Empty();
}

bool GHexView::Empty()
{
	Buf.DeleteObjects();
	Cursor.Empty();
	Selection.Empty();
	return true;
}

int GHexView::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDC_VSCROLL:
		{
			Invalidate();
			break;
		}
	}

	return 0;
}

int64 GHexView::GetFileSize()
{
	if (Buf.Length() && Buf[0])
		return Buf[0]->Size;

	return -1;
}

bool GHexView::SetFileSize(int64 size)
{
	// Save any changes
	if (App->SetDirty(false) &&
		Buf.Length() &&
		Cursor.Buf)
	{
		Cursor.Buf->SetSize(size);

		int p = Cursor.Buf->BufPos;
		Cursor.Buf->BufPos++;
		Cursor.Buf->GetData(p, 1);

		UpdateScrollBar();
		Invalidate();
	}

	return false;
}

void GHexView::SetIsHex(bool i)
{
	if (i != IsHex)
	{
		IsHex = i;
		Invalidate();
	}
}

void GHexView::Copy(FormatType Fmt)
{
	if (!HasSelection() ||
		Buf.Length() == 0 ||
		!Buf[0])
		return;

	GClipBoard c(this);
	GHexBuffer *b = Buf[0];

	int64 Min = min(Selection.Index, Cursor.Index);
	int64 Max = max(Selection.Index, Cursor.Index);
	int64 Len = Max - Min + 1;

	if (b->GetData(Min, Len))
	{
		uint64 Offset = Min - b->BufPos;
		uchar *Ptr = b->Buf + Offset;
		
		if (Len > b->BufUsed - Offset)
		{
			Len = b->BufUsed - Offset;
		}

		GStringPipe p;
		if (Fmt == FmtCode)
			p.Print("unsigned char Var[] = {\n\t");
		
		for (int i=0; i<Len; i++)
		{
			if (Fmt == FmtHex)
				p.Print("%s%2.2X", i ? " " : "", Ptr[i]);
			else if (Fmt == FmtCode)
				p.Print("0x%2.2X%s", Ptr[i], i == Len-1 ? "" : ",");
			else
				p.Print("%c", Ptr[i]);

			if (Fmt == FmtCode && i % 32 == 31)
				p.Print("\n\t");
		}
		if (Fmt == FmtCode)
			p.Print("\n};\n");
		GAutoString str(p.NewStr());
	
		#ifdef WIN32
		c.Binary(CF_PRIVATEFIRST, Ptr, Len, true);
		#endif
		c.Text(str, false);
	}
}

void GHexView::Paste()
{
	GClipBoard c(this);

	GAutoPtr<uint8> Ptr;
	ssize_t Len = 0;
	#ifdef WIN32
	if (c.Binary(CF_PRIVATEFIRST, Ptr, &Len))
	{
	}
	else
	#endif
	{
		GString Txt = c.Text();
		if (Txt)
		{
			// Convert from binary...
			GArray<uint8> Out;
			int High = -1;

			bool HasComma = false;
			for (char *i = Txt; *i; i++)
			{
				if (*i == ',')
				{
					HasComma = true;
					break;
				}
			}

			if (HasComma)
			{
				// Comma separated integers?
				for (char *i = Txt; *i; )
				{
					while (*i && !IsDigit(*i))
						i++;

					char *e = i;
					while (*e && IsDigit(*e))
						e++;

					Out.Add(Atoi(i));
					i = e;
				}
			}
			else
			{
				// Hex data?
				for (char *i = Txt; *i; i++)
				{
					int n = -1;
					if (*i >= '0' && *i <= '9')
						n = *i - '0';
					else if (*i >= 'a' && *i <= 'f')
						n = *i - 'a' + 10;
					else if (*i >= 'A' && *i <= 'F')
						n = *i - 'A' + 10;
					if (n >= 0)
					{
						if (High >= 0)
						{
							Out.Add(High << 4 | n);
							High = -1;
						}
						else
						{
							High = n;
						}
					}
				}
			}

			if (Out.Length())
			{
				Len = Out.Length();
				Ptr.Reset(Out.Release());
			}
		}
	}

	if (Ptr && Len > 0)
	{
		Cursor.Index = MAX(0, Cursor.Index);
		if (Buf.Length() == 0 || !Buf[0]->GetData(Cursor.Index, Len))
		{
			if (!CreateFile(Len))
				return;
			if (!Buf[0]->GetData(0, Len))
				return;
		}

		GHexBuffer *b = Buf[0];
		if (b)
		{
			memcpy(b->Buf + Cursor.Index - b->BufPos, Ptr, Len);
			App->SetDirty(true);
			Invalidate();
			DoInfo();
		}
	}
}

void GHexView::UpdateScrollBar()
{
	int Lines = GetClient().Y() / CharSize.y;
	int64 DocLines = 0;
	for (unsigned i=0; i<Buf.Length(); i++)
	{
		GHexBuffer *b = Buf[i];
		int BufLines = (b->Size + 15) / 16;
		DocLines = MAX(DocLines, BufLines);
	}

	SetScrollBars(false, DocLines > Lines);
	if (VScroll)
	{
		VScroll->SetLimits(0, DocLines > 0 ? DocLines : 0);
		VScroll->SetPage(Lines);
	}
}

void GHexView::SwapBytes(void *p, int Len)
{
	if (Bar && !Bar->IsLittleEndian())
	{
		uchar *c = (uchar*)p;
		for (int i=0; i<Len>>1; i++)
		{
			uchar t = c[i];
			c[i] = c[Len-1-i];
			c[Len-1-i] = t;
		}
	}
}

bool GHexView::GetDataAtCursor(char *&Data, int &Len)
{
	GHexBuffer *b = Buf.Length() ? Buf.First() : NULL;
	if (b && b->Buf)
	{
		int Offset = Cursor.Index - b->BufPos;
		Data = (char*)b->Buf + Offset;
		Len = min(b->BufUsed, b->BufLen) - Offset;
		return true;
	}

	return false;
}

bool GHexView::HasSelection()
{
	return Selection.Index >= 0;
}

int GHexView::GetSelectedNibbles()
{
	if (!HasSelection())
		return 0;

	int c = (Cursor.Index << 1) + Cursor.Nibble;
	int s = (Selection.Index << 1) + Selection.Nibble;
	
	int Min, Max;
	if (s < c)
	{
		Min = s;
		Max = c;
	}
	else
	{
		Max = s;
		Min = c;
	}
	
	return Max - Min + 1;
}

void GHexView::InvalidateLines(GArray<GRect> &NewLoc, GArray<GRect> &OldLoc)
{
	if (NewLoc.Length() > 0 && OldLoc.Length() > 0)
	{
		// Work out the union of NewLoc and OldLoc
		int MinY = min(NewLoc[0].y1, OldLoc[0].y1);
		int MaxY = max(NewLoc[0].y2, OldLoc[0].y2);
		GRect u(0, MinY, X()-1, MaxY);
		Invalidate(&u);
	}
	else LgiAssert(0);
}

GHexBuffer *GHexView::GetCursorBuffer()
{
	return Cursor.Buf;
}

void GHexView::SetCursor(GHexBuffer *b, int64 cursor, int nibble, bool Selecting)
{
	GArray<GRect> OldLoc, NewLoc;
	bool SelectionChanging = false;
	bool SelectionEnding = false;
	
	if (!b)
		b = Cursor.Buf;
	if (!b)
		return;

	if (Selecting)
	{
		if (!HasSelection())
			// Start selection
			Selection = Cursor;

		SelectionChanging = true;
	}
	else
	{
		if (HasSelection())
		{
			// Deselecting
			
			// Repaint the entire selection area...
			b->GetLocationOfByte(NewLoc, Cursor.Index, NULL);
			b->GetLocationOfByte(OldLoc, Selection.Index, NULL);
			InvalidateLines(NewLoc, OldLoc);
			
			SelectionEnding = true;
			Selection.Index = -1;
		}
	}

	if (!SelectionEnding)
		b->GetLocationOfByte(OldLoc, Cursor.Index, NULL);
	// else the selection just ended and the old cursor location just got repainted anyway

	// Limit to doc
	if (cursor >= b->Size)
	{
		cursor = b->Size - 1;
		nibble = 1;
	}
	if (cursor < 0)
	{
		cursor = 0;
		nibble = 0;
	}

	// Is different?
	if (Cursor.Buf != b ||
		Cursor.Index != cursor ||
		Cursor.Nibble != nibble)
	{
		// Set the cursor
		Cursor.Buf = b;
		Cursor.Index = cursor;
		Cursor.Nibble = nibble;

		// Make sure the cursor is in the viewable area?
		if (VScroll)
		{
			int64 Start = (uint64) (VScroll ? VScroll->Value() : 0) * 16;
			int Lines = GetClient().Y() / CharSize.y;
			int64 End = min(b->Size, Start + (Lines * 16));
			if (Cursor.Index < Start)
			{
				// Scroll up
				VScroll->Value((Cursor.Index - (Cursor.Index%16)) / 16);
				Invalidate();
			}
			else if (Cursor.Index >= End)
			{
				// Scroll down
				int64 NewVal = (Cursor.Index - (Cursor.Index%16) - ((Lines-1) * 16)) / 16;
				VScroll->Value(NewVal);
				Invalidate();
			}
		}

		if (Bar)
		{
			Bar->SetOffset(Cursor.Index);
			DoInfo();
		}
		
		Cursor.Flash = true;

		b->GetLocationOfByte(NewLoc, Cursor.Index, NULL);
		if (!SelectionChanging)
		{
			// Just update the cursor's old and new locations
			NewLoc.Add(OldLoc);
			// DbgRect = NewLoc;
			for (unsigned i=0; i<NewLoc.Length(); i++)
			{
				Invalidate(&NewLoc[i]);
			}
		}
	}
	
	if (SelectionChanging)
	{
		if (!NewLoc.Length())
			b->GetLocationOfByte(NewLoc, Cursor.Index, NULL);
		InvalidateLines(NewLoc, OldLoc);
	}

	SendNotify(GNotifyCursorChanged);
}

int64 GHexView::Search(SearchDlg *For, uchar *Bytes, int Len)
{
	int64 Hit = -1;

	if (For->Bin && For->Length > 0)
	{
		for (int i=0; i<Len - For->Length; i++)
		{
			bool Match = true;
			for (int n=0; n<For->Length; n++)
			{
				if (For->MatchCase || For->ForHex)
				{
					if (For->Bin[n] != Bytes[i+n])
					{
						Match = false;
						break;
					}
				}
				else
				{
					if (tolower(For->Bin[n]) != tolower(Bytes[i+n]))
					{
						Match = false;
						break;
					}
				}
			}
			if (Match)
			{
				Hit = i;
				break;
			}
		}
	}

	return Hit;
}

void GHexView::DoSearch(SearchDlg *For)
{
	int Block = 32 << 10;
	int64 Hit = -1, c;
	int64 Time = LgiCurrentTime();
	GProgressDlg *Prog = 0;
	GHexBuffer *b = Cursor.Buf;
	if (!b)
		return;

	// Search through to the end of the file...
	for (c = Cursor.Index + 1; c < b->Size; c += Block)
	{
		int Actual = min(Block, GetFileSize() - c);
		if (b->GetData(c, Actual))
		{
			Hit = Search(For, b->Buf + (c - b->BufPos), Actual);
			if (Hit >= 0)
			{
				Hit += c;
				break;
			}

			int64 Now = LgiCurrentTime();
			if (Now - Time > UI_UPDATE_SPEED)
			{
				Time = Now;
				if (!Prog)
				{
					if ((Prog = new GProgressDlg(this)))
					{
						Prog->SetDescription("Searching...");
						Prog->SetLimits(0, GetFileSize());
						Prog->SetScale(1.0 / 1024.0);
						Prog->SetType("kb");
					}
				}
				else
				{
					Prog->Value(c - Cursor.Index);
					LgiYield();
				}
			}
		}
		else break;
	}

	if (Hit < 0)
	{
		// Now search from the start of the file to the original cursor
		for (c = 0; c < Cursor.Index; c += Block)
		{
			if (b->GetData(c, Block))
			{
				int Actual = min(Block, Cursor.Index - c);
				Hit = Search(For, b->Buf + (c - b->BufPos), Actual);
				if (Hit >= 0)
				{
					Hit += c;
					break;
				}

				int64 Now = LgiCurrentTime();
				if (Now - Time > UI_UPDATE_SPEED)
				{
					Time = Now;
					if (!Prog)
					{
						if ((Prog = new GProgressDlg(this)))
						{
							Prog->SetDescription("Searching...");
							Prog->SetLimits(0, GetFileSize());
							Prog->SetScale(1.0 / 1024.0);
							Prog->SetType("kb");
						}
					}
					else
					{
						Prog->Value(b->Size - Cursor.Index + c);
						LgiYield();
					}
				}
			}
			else break;
		}
	}

	if (Hit >= 0)
	{
		SetCursor(b, Hit);
		SetCursor(b, Hit + For->Length - 1, 1, true);
	}

	DeleteObj(Prog);
}

void GHexView::SetBit(uint8 Bit, bool On)
{
	GHexBuffer *b = Cursor.Buf;
	if (!b)
		return;

	if (b->GetData(Cursor.Index, 1))
	{
		if (On)
		{
			b->Buf[Cursor.Index - b->BufPos] |= Bit;
		}
		else
		{
			b->Buf[Cursor.Index - b->BufPos] &= ~Bit;
		}

		App->SetDirty(true);
		Invalidate();
		DoInfo();
	}	
}

void GHexView::SetByte(uint8 Byte)
{
	GHexBuffer *b = Cursor.Buf;
	if (!b)
		return;

	if (b->GetData(Cursor.Index, 1))
	{
		if (b->Buf[Cursor.Index - b->BufPos] != Byte)
		{
			b->Buf[Cursor.Index - b->BufPos] = Byte;
			App->SetDirty(true);
			Invalidate();
			DoInfo();
		}
	}	
}

void GHexView::SetShort(uint16 Short)
{
	GHexBuffer *b = Cursor.Buf;
	if (!b)
		return;

	if (b->GetData(Cursor.Index, 2))
	{
		SwapBytes(&Short, sizeof(Short));

		uint16 *p = (uint16*) (&b->Buf[Cursor.Index - b->BufPos]);
		if (*p != Short)
		{
			*p = Short;
			App->SetDirty(true);
			Invalidate();
			DoInfo();
		}
	}	
}

void GHexView::SetInt(uint32 Int)
{
	GHexBuffer *b = Cursor.Buf;
	if (!b)
		return;

	if (b->GetData(Cursor.Index, 4))
	{
		SwapBytes(&Int, sizeof(Int));

		uint32 *p = (uint32*) (&b->Buf[Cursor.Index - b->BufPos]);
		if (*p != Int)
		{
			*p = Int;
			App->SetDirty(true);
			Invalidate();
			DoInfo();
		}
	}	
}

void GHexView::DoInfo()
{
	GHexBuffer *b = Cursor.Buf;
	if (Bar && b)
	{
		bool IsSigned = Bar->IsSigned();
		GView *w = GetWindow();

		char s[256] = "";
		if (b->GetData(Cursor.Index, 1))
		{
			int c = b->Buf[Cursor.Index - b->BufPos], sc;
			if (IsSigned)
				sc = (char)b->Buf[Cursor.Index - b->BufPos];
			else
				sc = b->Buf[Cursor.Index - b->BufPos];

			Bar->NotifyOff = true;

			sprintf(s, "%i", sc);
			w->SetCtrlName(IDC_DEC_1, s);
			sprintf(s, "%02.2X", c);
			w->SetCtrlName(IDC_HEX_1, s);
			sprintf(s, "%c", c >= ' ' && c <= 0x7f ? c : '.');
			w->SetCtrlName(IDC_ASC_1, s);

			uint8 Bits = b->Buf[Cursor.Index - b->BufPos];
			Bar->SetCtrlValue(IDC_BIT7, (Bits & 0x80) != 0);
			Bar->SetCtrlValue(IDC_BIT6, (Bits & 0x40) != 0);
			Bar->SetCtrlValue(IDC_BIT5, (Bits & 0x20) != 0);
			Bar->SetCtrlValue(IDC_BIT4, (Bits & 0x10) != 0);
			Bar->SetCtrlValue(IDC_BIT3, (Bits & 0x08) != 0);
			Bar->SetCtrlValue(IDC_BIT2, (Bits & 0x04) != 0);
			Bar->SetCtrlValue(IDC_BIT1, (Bits & 0x02) != 0);
			Bar->SetCtrlValue(IDC_BIT0, (Bits & 0x01) != 0);

			Bar->NotifyOff = false;
		}

		GViewI *Hex, *Dec;
		if (w->GetViewById(IDC_HEX_2, Hex) &&
			w->GetViewById(IDC_DEC_2, Dec))
		{
			bool Valid = b->GetData(Cursor.Index, 2);
			GString sHex, sDec;
			if (Valid)
			{
				uint16 *sp = (uint16*)(b->Buf+(Cursor.Index - b->BufPos));
				uint16 c = *sp;
				SwapBytes(&c, sizeof(c));
				int c2 = (int16)c;
				sDec.Printf("%i", IsSigned ? c2 : c);
				sHex.Printf("%04.4X", c);
			}

			Dec->Name(Valid ? sDec : NULL);
			Hex->Name(Valid ? sHex : NULL);

			Dec->Enabled(Valid);
			Hex->Enabled(Valid);
		}

		if (w->GetViewById(IDC_HEX_4, Hex) &&
			w->GetViewById(IDC_DEC_4, Dec))
		{
			bool Valid = b->GetData(Cursor.Index, 4);
			GString sHex, sDec;
			if (Valid)
			{
				uint32 *lp = (uint32*)(b->Buf + (Cursor.Index - b->BufPos));
				uint32 c = *lp;
				SwapBytes(&c, sizeof(c));

				sDec.Printf(IsSigned ? "%i" : "%u", c);
				sHex.Printf("%08.8X", c);
			}

			Dec->Name(Valid ? sDec : NULL);
			Hex->Name(Valid ? sHex : NULL);

			Dec->Enabled(Valid);
			Hex->Enabled(Valid);
		}
	}
}

bool FileToArray(GArray<uint8> &a, const char *File)
{
	GFile f;
	if (!f.Open(File, O_READ))
		return false;
	
	if (!a.Length(f.GetSize()))
		return false;
	
	int rd = f.Read(&a[0], a.Length());
	if (rd != a.Length())
		return false;
	
	return true;	
}

void GHexView::CompareFile(char *CmpFile)
{
	if (FileExists(CmpFile))
	{
		GAutoPtr<GHexBuffer> b(new GHexBuffer(this));
		if (b)
		{
			if (b->Open(CmpFile, false))
			{
				Buf.Add(b.Release());
				UpdateScrollBar();
				Invalidate();
			}
		}
	}
}

bool GHexView::CreateFile(int64 Len)
{
	if (!App->SetDirty(false))
		return false;

	GHexBuffer *b = new GHexBuffer(this);
	if (!b)
		return false;

	Buf.Add(b);
	b->Buf = new uchar[b->BufLen = Len];
	if (!b->Buf)
	{
		Buf.DeleteObjects();
		return false;
	}

	memset(b->Buf, 0, Len);
	b->BufUsed = Len;
	b->Size = Len;

	Focus(true);
	SetCursor(b, 0);
	UpdateScrollBar();

	if (Bar)
		Bar->SetCtrlValue(IDC_OFFSET, 0);

	Invalidate();
	DoInfo();
	App->Name(AppName);
	App->OnDocument(true);

	return true;
}

bool GHexView::OpenFile(char *FileName, bool ReadOnly)
{
	bool Status = false;

	if (App->SetDirty(false))
	{
		Empty();

		GAutoPtr<GHexBuffer> b(new GHexBuffer(this));
		if (b && FileName)
		{
			if (b->Open(FileName, ReadOnly))
			{
				Focus(true);
				SetCursor(b, 0);
				Buf[0] = b.Release();
				Status = true;
			}
			else
			{
				LgiMsg(this, "Couldn't open '%s' for reading.", AppName, MB_OK, FileName);
			}
		}

		if (Bar)
		{
			Bar->SetCtrlValue(IDC_OFFSET, 0);
		}

		Invalidate();
		DoInfo();

		char Title[MAX_PATH + 100];
		sprintf_s(Title, sizeof(Title), "%s [%s]", AppName, FileName);
		App->Name(Title);

		UpdateScrollBar();
	}

	return Status;
}

bool GHexView::CloseFile(int Index)
{
	if (Index < 0)
		Index = Buf.Length() - 1;
	if (!Buf.AddressOf((unsigned) Index))
		return false;

	delete Buf[Index];
	Buf.DeleteAt(Index, true);
	
	Cursor.Empty();
	Selection.Empty();

	Invalidate();	
	return true;
}

bool GHexView::Save()
{
	bool Status = true;

	// Save all buffers
	for (unsigned i=0; i<Buf.Length(); i++)
	{
		GHexBuffer *b = Buf[i];
		if (b->IsDirty ||
			!b->File)
		{
			if (!b->File)
			{
				GFileSelect s;
				s.Parent(this);
				if (s.Save())
				{
					b->File = new GFile;
					if (b->File && !b->File->Open(s.Name(), O_READWRITE))
						DeleteObj(b->File);

					App->SetCurFile(s.Name());
				}
				else Status = false;
			}

			b->Save();
			b->SetDirty(false);
		}
	}

	return Status;
}

bool GHexView::SaveFile(GHexBuffer *b, char *FileName)
{
	bool Status = false;

	if (!b)
		b = Cursor.Buf;

	if (b &&
		b->File &&
		FileName)
	{
		if (stricmp(FileName, b->File->GetName()) == 0)
		{
			if (b->File->Seek(b->BufPos, SEEK_SET) == b->BufPos)
			{
				int Len = min(b->BufLen, b->Size - b->BufPos);
				Status = b->File->Write(b->Buf, Len) == Len;
			}
		}
	}

	return Status;
}

bool GHexView::HasFile()
{
	if (Buf.Length() && Buf[0])
		return Buf.First()->File != NULL;

	return false;
}

void GHexView::SaveSelection(GHexBuffer *b, char *FileName)
{
	if (!b)
		b = Cursor.Buf;
	
	if (b &&
		b->HasData() &&
		FileName)
	{
		GFile f;
		if (HasSelection() &&
			f.Open(FileName, O_WRITE))
		{
			int64 Min = min(Selection.Index, Cursor.Index);
			int64 Max = max(Selection.Index, Cursor.Index);
			int64 Len = Max - Min + 1;

			f.SetSize(Len);
			f.SetPos(0);

			int64 Block = 4 << 10;
			for (int64 i=0; i<Len; i+=Block)
			{
				int64 AbsPos = Min + i;
				int64 Bytes = min(Block, Len - i);
				if (b->GetData(AbsPos, Bytes))
				{
					uchar *p = b->Buf + (AbsPos - b->BufPos);
					f.Write(p, Bytes);
				}
			}									
		}
	}
}

void GHexView::SelectAll()
{
	GHexBuffer *b = Cursor.Buf;
	if (b)
	{
		SetCursor(b, 0, 0, false);
		SetCursor(b, b->Size-1, 1, true);
	}
}

void GHexView::SelectionFillRandom(GStream *Rnd)
{
	if (!Rnd || !Cursor.Buf)
		return;

	GHexBuffer *b = Cursor.Buf;
	int64 Min = min(Selection.Index, Cursor.Index);
	int64 Max = max(Selection.Index, Cursor.Index);
	int64 Len = Max - Min + 1;

	if (b->File)
	{
		int64 Last = LgiCurrentTime();
		int64 Start = Last;
		GProgressDlg Dlg(NULL);
		GArray<char> Buf;
		Buf.Length(2 << 20);
		Dlg.SetLimits(0, Len);
		Dlg.SetScale(1.0 / 1024.0 / 1024.0);
		Dlg.SetType("MB");

		b->File->SetPos(Min);

		#if 1
		if (Rnd->Read(&Buf[0], Buf.Length()) != Buf.Length())
		{
			LgiMsg(this, "Random stream failed.", AppName);
			return;
		}
		#endif

		for (int64 i=0; !Dlg.Cancel() && i<Len; i+=Buf.Length())
		{
			int64 Remain = min(Buf.Length(), Len-i);

			#if 0
			if (Rnd->Read(&Buf[0], Remain) != Remain)
			{
				LgiMsg(this, "Random stream failed.", AppName);
				return;
			}
			#endif

			int w = b->File->Write(&Buf[0], Remain);
			if (w != Remain)
			{
				LgiMsg(this, "Write file failed.", AppName);
				break;
			}

			int64 Now = LgiCurrentTime();
			if (Now - Last > 500)
			{
				Dlg.Value(i);
				LgiYield();
				Last = Now;

				double Sec = (double)(int64)(Now - Start) / 1000.0;
				double Rate = (double)(int64)(i + Remain) / Sec;
				int TotalSeconds = (int) ((Len - i - Remain) / Rate);
				char s[64];
				sprintf(s, "%i:%02.2i:%02.2i remaining", TotalSeconds/3600, (TotalSeconds%3600)/60, TotalSeconds%60);
				Dlg.SetDescription(s);
			}
		}

		if (b->File->SetPos(b->BufPos) == b->BufPos)
		{
			b->BufUsed = b->File->Read(b->Buf, b->BufLen);
		}
	
		Invalidate();
	}
}

bool GHexView::Pour(GRegion &r)
{
	GRect *Best = FindLargest(r);
	if (Best)
	{
		SetPos(*Best, true);
		return true;
	}
	return false;
}

void GHexView::OnPosChange()
{
	UpdateScrollBar();
	GLayout::OnPosChange();
}

void GHexView::InvalidateCursor()
{
	for (int i=0; i<Cursor.Pos.Length(); i++)
	{
		Invalidate(&Cursor.Pos[i]);
	}
}

void GHexView::OnPulse()
{
	Cursor.Flash = !Cursor.Flash;
	InvalidateCursor();
}

void GHexView::OnPaint(GSurface *pDC)
{
	GRect Cli = GetClient();
	GRect r = Cli;

	#if DEBUG_COVERAGE_CHECK
	pDC->Colour(GColour(255, 0, 255));
	pDC->Rectangle();
	#endif

	GRegion TopMargin;
	if (Buf.Length() > 1)
	{
		r.y1 += (int) (SysBold->GetHeight() * 1.5);
		TopMargin = GRect(0, 0, r.x2, r.y1-1);
	}

	int64 YPos = VScroll ? VScroll->Value() : 0;
	int64 Start = YPos * BytesPerLine;
	
	int Columns = (3 * BytesPerLine) + GAP_HEX_ASCII + (BytesPerLine);
	int Lines = (r.Y() + CharSize.y -1) / CharSize.y;
	
	Cursor.Pos.Length(0);

	int MaxSize = 0;
	for (unsigned int BufIdx = 0; BufIdx < Buf.Length(); BufIdx++)
		MaxSize = MAX(MaxSize, Buf[BufIdx]->Size);
	int AddrLines = (MaxSize + BytesPerLine - 1) / BytesPerLine;
	int Addrs = AddrLines - YPos;

	// Draw all the addresses
	Font->Transparent(false);
	Font->Colour(LC_TEXT, LC_WORKSPACE);
	int CurrentY = 0;
	int CurrentX = 0;
	for (int Line=0; Line<Addrs; Line++)
	{
		CurrentY = r.y1 + (Line * CharSize.y);
		if (CurrentY > r.y2)
			break;
		int64 LineAddr = Start + (Line * BytesPerLine);
			
		GString p;
		if (IsHex)
			p.Printf("%02.2x:%08.8X  ", (uint)(LineAddr >> 32), (uint)LineAddr);
		else
			#ifdef WIN32
			p.Printf("%11.11I64i  ", LineAddr);
			#else
			p.Printf("%11.11lli  ", LineAddr);
			#endif
		GDisplayString ds(Font, p);
		ds.Draw(pDC, r.x1, CurrentY);
		CurrentX = ds.X();
	}
	if (Addrs)
		CurrentY += CharSize.y;
	if (CurrentY < r.y2)
	{
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle(r.x1, CurrentY, CurrentX-1, r.y2);
	}

	// Draw all the data buffers...
	for (unsigned int BufIdx = 0; BufIdx < Buf.Length(); BufIdx++)
	{
		GHexBuffer *b = Buf[BufIdx];
		b->Pos.ZOff(Columns * CharSize.x, Lines * CharSize.y);
		b->Pos.Offset(r.x1 + (HEX_COLUMN * CharSize.x), r.y1);
		if (BufIdx)
			b->Pos.Offset
			(
				BufIdx
				*
				(Columns + GAP_FILES)
				*
				CharSize.x,
				0
			);

		if (CurrentX < b->Pos.x1)
		{
			// Paint any whitespace before this column
			pDC->Colour(LC_WORKSPACE, 24);
			pDC->Rectangle(CurrentX + 1, r.y1, b->Pos.x1 - 1, r.y2);
		}
		
		SysBold->Transparent(false);
		SysBold->Colour(LC_TEXT, LC_WORKSPACE);
		GDisplayString Ds(SysBold, b->File ? b->File->GetName() : LgiLoadString(IDS_UNTITLED_BUFFER));
		GRect r(b->Pos.x1, 0, b->Pos.x1+Ds.X()-1, Ds.Y()-1);
		Ds.Draw(pDC, r.x1, r.y1);
		TopMargin.Subtract(&r);
	
		int64 End = min(b->Size, Start + (Lines * BytesPerLine));
		if (b->GetData(Start, End-Start))
		{
			GHexBuffer *Comp = Buf.Length() > 1 ? Buf[!BufIdx] : NULL;
			b->OnPaint(pDC, Start, End - Start, Comp);
		}

		CurrentX = b->Pos.x2;
	}
	
	if (CurrentX < r.x2)
	{
		// Paint any whitespace after the last column
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle(CurrentX + 1, r.y1, r.x2, r.y2);
	}
	if (TopMargin.Length() > 0)
	{
		for (unsigned i=0; i<TopMargin.Length(); i++)
		{
			GRect *r = TopMargin[i];
			pDC->Colour(LC_WORKSPACE, 24);
			pDC->Rectangle(r);
		}
	}
}

bool GHexView::OnMouseWheel(double Lines)
{
	if (VScroll)
	{
		VScroll->Value(VScroll->Value() + (int)Lines);
		Invalidate();
	}
	return true;
}

bool GHexView::GetCursorFromLoc(int x, int y, GHexCursor &c)
{
	uint64 Start = ((uint64)(VScroll ? VScroll->Value() : 0)) * BytesPerLine;
	int HexCols = BytesPerLine * 3;
	int AsciiCols = HexCols + GAP_HEX_ASCII;

	for (unsigned i=0; i<Buf.Length(); i++)
	{
		GHexBuffer *b = Buf[i];
		if (b->Pos.Overlap(x, y))
		{
			int col = (x - b->Pos.x1) / CharSize.x;
			int row = (y - b->Pos.y1) / CharSize.y;

			if (col >= 0 && col < HexCols)
			{
				int Byte = col / 3;
				int Bit = col % 3;

				c.Buf = b;
				c.Index = Start + (row * BytesPerLine) + Byte;
				c.Nibble = Bit > 0;
				c.Pane = HexPane;
				return true;
			}
			else if (col >= AsciiCols)
			{
				int Asc = col - AsciiCols;
				if (Asc < BytesPerLine)
				{
					c.Buf = b;
					c.Index = Start + (row * BytesPerLine) + Asc;
					c.Nibble = 0;
					c.Pane = AsciiPane;
					return true;
				}
			}
		}
	}

	return false;
}

void GHexView::OnMouseClick(GMouse &m)
{
	Capture(m.Down());
	if (m.Down())
	{
		Focus(true);

		if (m.Left())
		{
			GHexCursor c;
			if (GetCursorFromLoc(m.x, m.y, c))
			{
				int Idx = Buf.IndexOf(c.Buf);
				SetCursor(c.Buf, c.Index, c.Nibble, m.Shift());
				Cursor.Pane = c.Pane;
			}
		}
	}
}

void GHexView::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		GHexCursor c;
		if (GetCursorFromLoc(m.x, m.y, c))
		{
			if (c.Pane == AsciiPane &&
				c.Index >= Cursor.Index)
			{
				c.Nibble = 1;
			}

			SetCursor(c.Buf, c.Index, c.Nibble, true);
		}
	}
}

void GHexView::OnFocus(bool f)
{
	Invalidate();
}

void GHexView::InvalidateByte(int64 Idx)
{
	for (unsigned i=0; i<Buf.Length(); i++)
	{
		GHexBuffer *b = Buf[i];

		GArray<GRect> Loc;
		if (b->GetLocationOfByte(Loc, Idx, NULL))
		{
			Loc[0].x2 += CharSize.x;
			for (unsigned i=0; i<Loc.Length(); i++)
				Invalidate(&Loc[i]);
		}
	}
}

bool GHexView::OnKey(GKey &k)
{
	int Lines = GetClient().Y() / CharSize.y;
	GHexBuffer *b = Cursor.Buf;

	k.Trace("HexView");
	
	switch (k.vkey)
	{
		default:
		{
			if (b && k.IsChar && !b->IsReadOnly)
			{
				if (k.Down())
				{
					if (Cursor.Pane == HexPane)
					{
						int c = -1;
						if (k.c16 >= '0' && k.c16 <= '9')		c = k.c16 - '0';
						else if (k.c16 >= 'a' && k.c16 <= 'f')	c = k.c16 - 'a' + 10;
						else if (k.c16 >= 'A' && k.c16 <= 'F')	c = k.c16 - 'A' + 10;

						if (c >= 0 && c < 16)
						{
							uchar *Byte = b->Buf + (Cursor.Index - b->BufPos);
							if (Cursor.Nibble)
							{
								*Byte = (*Byte & 0xf0) | c;
							}
							else
							{
								*Byte = (c << 4) | (*Byte & 0xf);
							}

							b->SetDirty();
							InvalidateByte(Cursor.Index);
							if (Cursor.Nibble == 0)
								SetCursor(b, Cursor.Index, 1);
							else if (Cursor.Index < b->Size - 1)
								SetCursor(b, Cursor.Index+1, 0);
						}
					}
					else if (Cursor.Pane == AsciiPane)
					{
						uchar *Byte = b->Buf + (Cursor.Index - b->BufPos);

						*Byte =  k.c16;

						InvalidateByte(Cursor.Index);

						b->SetDirty();
						SetCursor(b, Cursor.Index + 1);
					}
				}

				return true;
			}			
			break;
		}
		case VK_RIGHT:
		{
			if (b && k.Down())
			{
				if (Cursor.Pane == HexPane)
				{
					if (Cursor.Nibble == 0)
					{
						SetCursor(b, Cursor.Index, 1, k.Shift());
					}
					else if (Cursor.Index < b->Size - 1)
					{
						SetCursor(b, Cursor.Index + 1, 0, k.Shift());
					}
				}
				else
				{
					SetCursor(b, Cursor.Index + 1, 0);
				}
			}
			return true;
			break;
		}
		case VK_LEFT:
		{
			if (b && k.Down())
			{
				if (Cursor.Pane == HexPane)
				{
					if (Cursor.Nibble == 1)
					{
						SetCursor(b, Cursor.Index, 0, k.Shift());
					}
					else if (Cursor.Index > 0)
					{
						SetCursor(b, Cursor.Index - 1, 1, k.Shift());
					}
				}
				else
				{
					SetCursor(b, Cursor.Index - 1, 0);
				}
			}
			return true;
			break;
		}
		case VK_UP:
		{
			if (b && k.Down())
			{
				SetCursor(b, Cursor.Index - 16, Cursor.Nibble, k.Shift());
			}
			return true;
			break;
		}
		case VK_DOWN:
		{
			if (b && k.Down())
			{
				if (k.Ctrl())
				{
					// Find next difference
					bool Done = false;
					/*
					for (int64 n = Cursor - BufPos + 1; !Done && n < Size; n += Block)
					{
						if (GetData(n, Block))
						{
							int Off = n - BufPos;
							int Len = BufUsed - Off;
							if (Len > Block) Len = Block;

							for (int i=0; i<Len; i++)
							{
								if (CompareMap[Off + i])
								{
									SetCursor(BufPos + Off + i, 0);
									Done = true;
									break;
								}
							}
						}
						else break;
					}
					*/

					if (!Done)
						LgiMsg(this, "No differences.", AppName);
				}
				else
				{
					// Down
					SetCursor(b, Cursor.Index + 16, Cursor.Nibble, k.Shift());
				}
			}
			return true;
			break;
		}
		case VK_PAGEUP:
		{
			if (b && k.Down())
			{
				if (k.Ctrl())
				{
					SetCursor(b, Cursor.Index - (Lines * 16 * 16), Cursor.Nibble, k.Shift());
				}
				else
				{
					SetCursor(b, Cursor.Index - (Lines * 16), Cursor.Nibble, k.Shift());
				}
			}
			return true;
			break;
		}
		case VK_PAGEDOWN:
		{
			if (b && k.Down())
			{
				if (k.Ctrl())
				{
					SetCursor(b, Cursor.Index + (Lines * 16 * 16), Cursor.Nibble, k.Shift());
				}
				else
				{
					SetCursor(b, Cursor.Index + (Lines * 16), Cursor.Nibble, k.Shift());
				}
			}
			return true;
			break;
		}
		case VK_HOME:
		{
			if (b && k.Down())
			{
				if (k.Ctrl())
				{
					SetCursor(b, 0, 0, k.Shift());
				}
				else
				{
					SetCursor(b, Cursor.Index - (Cursor.Index % 16), 0, k.Shift());
				}
			}
			return true;
			break;
		}
		case VK_END:
		{
			if (b && k.Down())
			{
				if (k.Ctrl())
				{
					SetCursor(b, b->Size - 1, 1, k.Shift());
				}
				else
				{
					SetCursor(b, Cursor.Index - (Cursor.Index % 16) + 15, 1, k.Shift());
				}
			}
			return true;
			break;
		}
		case VK_BACKSPACE:
		{
			if (b && k.Down() && !k.IsChar)
			{
				if (Cursor.Pane == HexPane)
				{
					if (Cursor.Nibble == 0)
					{
						SetCursor(b, Cursor.Index - 1, 1);
					}
					else
					{
						SetCursor(b, Cursor.Index, 0);
					}
				}
				else
				{
					SetCursor(b, Cursor.Index - 1);
				}
			}
			return true;
			break;
		}
		case '\t':
		{
			if (k.Down())
			{
				if (k.IsChar)
				{
					if (Cursor.Pane == HexPane)
					{
						Cursor.Pane = AsciiPane;
					}
					else
					{
						Cursor.Pane = HexPane;
					}

					Invalidate();
				}
			}
			return true;
			break;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////
AppWnd::AppWnd() : GDocApp<GOptionsFile>(AppName, "MAIN")
{
	/*
	if (Options = new GOptionsFile("iHexOptions"))
	{
		Options->Serialize(false);
	}
	*/
	
	#ifdef MAC
	LgiGetResObj(false, "ihex");
	#endif

	Tools = 0;
	Status = 0;
	Active = false;
	Doc = 0;
	Search = 0;
	Bar = 0;
	Split = 0;
	Visual = 0;
	TextView = 0;

	if (_Create())
	{
		DropTarget(true);
		if (_LoadMenu("IDM_MENU", NULL, IDM_FILE_MENU, IDM_RECENT_MENU))
		{
			GMenuItem *i = Menu->FindItem(IDM_SAVEAS);
			DeleteObj(i);

			CmdSave.MenuItem = Menu->FindItem(IDM_SAVE);
			CmdClose.MenuItem = Menu->FindItem(IDM_CLOSE);
			CmdChangeSize.MenuItem = Menu->FindItem(IDM_CHANGE_SIZE);
			CmdFind.MenuItem = Menu->FindItem(IDM_SEARCH);
			CmdNext.MenuItem = Menu->FindItem(IDM_NEXT);
		}

		Tools = LgiLoadToolbar(this, "Tools.gif", 24, 24);
		if (Tools)
		{
			Tools->TextLabels(true);
			Tools->Attach(this);
			Tools->AppendButton("Open", IDM_OPEN);
			CmdSave.ToolButton = Tools->AppendButton("Save", IDM_SAVE, TBT_PUSH, false);
			// CmdSaveAs.ToolButton = Tools->AppendButton("Save As", IDM_SAVEAS, TBT_PUSH, false);
			CmdFind.ToolButton = Tools->AppendButton("Search", IDM_SEARCH, TBT_PUSH, false, 3);
			Tools->AppendSeparator();
			CmdVisualise.ToolButton = Tools->AppendButton("Visualise", IDM_VISUALISE, TBT_TOGGLE, false, 4);
			CmdText.ToolButton = Tools->AppendButton("Text", IDM_TEXTVIEW, TBT_TOGGLE, false, 5);
		}

		PourAll();
		Bar = new IHexBar(this, Tools ? Tools->Y() : 20);

		Status = new GStatusBar;
		if (Status)
		{
			StatusInfo[0] = Status->AppendPane("", -1);
			StatusInfo[1] = Status->AppendPane("", 200);
			if (StatusInfo[1]) StatusInfo[1]->Sunken(true);
			Status->Attach(this);
		}

		Doc = new GHexView(this, Bar);
		if (Bar)
		{
			Bar->View = Doc;
		}

		OnDirty(false);
		
		#ifdef LINUX
		GAutoString f(LgiFindFile("icon-32x32.png"));
		printf("Fixme: add icon to window here...\n");
		#endif
		
		Visible(true);
		PourAll();
		
		DropTarget(true);
	}
}

AppWnd::~AppWnd()
{
	DeleteObj(Search);
	LgiApp->AppWnd = 0;
	DeleteObj(Bar);
	_Destroy();
}

void AppWnd::OnReceiveFiles(GArray<char*> &Files)
{
	if (Files.Length() > 0)
	{
		if (OpenFile(Files[0], false) &&
			Files.Length() > 1)
		{
			Doc->CompareFile(Files[1]);
		}
	}
}

bool AppWnd::OnRequestClose(bool OsShuttingDown)
{
	if (!Active)
	{
		return GWindow::OnRequestClose(OsShuttingDown);
	}

	return false;
}

void AppWnd::OnDirty(bool NewValue)
{
	CmdSave.Enabled(NewValue);
	CmdSaveAs.Enabled(NewValue);
	
	// CmdClose.Enabled(Doc && Doc->HasFile());
	// CmdChangeSize.Enabled(Doc && Doc->HasFile());
}

bool AppWnd::OnKey(GKey &k)
{
	return false;
}

void AppWnd::OnPosChange()
{
	GDocApp<GOptionsFile>::OnPosChange();
}

int AppWnd::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_HEX_VIEW:
		{
			if (Flags == GNotifyCursorChanged)
			{
				char *Data;
				int Len;
				if (Visual && Doc->GetDataAtCursor(Data, Len))
				{
					Visual->Visualise(Data, Len, GetCtrlValue(IDC_LITTLE) );
				}
				if (TextView && Doc->GetDataAtCursor(Data, Len))
				{
					GStringPipe p(1024);
					for (char *s = Data; s < Data + Len && s < Data + (4 << 10) && *s; s++)
					{
						if (*s >= ' ' || *s == '\n' || *s == '\t')
						{
							p.Push(s, 1);
						}
					}
					char *t = p.NewStr();
					if (t)
					{
						TextView->Name(t);
						DeleteArray(t);
					}
				}
				
				int SelLen = Doc->GetSelectedNibbles();
				char s[256];
				sprintf_s(s, sizeof(s), "Selection: %.1f bytes", (double)SelLen/2.0);
				StatusInfo[1]->Name(SelLen ? s : NULL);
			}
			break;
		}
	}
	return GDocApp<GOptionsFile>::OnNotify(Ctrl, Flags);
}

void AppWnd::OnPulse()
{
}

GMessage::Result AppWnd::OnEvent(GMessage *Msg)
{
	return GDocApp<GOptionsFile>::OnEvent(Msg);
}

void AppWnd::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

#define SPLIT_X		590

void AppWnd::ToggleVisualise()
{
	if (GetCtrlValue(IDM_VISUALISE))
	{
		if (!Split)
			Split = new GSplitter;

		if (Split)
		{
			GAutoString DefVisual;
			LgiApp->GetOption("visual", DefVisual);
			
			Doc->Detach();
			Split->Value(SPLIT_X);
			Split->Border(false);
			Split->Raised(false);
			Split->Attach(this);
			Split->SetViewA(Doc, false);
			Split->SetViewB(Visual = new GVisualiseView(this, DefVisual), false);
		}
	}
	else
	{
		Doc->Detach();
		DeleteObj(Split);
		Doc->Attach(this);
		Visual = 0;
	}

	PourAll();
}

void AppWnd::ToggleTextView()
{
	if (GetCtrlValue(IDM_TEXTVIEW))
	{
		SetCtrlValue(IDM_VISUALISE, false);

		if (!Split)
			Split = new GSplitter;

		if (Split)
		{
			Doc->Detach();
			Split->Value(SPLIT_X);
			Split->Border(false);
			Split->Raised(false);
			Split->Attach(this);
			Split->SetViewA(Doc, false);
			Split->SetViewB(TextView = new GTextView3(100, 0, 0, 100, 100, 0), false);
		}
	}
	else
	{
		Doc->Detach();
		DeleteObj(Split);
		Doc->Attach(this);
		TextView = 0;
	}

	PourAll();
}

int Cmp(char **a, char **b)
{
	return stricmp(*a, *b);
}

int AppWnd::OnCommand(int Cmd, int Event, OsView Wnd)
{
	switch (Cmd)
	{
		case IDM_COPY_HEX:
		{
			Doc->Copy(FmtHex);
			break;
		}
		case IDM_COPY_TEXT:
		{
			Doc->Copy(FmtText);
			break;
		}
		case IDM_COPY_CODE:
		{
			Doc->Copy(FmtCode);
			break;
		}
		case IDM_PASTE:
		{
			Doc->Paste();
			break;
		}
		case IDM_COMBINE_FILES:
		{
			GFileSelect s;
			s.Parent(this);
			s.MultiSelect(true);
			if (s.Open())
			{
				int64 Size = 0;
				int i;
				for (i=0; i<s.Length(); i++)
				{
					char *f = s[i];
					Size += LgiFileSize(f);
				}
				
				GFileSelect o;
				if (o.Save())
				{
					GFile Out;
					if (Out.Open(o.Name(), O_WRITE))
					{
						GProgressDlg Dlg(this);
						Dlg.SetLimits(0, Size);
						Dlg.SetType("MB");
						Dlg.SetScale(1.0/1024.0/1024.0);
						GArray<char> Buf;
						Buf.Length(1 << 20);
						Out.SetSize(0);
						GArray<char*> Files;
						for (i=0; i<s.Length(); i++)
							Files[i] = s[i];
						
						Files.Sort(Cmp);

						for (i=0; i<Files.Length(); i++)
						{
							GFile In;
							if (In.Open(Files[i], O_READ))
							{
								printf("Appending %s\n", Files[i]);
								char *d = strrchr(Files[i], DIR_CHAR);
								if (d) Dlg.SetDescription(d+1);
								
								int64 Fs = In.GetSize();
								for (int64 p = 0; p<Fs; )
								{
									int r = In.Read(&Buf[0], Buf.Length());
									if (r > 0)
									{
										int w = Out.Write(&Buf[0], r);
										if (w != r)
										{
											printf("%s:%i - Write error...!\n", _FL);
											break;
										}
										
										p += w;
										Dlg.Value(Dlg.Value() + w);
										LgiYield();
									}
									else break;
								}
							}
							else printf("%s:%i - Can't open %s\n", _FL, Files[i]);
						}
					}
					else printf("%s:%i - Can't open %s\n", _FL, o.Name());
				}
			}
			break;
		}
		case IDM_VISUALISE:
		{
			if (GetCtrlValue(IDM_TEXTVIEW))
			{
				SetCtrlValue(IDM_TEXTVIEW, false);
				ToggleTextView();
			}
			ToggleVisualise();
			OnNotify(Doc, GNotifyCursorChanged);
			break;
		}
		case IDM_TEXTVIEW:
		{
			if (GetCtrlValue(IDM_VISUALISE))
			{
				SetCtrlValue(IDM_VISUALISE, false);
				ToggleVisualise();
			}
			ToggleTextView();
			OnNotify(Doc, GNotifyCursorChanged);
			break;
		}
		case IDM_SAVE:
		{
			if (Doc)
				Doc->Save();
			break;
		}
		case IDM_EXIT:
		{
			if (Doc)
				Doc->Empty();

			LgiCloseApp();
			break;
		}
		case IDM_NEW_BUFFER:
		{
			if (!Doc)
				break;
			Doc->CreateFile(256);
			break;
		}
		case IDM_CLOSE:
		{
			if (Doc && Doc->HasFile())
				Doc->CloseFile();
			else
				LgiCloseApp();
			break;
		}
		case IDM_SAVE_SELECTION:
		{
			if (Doc)
			{
				GFileSelect s;
				s.Parent(this);
				if (s.Save())
				{
					Doc->SaveSelection(NULL, s.Name());
				}
			}
			break;
		}
		case IDM_FILL_RND:
		{
			if (!Doc)
				break;

			RandomData Rnd;
			Doc->SelectionFillRandom(&Rnd);
			break;
		}
		case IDM_SEARCH:
		{
			if (Doc)
			{
				DeleteObj(Search);
				Search = new SearchDlg(this);
				if (Search && Search->DoModal() == IDOK)
				{
					Doc->DoSearch(Search);
				}
			}
			break;
		}
		case IDM_NEXT:
		{
			if (Doc && Search)
			{
				Doc->DoSearch(Search);
			}
			break;
		}
		case IDM_FILE_COMPARE:
		{
			if (Doc && Doc->HasFile())
			{
				GFileSelect s;
				s.Parent(this);
				if (s.Open())
				{
					char *Sn = s.Name();
					Doc->CompareFile(Sn);
				}
			}
			break;
		}
		case IDM_CHANGE_SIZE:
		{
			if (Doc)
			{
				GHexBuffer *Cur = Doc->GetCursorBuffer();
				if (Cur)
				{
					ChangeSizeDlg Dlg(this, Cur->Size);
					if (Dlg.DoModal())
					{
						Doc->SetFileSize(Dlg.Size);
					}
				}
			}
			break;
		}
		case IDM_SELECT_ALL:
		{
			if (Doc)
				Doc->SelectAll();
			break;
		}
		case IDM_HELP:
		{
			Help("index.html");
			break;
		}
		case IDM_ABOUT:
		{
			GAbout Dlg(	this,
						AppName,
						APP_VER,
						"\nSimple Hex Viewer",
						"_about.gif",
						"http://www.memecode.com/ihex.php",
						"fret@memecode.com");
			break;
		}
	}
	
	return GDocApp<GOptionsFile>::OnCommand(Cmd, Event, Wnd);
}

void AppWnd::Help(const char *File)
{
	char e[300];
	if (File && LgiGetExePath(e, sizeof(e)))
	{
		#ifdef WIN32
		if (stristr(e, "\\Release") || stristr(e, "\\Debug"))
			LgiTrimDir(e);
		#endif
		LgiMakePath(e, sizeof(e), e, "Help");
		LgiMakePath(e, sizeof(e), e, File);
		if (FileExists(e))
		{
			LgiExecute(e);
		}
		else
		{
			LgiMsg(this, "The help file '%s' doesn't exist.", AppName, MB_OK, e);
		}
	}
}

void AppWnd::SetStatus(int Pos, char *Text)
{
	if (Pos >= 0 && Pos < 3 && StatusInfo[Pos] && Text)
	{
		StatusInfo[Pos]->Name(Text);
	}
}

GRect GetClient(GView *w)
{
	#ifdef WIN32
	RECT r = {0, 0, 0, 0};
	if (w)
	{
		GetClientRect(w->Handle(), &r);
	}
	return GRect(r);
	#else
	return GRect(0, 0, (w)?w->X()-1:0, (w)?w->Y()-1:0);
	#endif
}

void AppWnd::PourAll()
{
	GDocApp<GOptionsFile>::PourAll();
}

bool AppWnd::OpenFile(char *FileName, bool ReadOnly)
{
	bool Status = false;
	if (Doc)
	{
		Status = Doc->OpenFile(FileName, ReadOnly);
		OnDocument(Status);
		OnDirty(GetDirty());
	}
	return Status;
}

bool AppWnd::SaveFile(char *FileName)
{
	bool Status = false;
	if (Doc)
	{
		Status = Doc->SaveFile(NULL, FileName);
	}
	return Status;
}

void AppWnd::OnDocument(bool Valid)
{
	// LgiTrace("%s:%i - OnDocument(%i)\n", _FL, Valid);
	CmdFind.Enabled(Valid);
	CmdNext.Enabled(Valid);
	CmdVisualise.Enabled(Valid);
	CmdText.Enabled(Valid);

	bool Dirt = GetDirty();
	CmdSave.Enabled(Valid && Dirt);
	CmdSaveAs.Enabled(Valid);
	
	CmdClose.Enabled(Valid);
	CmdChangeSize.Enabled(Valid);
}

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, "i.Hex");
	if (a.IsOk())
	{
		a.AppWnd = new AppWnd;
		a.Run();
	}

	return 0;
}
