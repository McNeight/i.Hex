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

enum FormatType
{
	FmtText,
	FmtHex,
	FmtCode
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Application identification
const char *AppName = "i.Hex";
bool CancelSearch = false;

#define ColourSelectionFore			Rgb24(255, 255, 0)
#define ColourSelectionBack			Rgb24(0, 0, 255)
#define	CursorColourBack			Rgb24(192, 192, 192)

#define HEX_COLUMN					13
#define TEXT_COLUMN					63

#define FILE_BUFFER_SIZE			1024
#define	UI_UPDATE_SPEED				500 // ms

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

	int Read(void *Ptr, int Len, int Flags = 0)
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

///////////////////////////////////////////////////////////////////////////////////////////////
enum PaneType
{
	HexPane,
	AsciiPane
};

class IHexBar;
class GHexView : public GLayout
{
	AppWnd *App;
	IHexBar *Bar;
	GFont *Font;
	GArray<GRect> DbgRect;
	
	// General display parameters
		bool IsHex; // Display offsets in hex
		bool IsReadOnly; // Data is read only
		GdcPt2 CharSize; // Size of character in pixels
		int BytesPerLine; // Number of bytes to display on each line
		int IntWidth; // Number of bytes to display in one contiguous number

	// Layout / paint state
		int CurrentY;

	// File
	GFile *File;
	int64 Size;

	// Comparision file
	GFile *Compare;
	GArray<uint8> BufA, BufB;
	int UsedA, UsedB;
	diff_info DiffInfo;
	
	struct Layout
	{
		int Len[2];
		int64 Offset[2];
		uint8 *Data[2];
		GRect Pos[2];
		bool Same;
		
		Layout()
		{
			Same = false;
			Len[0] = Len[1] = 0;
			Offset[0] = Offset[1] = 0;
			Data[0] = Data[1] = NULL;
		}
	};
	GArray<Layout> CmpLayout;
	
	void PaintLayout(GSurface *pDC, Layout &l, GRect &client);

	// Buffer
	uchar *Buf;			// Buffer for data from the file
	int BufLen;			// Length of the data buffer
	int BufUsed;		// Length of the buffer used
	int64 BufPos;		// Where the start of the buffer came from in the file
	
	// Cursor
	int64 Cursor;		// Offset into the file that the cursor is over
	int Nibble;			// 0 or 1, defining the nibble pointed to
						// 0: 0xc0, 1: 0x0c etc
	PaneType Pane;		// 0 = hex, 1 = ascii
	bool Flash;
	GArray<GRect> CursorPos;

	// Selection
	int64 Select;
	int SelectNibble;

	bool GetData(int64 Start, int Len);
	void SwapBytes(void *p, int Len);
	// GRect GetPositionAt(int64 Offset);

public:
	GHexView(AppWnd *app, IHexBar *bar);
	~GHexView();

	bool OpenFile(char *FileName, bool ReadOnly);
	bool SaveFile(char *FileName);
	bool HasFile() { return File != 0; }
	void SaveSelection(char *File);
	void SelectionFillRandom(GStream *Rnd);
	void CompareFile(char *File);

	void Copy(FormatType Fmt);
	void Paste();

	bool HasSelection() { return Select >= 0; }
	int GetSelectedNibbles();
	void SetScroll();
	void SetCursor(int64 cursor, int nibble = 0, bool select = false);
	void SetIsHex(bool i);
	int64 GetFileSize() { return Size; }
	bool SetFileSize(int64 Size);
	void DoInfo();
	int64 Search(SearchDlg *For, uchar *Bytes, int Len);
	void DoSearch(SearchDlg *For);
	bool GetCursorFromLoc(int x, int y, int64 &Cursor, int &Nibble);
	bool GetDataAtCursor(char *&Data, int &Len);
	void SetBit(uint8 Bit, bool On);
	void SetByte(uint8 Byte);
	void SetShort(uint16 Byte);
	void SetInt(uint32 Byte);
	void InvalidateCursor();
	void InvalidateLines(GArray<GRect> &a, GArray<GRect> &b);
	bool GetLocationOfByte(GArray<GRect> &Loc, int64 Offset, const char16 *LineBuf);

	bool Pour(GRegion &r);

	int OnNotify(GViewI *c, int f);
	void OnPosChange();
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	void OnFocus(bool f);
	bool OnKey(GKey &k);
	bool OnMouseWheel(double Lines);
	void OnPulse();
	void OnCreate() { SetPulse(500); }
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

				View->SetCursor(Select ? Off - 1 : Off, Select, Select);

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
GHexView::GHexView(AppWnd *app, IHexBar *bar)
{
	// Init
	Flash = true;
	App = app;
	Bar = bar;
	File = 0;
	Buf = 0;
	BufLen = 0;
	BufUsed = 0;
	BufPos = 0;
	Cursor = -1;
	Nibble = 0;
	Font = 0;
	Size = 0;
	CharSize.x = 8;
	CharSize.y = 16;
	IsHex = true;
	Pane = HexPane;
	Select = -1;
	SelectNibble = 0;
	IsReadOnly = false;
	Compare = 0;
	
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
	DeleteObj(Compare);
	DeleteObj(Font);
	DeleteObj(File);
	DeleteArray(Buf);
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

bool GHexView::SetFileSize(int64 size)
{
	// Save any changes
	if (App->SetDirty(false))
	{
		Size = File->SetSize(size);

		int p = BufPos;
		BufPos++;
		GetData(p, 1);

		SetScroll();
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
	GClipBoard c(this);

	int64 Min = min(Select, Cursor);
	int64 Max = max(Select, Cursor);
	int64 Len = Max - Min + 1;

	if (GetData(Min, Len))
	{
		uint64 Offset = Min - BufPos;
		uchar *Ptr = Buf + Offset;
		
		if (Len > BufUsed - Offset)
		{
			Len = BufUsed - Offset;
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
	#ifdef WIN32
	int Len = 0;
	if (c.Binary(CF_PRIVATEFIRST, Ptr, &Len))
	{
		if (GetData(Cursor, Len))
		{
			memcpy(Buf + Cursor - BufPos, Ptr, Len);
			App->SetDirty(true);
			Invalidate();
			DoInfo();
		}	
	}
	else
	#endif
	{
		GAutoString Txt(c.Text());
		if (Txt)
		{
			// Convert from binary...
			GArray<uint8> Out;
			int High = -1;
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

			if (Out.Length() &&
				GetData(Cursor, Out.Length()))
			{
				memcpy(Buf + Cursor - BufPos, &Out[0], Out.Length());
				App->SetDirty(true);
				Invalidate();
				DoInfo();
			}
		}
	}
}

void GHexView::SetScroll()
{
	if (VScroll)
	{
		int Lines = GetClient().Y() / CharSize.y;
		int64 DocLines = (Size + 15) / 16;

		VScroll->SetLimits(0, DocLines > 0 ? DocLines - 1 : 0);
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

bool GHexView::GetData(int64 Start, int Len)
{
	static bool IsAsking = false;
	bool Status = false;

	if (!IsAsking && File && File->IsOpen())
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
		if (Start < BufPos || (Start + Len) >= (BufPos + BufLen))
		{
			// clear changes
			BufUsed = 0;
			
			IsAsking = true;
			bool IsClean = App->SetDirty(false);
			IsAsking = false;

			if (IsClean)
			{
				// move buffer to cover cursor pos
				int Half = BufLen >> 1;
				BufPos = Start - (Start % Half);
				if (File->Seek(BufPos, SEEK_SET) == BufPos)
				{
					memset(Buf, 0xcc, BufLen);
					BufUsed = File->Read(Buf, BufLen);
					Status =	(Start >= BufPos) &&
								((Start + Len) < (BufPos + BufUsed));

					// Check for comparision file
					if (Compare)
					{
						if (Compare->Seek(BufPos, SEEK_SET) == BufPos)
						{
							// Compare->Read(CompareMap, BufUsed);
						}
					}
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

bool GHexView::GetDataAtCursor(char *&Data, int &Len)
{
	if (Buf)
	{
		int Offset = Cursor - BufPos;
		Data = (char*)Buf + Offset;
		Len = min(BufUsed, BufLen) - Offset;
		return true;
	}
	return false;
}

int GHexView::GetSelectedNibbles()
{
	if (Select < 0)
		return 0;

	int c = (Cursor << 1) + Nibble;
	int s = (Select << 1) + SelectNibble;
	
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

void GHexView::SetCursor(int64 cursor, int nibble, bool Selecting)
{
	GArray<GRect> OldLoc, NewLoc;
	bool SelectionChanging = false;
	bool SelectionEnding = false;
	
	if (Selecting)
	{
		if (Select < 0)
		{
			// Start selection
			Select = Cursor;
			SelectNibble = Nibble;
		}

		SelectionChanging = true;
	}
	else
	{
		if (Select >= 0)
		{
			// Deselecting
			
			// Repaint the entire selection area...
			GetLocationOfByte(NewLoc, Cursor, NULL);
			GetLocationOfByte(OldLoc, Select, NULL);
			InvalidateLines(NewLoc, OldLoc);
			
			SelectionEnding = true;
			Select = -1;
		}
	}

	if (!SelectionEnding)
		GetLocationOfByte(OldLoc, Cursor, NULL);
	// else the selection just ended and the old cursor location just got repainted anyway

	// Limit to doc
	if (cursor >= Size)
	{
		cursor = Size - 1;
		nibble = 1;
	}
	if (cursor < 0)
	{
		cursor = 0;
		nibble = 0;
	}

	// Is different?
	if (Cursor != cursor ||
		Nibble != nibble)
	{
		// Set the cursor
		Cursor = cursor;
		Nibble = nibble;

		// Make sure the cursor is in the viewable area?
		if (VScroll)
		{
			int64 Start = (uint64) (VScroll ? VScroll->Value() : 0) * 16;
			int Lines = GetClient().Y() / CharSize.y;
			int64 End = min(Size, Start + (Lines * 16));
			if (Cursor < Start)
			{
				// Scroll up
				VScroll->Value((Cursor - (Cursor%16)) / 16);
				Invalidate();
			}
			else if (Cursor >= End)
			{
				// Scroll down
				int64 NewVal = (Cursor - (Cursor%16) - ((Lines-1) * 16)) / 16;
				VScroll->Value(NewVal);
				Invalidate();
			}
		}

		if (Bar)
		{
			Bar->SetOffset(Cursor);
			DoInfo();
		}
		
		Flash = true;

		GetLocationOfByte(NewLoc, Cursor, NULL);
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
			GetLocationOfByte(NewLoc, Cursor, NULL);
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

	// Search through to the end of the file...
	for (c = Cursor + 1; c < GetFileSize(); c += Block)
	{
		int Actual = min(Block, GetFileSize() - c);
		if (GetData(c, Actual))
		{
			Hit = Search(For, Buf + (c - BufPos), Actual);
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
					Prog->Value(c - Cursor);
					LgiYield();
				}
			}
		}
		else break;
	}

	if (Hit < 0)
	{
		// Now search from the start of the file to the original cursor
		for (c = 0; c < Cursor; c += Block)
		{
			if (GetData(c, Block))
			{
				int Actual = min(Block, Cursor - c);
				Hit = Search(For, Buf + (c - BufPos), Actual);
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
						Prog->Value(GetFileSize()-Cursor+c);
						LgiYield();
					}
				}
			}
			else break;
		}
	}

	if (Hit >= 0)
	{
		SetCursor(Hit);
		SetCursor(Hit + For->Length - 1, 1, true);
	}

	DeleteObj(Prog);
}

void GHexView::SetBit(uint8 Bit, bool On)
{
	if (GetData(Cursor, 1))
	{
		if (On)
		{
			Buf[Cursor-BufPos] |= Bit;
		}
		else
		{
			Buf[Cursor-BufPos] &= ~Bit;
		}

		App->SetDirty(true);
		Invalidate();
		DoInfo();
	}	
}

void GHexView::SetByte(uint8 Byte)
{
	if (GetData(Cursor, 1))
	{
		if (Buf[Cursor-BufPos] != Byte)
		{
			Buf[Cursor-BufPos] = Byte;
			App->SetDirty(true);
			Invalidate();
			DoInfo();
		}
	}	
}

void GHexView::SetShort(uint16 Short)
{
	if (GetData(Cursor, 2))
	{
		SwapBytes(&Short, sizeof(Short));

		uint16 *p = (uint16*) (&Buf[Cursor-BufPos]);
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
	if (GetData(Cursor, 4))
	{
		SwapBytes(&Int, sizeof(Int));

		uint32 *p = (uint32*) (&Buf[Cursor-BufPos]);
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
	if (Bar)
	{
		bool IsSigned = Bar->IsSigned();
		GView *w = GetWindow();

		char s[256] = "";
		if (GetData(Cursor, 1))
		{
			int c = Buf[Cursor-BufPos], sc;
			if (IsSigned)
				sc = (char)Buf[Cursor-BufPos];
			else
				sc = Buf[Cursor-BufPos];

			Bar->NotifyOff = true;

			sprintf(s, "%i", sc);
			w->SetCtrlName(IDC_DEC_1, s);
			sprintf(s, "%02.2X", c);
			w->SetCtrlName(IDC_HEX_1, s);
			sprintf(s, "%c", c >= ' ' && c <= 0x7f ? c : '.');
			w->SetCtrlName(IDC_ASC_1, s);

			uint8 Bits = Buf[Cursor-BufPos];
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
		if (GetData(Cursor, 2))
		{
			uint16 *sp = (uint16*)(Buf+(Cursor-BufPos));
			uint16 c = *sp;
			SwapBytes(&c, sizeof(c));
			int c2 = (int16)c;

			sprintf(s, "%i", IsSigned ? c2 : c);
			w->SetCtrlName(IDC_DEC_2, s);
			sprintf(s, "%04.4X", c);
			w->SetCtrlName(IDC_HEX_2, s);
		}
		if (GetData(Cursor, 4))
		{
			uint32 *lp = (uint32*)(Buf+(Cursor-BufPos));
			uint32 c = *lp;
			SwapBytes(&c, sizeof(c));

			sprintf(s, IsSigned ? "%i" : "%u", c);
			w->SetCtrlName(IDC_DEC_4, s);
			sprintf(s, "%08.8X", c);
			w->SetCtrlName(IDC_HEX_4, s);
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
	#if 1
	
	if (!FileToArray(BufB, CmpFile))
		return;
	
	if (!GetData(0, Size))
		return;		
	
	/*
	if (binary_diff(DiffInfo, Buf, BufUsed, &BufB[0], BufB.Length()))
	{
		for (unsigned i=0; i<DiffInfo.ctrl.Length(); i++)
		{
			ctrl_info &c = DiffInfo.ctrl[i];
			LgiTrace("ctrl[%i]=%i,%i,%i\n", i, (int)c.a[0], (int)c.a[1], (int)c.a[2]);
		}
	}
	*/
	
	#else
	
	DeleteObj(Compare);
	CmpItems.Length(0);
	CmpLayout.Length(0);
	BufA.Length(0);
	BufB.Length(0);

	if (!File)
		return;

	if (!(Compare = new GFile))
		return;

	if (!Compare->Open(CmpFile, O_READ))
	{
		DeleteObj(Compare);
		return;
	}

	UsedA = UsedB = 0;
	BufA.Length(BufLen);
	BufB.Length(BufLen);

	// Read main file
	File->SetPos(BufPos);
	UsedA = File->Read(&BufA[0], BufLen);
	if (UsedA < BufA.Length())
		BufA.Length(UsedA);
	
	// Read comparison file
	Compare->SetPos(BufPos);
	UsedB = Compare->Read(&BufB[0], BufLen);
	if (UsedB < BufB.Length())
		BufB.Length(UsedB);
	
	// If we have no more data to compare, bail with an error
	if (UsedA <= 0 && UsedB <= 0)
		return;
	
	Diff Df;
	CmpItems = Df.DiffInt(BufA, BufB);

	int64 PosA = 0, PosB = 0;
	for (int i=0; i<CmpItems.Length(); i++)
	{
		Diff::Item &it = CmpItems[i];
		// LgiTrace("Diff %i,%i del=%i ins=%i\n", it.StartA, it.StartB, it.deletedA, it.insertedB);
		
		if (it.StartA > PosA &&
			it.StartB > PosB)
		{
			// Insert unchanged block
			int a = it.StartA - PosA;
			int b = it.StartB - PosB;
			LgiAssert(a == b); // ?

			Layout &l = CmpLayout.New();
			l.Same = true;
			l.Len[0] = a;
			l.Len[1] = b;
			l.Offset[0] = PosA;
			l.Offset[1] = PosB;
			l.Data[0] = &BufA[0] + PosA;
			l.Data[1] = &BufB[0] + PosB;
			PosA += a;
			PosB += b;
		}
		
		// Insert change block
		Layout &l = CmpLayout.New();
		l.Len[0] = it.deletedA;
		l.Len[1] = it.insertedB;
		l.Offset[0] = PosA;
		l.Offset[1] = PosB;
		l.Data[0] = &BufA[0] + it.StartA;
		l.Data[1] = &BufB[0] + it.StartB;
		PosA += it.deletedA;
		PosB += it.insertedB;
	}

	if (PosA < UsedA || PosB < UsedB)
	{
		// Insert layout block
		int a = UsedA - PosA;
		int b = UsedB - PosB;
		LgiAssert(a == b); // ?

		Layout &l = CmpLayout.New();
		l.Same = true;
		l.Len[0] = a;
		l.Len[1] = b;
		l.Offset[0] = PosA;
		l.Offset[1] = PosB;
		l.Data[0] = &BufA[0] + PosA;
		l.Data[1] = &BufB[0] + PosB;
		PosA += a;
		PosB += b;
	}

	int CharsPerLine =	11 + // offset
						2 + // separator
						(BytesPerLine * 3) + // hex data
						2 + // separator
						BytesPerLine + // Acsii
						4; // separator

	int y = 0;
	for (int i=0; i<CmpLayout.Length(); i++)
	{
		Layout &l = CmpLayout[i];
		
		// First position the layout
		int Bytes = max(l.Len[0], l.Len[1]);
		int Lines = (Bytes + BytesPerLine - 1) / BytesPerLine;
		l.Pos[0].ZOff(CharsPerLine * CharSize.x, Lines * CharSize.y - 1);
		l.Pos[1] = l.Pos[0];
		l.Pos[0].Offset(0, y);
		l.Pos[1].Offset(l.Pos[0].x2 + 1, y);
		
		y += Lines * CharSize.y;
	}
	
	Invalidate();
	
	#endif
}

bool GHexView::OpenFile(char *FileName, bool ReadOnly)
{
	bool Status = false;

	if (App->SetDirty(false))
	{
		DeleteObj(Compare);
		// CmpItems.Length(0);
		CmpLayout.Length(0);
		BufA.Length(0);
		BufB.Length(0);
		DeleteObj(File);
		DeleteArray(Buf);
		BufPos = 0;

		if (FileName)
		{
			File = new GFile;
			if (File)
			{
				bool IsOpen = false;

				if (FileExists(FileName))
				{
					if (!File->Open(FileName, ReadOnly ? O_READ : O_READWRITE))
					{
						if (File->Open(FileName, O_READ))
						{
							IsReadOnly = true;
							IsOpen = true;
						}
					}
					else
					{
						IsOpen = true;
						IsReadOnly = false;
					}
				}

				if (IsOpen)
				{
					Focus(true);
					Size = File->GetSize();
					SetCursor(0);
					SetScroll();
					Status = true;
				}
				else
				{
					LgiMsg(this, "Couldn't open '%s' for reading.", AppName, MB_OK, FileName);
				}
			}
		}
		else if (VScroll)
		{
			VScroll->SetLimits(0, -1);
			Size = 0;
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
	}

	return Status;
}

bool GHexView::SaveFile(char *FileName)
{
	bool Status = false;

	if (File &&
		FileName)
	{
		if (stricmp(FileName, File->GetName()) == 0)
		{
			if (File->Seek(BufPos, SEEK_SET) == BufPos)
			{
				int Len = min(BufLen, Size - BufPos);
				Status = File->Write(Buf, Len) == Len;
			}
		}
	}

	return Status;
}

void GHexView::SaveSelection(char *FileName)
{
	if (File && FileName)
	{
		GFile f;
		if (Select >= 0 &&
			f.Open(FileName, O_WRITE))
		{
			int64 Min = min(Select, Cursor);
			int64 Max = max(Select, Cursor);
			int64 Len = Max - Min + 1;

			f.SetSize(Len);
			f.SetPos(0);

			int64 Block = 4 << 10;
			for (int64 i=0; i<Len; i+=Block)
			{
				int64 AbsPos = Min + i;
				int64 Bytes = min(Block, Len - i);
				if (GetData(AbsPos, Bytes))
				{
					uchar *p = Buf + (AbsPos - BufPos);
					f.Write(p, Bytes);
				}
			}									
		}
	}
}

void GHexView::SelectionFillRandom(GStream *Rnd)
{
	if (!Rnd || !File)
		return;

	int64 Min = min(Select, Cursor);
	int64 Max = max(Select, Cursor);
	int64 Len = Max - Min + 1;

	File->SetPos(Min);

	{
		int64 Last = LgiCurrentTime();
		int64 Start = Last;
		GProgressDlg Dlg(NULL);
		GArray<char> Buf;
		Buf.Length(2 << 20);
		Dlg.SetLimits(0, Len);
		Dlg.SetScale(1.0 / 1024.0 / 1024.0);
		Dlg.SetType("MB");

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

			int w = File->Write(&Buf[0], Remain);
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
	}

	if (File->SetPos(BufPos) == BufPos)
	{
		BufUsed = File->Read(Buf, BufLen);
	}
	Invalidate();
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
	SetScroll();
	GLayout::OnPosChange();
}

void GHexView::InvalidateCursor()
{
	for (int i=0; i<CursorPos.Length(); i++)
	{
		Invalidate(&CursorPos[i]);
	}
}

void GHexView::OnPulse()
{
	Flash = !Flash;
	InvalidateCursor();
}

GColour ChangedFore(0xf1, 0xe2, 0xad);
GColour ChangedBack(0xef, 0xcb, 0x05);
GColour DeletedBack(0xc0, 0xc0, 0xc0);

void GHexView::PaintLayout(GSurface *pDC, Layout &l, GRect &client)
{
	// First position the layout
	int Bytes = max(l.Len[0], l.Len[1]);
	int Lines = (Bytes + BytesPerLine - 1) / BytesPerLine;
	int Sides = Compare ? 2 : 1;

	// Now draw the layout data
	char s[256];
	COLOUR Fore[256];
	COLOUR Back[256];

	for (int Side = 0; Side < Sides; Side++)
	{
		if (l.Len[Side] == 0)
		{
			pDC->Colour(DeletedBack);
			pDC->Rectangle(&l.Pos[Side]);
			continue;
		}	
		
		for (int Line=0; Line<Lines; Line++)
		{
			int CurY = l.Pos[Side].y1 + (Line * CharSize.y);
			char *p = s;
			int64 LineStart = l.Offset[Side] + (Line * BytesPerLine);
			
			// Clear the colours for this line
			int i;
			for (i=0; i<CountOf(Back); i++)
			{
				Fore[i] = LC_TEXT;
				Back[i] = l.Same ? LC_WORKSPACE : ChangedBack.c24();
			}

			// Create line of text
			if (IsHex)
			{
				p += sprintf(p, "%02.2x:%08.8X  ", (uint)(LineStart >> 32), (uint)LineStart);
			}
			else
			{
				#ifdef WIN32
				p += sprintf(p, "%11.11I64i  ", LineStart);
				#else
				p += sprintf(p, "%11.11lli  ", LineStart);
				#endif
			}

			// Print the hex bytes to the line
			int64 n;
			int64 From = Line * BytesPerLine, To = From + BytesPerLine;
			int StartOfHex = p - s;
			for (n=From; n<To; n++, p+=3)
			{
				if (n < l.Len[Side])
				{
					sprintf(p, "%02.2X ", l.Data[Side][n]);
					if (!l.Same)
					{
						int k = p - s;
						Back[k++] = ChangedFore.c24();
						Back[k++] = ChangedFore.c24();
						Back[k++] = ChangedFore.c24();
					}
				}
				else
				{
					strcat(p, "   ");
				}
			}

			// Separator between hex/ascii
			strcat(s, "  ");
			p += 2;

			// Print the ascii characters to the line
			int StartOfAscii = p - s;
			for (n=From; n<To; n++)
			{
				if (n < l.Len[Side])
				{
					uchar c = l.Data[Side][n];
					if (!l.Same)
					{
						int k = p - s;
						Back[k++] = ChangedFore.c24();
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
			if (Cursor >= l.Offset[Side] && Cursor < l.Offset[Side] + l.Len[Side])
			{
				CursorOff = Cursor - l.Offset[Side];
				if ((CursorOff >= From) && (CursorOff < To))
					CursorOff -= From;
				else
					CursorOff = -1;
			}

			// Draw text
			GRect Tr = l.Pos[Side];

			Font->Colour(LC_TEXT, LC_WORKSPACE);
			char16 *Wide = (char16*)LgiNewConvertCp(LGI_WideCharset, s, "iso-8859-1");
			if (Wide)
			{
				// Paint the selection into the colour buffers
				int64 Min = Select >= 0 ? min(Select, Cursor) : -1;
				int64 Max = Select >= 0 ? max(Select, Cursor) : -1;
				if (Min < LineStart + BytesPerLine &&
					Max >= LineStart)
				{
					// Part or all of this line is selected
					int64 s = ((Select - LineStart) * 3) + SelectNibble;
					int64 e = ((Cursor - LineStart) * 3) + Nibble;
					if (s > e)
					{
						int64 i = s;
						s = e;
						e = i;
					}
					if (s < 0) s = 0;
					if (e > BytesPerLine * 3 - 2) e = BytesPerLine * 3 - 2;

					for (i=s+StartOfHex; i<=e+StartOfHex; i++)
					{
						Fore[i] = ColourSelectionFore;
						Back[i] = ColourSelectionBack;
					}
					for (i=(s/3)+StartOfAscii; i<=(e/3)+StartOfAscii; i++)
					{
						Fore[i] = ColourSelectionFore;
						Back[i] = ColourSelectionBack;
					}
				}

				// Colour the back of the cursor grey...
				if (CursorOff >= 0 && Select < 0 && Flash)
				{
					Back[StartOfHex + (CursorOff * 3) + Nibble] = CursorColourBack;
					Back[StartOfAscii + CursorOff] = CursorColourBack;
				}

				// Go through the colour buffers, painting in runs of similar colour
				GRect r;
				int CxF = Tr.x1 << GDisplayString::FShift;
				int Len = p - s;
				for (i=0; i<Len; )
				{
					int e = i;
					while (e < Len)
					{
						if (Fore[e] != Fore[i] ||
							Back[e] != Back[i])
							break;
						e++;
					}

					int Run = e - i;
					GDisplayString Str(Font, s + i, Run);
					
					r.x1 = CxF;
					r.y1 = CurY << GDisplayString::FShift;
					r.x2 = CxF + Str.FX();
					r.y2 = (CurY + Str.Y()) << GDisplayString::FShift;
					// printf("Line=%i i=%i e=%i Len=%i r=%i,%i,%i,%i\n", Line, i, e, Len, r.x1>>16,r.y1>>16,r.x2>>16,r.y2>>16);
					
					Font->Colour(Fore[i], Back[i]);
					
					Str.FDraw(pDC, CxF, CurY<<GDisplayString::FShift, &r);
					
					CxF += Str.FX();
					i = e;
				}

				int Cx = CxF >> GDisplayString::FShift;
				if (Cx < client.x2)
				{
					pDC->Colour(LC_WORKSPACE, 24);
					pDC->Rectangle(Cx, CurY, client.x2, CurY+CharSize.y);
				}
				
				DeleteArray(Wide);
			}

			if (CursorOff >= 0)
			{
				// Draw cursor
				#if 1
				
					GetLocationOfByte(CursorPos, Cursor, Wide);

					pDC->Colour(Focus() ? LC_TEXT : LC_LOW, 24);
					for (unsigned i=0; i<CursorPos.Length(); i++)
					{
						GRect r = CursorPos[i];
						r.y1 = r.y2;
						if (i == 0)
						{
							// Hex side..
							if (Nibble)
								r.x1 += CharSize.x;
							else
								r.x2 -= CharSize.x;

							if (Pane == HexPane)
								r.y1--;
						}
						else if (Pane == AsciiPane)
						{
							r.y1--;
						}
						
						pDC->Rectangle(&r);
					}
				
				#else // old code

					// Work out the x position on screen for the cursor in the hex section
					int Off = StartOfHex + (CursorOff * 3) + Nibble;
					int Cx1 = l.Pos[Side].x1 + CharSize.x * Off;

					// Work out cursor location in the ASCII view
					Off = StartOfAscii + CursorOff;
					int Cx2 = l.Pos[Side].x1 + CharSize.x * Off;

					pDC->Colour(Focus() ? LC_TEXT : LC_LOW, 24);
					int Cy = CurY + CharSize.y - 1;

					// hex cursor
					GRect CursorHex(Cx1, Cy - (Pane == HexPane ? 1 : 0), Cx1 + CharSize.x, Cy);
					pDC->Rectangle(&CursorHex);

					// ascii cursor
					GRect CursorText(Cx2, Cy - (Pane == AsciiPane ? 1 : 0), Cx2 + CharSize.x, Cy);
					pDC->Rectangle(&CursorText);

					// Update position for scrolling
					int Ox, Oy;
					pDC->GetOrigin(Ox, Oy);

					CursorHex.Offset(-Ox, -Oy);
					CursorHex.y1 -= CharSize.y;

					CursorText.Offset(-Ox, -Oy);
					CursorText.y1 -= CharSize.y;

					CursorPos.New() = CursorHex;
					CursorPos.New() = CursorText;
					
				#endif
			}
		}
	}

	CurrentY += Lines * CharSize.y;
}

bool GHexView::GetLocationOfByte(GArray<GRect> &Loc, int64 Offset, const char16 *LineBuf)
{
	if (Offset < 0)
		return false;

	int64 X = Offset % BytesPerLine;
	int64 Y = Offset / BytesPerLine;
	
	int64 YPos = VScroll ? VScroll->Value() : 0;
	int64 YPx = (Y - YPos) * CharSize.y;
	int64 XHexPx = 0, XAsciiPx = 0;
	
	char16 s[128];
	int HexLen = 11 + 2 + (X * 3);
	int AsciiLen = 11 + 2 + 48 + 2 + X;
	if (!LineBuf)
	{
		LineBuf = s;
		for (unsigned i=0; i<128; i++)
			s[i] = ' ';
	}
	
	{
		GDisplayString ds(Font, LineBuf, HexLen);
		XHexPx = ds.X();
	}
	{
		GDisplayString ds(Font, LineBuf, AsciiLen);
		XAsciiPx = ds.X();
	}
	
	GRect &rcHex = Loc.New();
	rcHex.ZOff((CharSize.x<<1), CharSize.y-1);
	rcHex.Offset(XHexPx, YPx);

	GRect &rcAscii = Loc.New();
	rcAscii.ZOff(CharSize.x, CharSize.y-1);
	rcAscii.Offset(XAsciiPx, YPx);
	
	return true;
}

void GHexView::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();

	CurrentY = r.y1;
	int64 YPos = VScroll ? VScroll->Value() : 0;
	int64 Start = YPos * BytesPerLine;
	int Lines = r.Y() / CharSize.y;
	int64 End = min(Size-1, Start + (Lines * BytesPerLine));
	Font->Transparent(false);
	CursorPos.Length(0);

	#if 0
	pDC->Colour(GColour(255, 0, 255));
	pDC->Rectangle();
	#endif

	if (CmpLayout.Length())
	{
		int DisplayOffset = YPos * CharSize.y;
		pDC->SetOrigin(0, DisplayOffset);
		r.Offset(0, DisplayOffset);
		
		for (int i=0; i<CmpLayout.Length(); i++)
		{
			Layout &l = CmpLayout[i];
			if (!l.Pos[0].Overlap(&r))
				continue;
			PaintLayout(pDC, l, r);
		}
		
		if (CurrentY < r.y2)
		{
			pDC->Colour(LC_WORKSPACE, 24);
			pDC->Rectangle(0, CurrentY, r.x2, r.y2);
		}
	}
	else if (GetData(Start, End-Start))
	{
		#if 1

		Layout Lo;
		Lo.Len[0] = End - Start + 1;
		Lo.Offset[0] = Start;
		Lo.Data[0] = Buf + (Start - BufPos);
		Lo.Pos[0] = r;
		Lo.Same = true;
		
		PaintLayout(pDC, Lo, r);

		if (CurrentY < r.y2)
		{
			pDC->Colour(LC_WORKSPACE, 24);
			pDC->Rectangle(0, CurrentY, r.x2, r.y2);
		}
		/*
		pDC->Colour(GColour(255, 0, 0));
		for (unsigned i=0; i<DbgRect.Length(); i++)
		{
			pDC->Box(&DbgRect[i]);
		}
		*/		
		#else
		
		for (int l=0; l<Lines; l++, CurrentY += CharSize.y)
		{
			char *p = s;
			int64 LineStart = Start + (l * BytesPerLine);
			if (LineStart >= Size) break;
			int Cx1; // Screen x for cursor in the HEX view
			int Cx2; // Screen x for cursor in the ASCII view
			bool IsCursor = ((Cursor >= LineStart) && (Cursor < LineStart + BytesPerLine));

			// Clear the colours for this line
			for (i=0; i<CountOf(Back); i++)
			{
				Fore[i] = LC_TEXT;
				Back[i] = LC_WORKSPACE;
			}

			// Create line of text
			if (IsHex)
			{
				p += sprintf(p, "%02.2x:%08.8X  ", (uint)(LineStart >> 32), (uint)LineStart);
			}
			else
			{
				#ifdef WIN32
				p += sprintf(p, "%11.11I64i  ", LineStart);
				#else
				p += sprintf(p, "%11.11lli  ", LineStart);
				#endif
			}			
			if (IsCursor)
			{
				// Work out the x position on screen for the cursor in the hex section
				int Off = ((Cursor-LineStart)*3) + Nibble;
				GDisplayString ds(Font, s);
				Cx1 = ds.X() + (CharSize.x * Off);
			}

			// Print the hex bytes to the line
			int64 n;
			for (n=LineStart; n<LineStart+BytesPerLine; n++, p+=3)
			{
				if (n < Size)
				{
					sprintf(p, "%02.2X ", Buf[n-BufPos]);

					/* FIXME
					if (CompareMap && CompareMap[n-BufPos])
					{
						Fore[p - s] = Rgb24(255, 0, 0);
						Fore[p - s + 1] = Rgb24(255, 0, 0);
						Fore[p - s + 2] = Rgb24(255, 0, 0);
					}
					*/
				}
				else
				{
					strcat(p, "   ");
				}
			}

			// Separator between hex/ascii
			strcat(s, "  ");
			p += 2;

			if (IsCursor)
			{
				// Work out cursor location in the ASCII view
				int Off = Cursor-LineStart;
				GDisplayString ds(Font, s);
				Cx2 = ds.X() + (CharSize.x * Off);
			}

			// Print the ascii characters to the line
			for (n=LineStart; n<Size && n<LineStart+BytesPerLine; n++)
			{
				uchar c = Buf[n-BufPos];
				/* FIXME
				if (CompareMap && CompareMap[n-BufPos])
				{
					Fore[p - s] = Rgb24(255, 0, 0);
				}
				*/
				*p++ = (c >= ' ' && c < 0x7f) ? c : '.';
			}
			*p++ = 0;

			// Draw text
			GRect Tr(0, CurrentY, r.X()-1, CurrentY+CharSize.y-1);

			Font->Colour(LC_TEXT, LC_WORKSPACE);
			char16 *Wide = (char16*)LgiNewConvertCp(LGI_WideCharset, s, "iso-8859-1");
			if (Wide)
			{
				// Paint the selection into the colour buffers
				int64 Min = Select >= 0 ? min(Select, Cursor) : -1;
				int64 Max = Select >= 0 ? max(Select, Cursor) : -1;
				if (Min < LineStart + BytesPerLine &&
					Max >= LineStart)
				{
					// Part or all of this line is selected
					int64 s = ((Select - LineStart) * 3) + SelectNibble;
					int64 e = ((Cursor - LineStart) * 3) + Nibble;
					if (s > e)
					{
						int64 i = s;
						s = e;
						e = i;
					}
					if (s < 0) s = 0;
					if (e > BytesPerLine * 3 - 2) e = BytesPerLine * 3 - 2;

					for (i=s+HEX_COLUMN; i<=e+HEX_COLUMN; i++)
					{
						Fore[i] = ColourSelectionFore;
						Back[i] = ColourSelectionBack;
					}
					for (i=(s/3)+TEXT_COLUMN; i<=(e/3)+TEXT_COLUMN; i++)
					{
						Fore[i] = ColourSelectionFore;
						Back[i] = ColourSelectionBack;
					}
				}

				// Colour the back of the cursor grey...
				if (Cursor >= LineStart && Cursor < LineStart+BytesPerLine)
				{
					if (Select < 0 && Flash)
					{
						Back[HEX_COLUMN+((Cursor-LineStart)*3)+Nibble] = CursorColourBack;
						Back[TEXT_COLUMN+Cursor-LineStart] = CursorColourBack;
					}
				}

				// Go through the colour buffers, painting in runs of similar colour
				GRect r;
				int Cx = 0;
				int Len = p - s;
				for (i=0; i<Len; )
				{
					int e = i;
					while (e < Len)
					{
						if (Fore[e] != Fore[i] ||
							Back[e] != Back[i])
							break;
						e++;
					}

					int Run = e - i;
					GDisplayString Str(Font, s + i, Run);
					if (e >= Len)
						r.Set(Cx, CurrentY, X()-1, CurrentY+Str.Y()-1);
					else
						r.Set(Cx, CurrentY, Cx+Str.X()-1, CurrentY+Str.Y()-1);
					Font->Colour(Fore[i], Back[i]);
					Str.Draw(pDC, Cx, CurrentY, &r);
					Cx += Str.X();
					i = e;
				}
				
				DeleteArray(Wide);
			}

			// Draw cursor
			if (IsCursor)
			{
				pDC->Colour(Focus() ? LC_TEXT : LC_LOW, 24);
				int Cy = CurrentY+CharSize.y-1;

				// hex cursor
				GRect CursorHex(Cx1, Cy - (Pane == HexPane ? 1 : 0), Cx1+CharSize.x, Cy);
				pDC->Rectangle(&CursorHex);
				CursorPos.New() = CursorHex;

				// ascii cursor
				GRect CursorText(Cx2, Cy - (Pane == AsciiPane ? 1 : 0), Cx2+CharSize.x, Cy);
				pDC->Rectangle(&CursorText);
				CursorPos.New() = CursorText;
			}
		}
		
		r.y1 = CurrentY;
		if (r.Valid())
		{
			pDC->Colour(LC_WORKSPACE, 24);
			pDC->Rectangle(&r);
		}

		#endif
	}
	else
	{
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle();
		
		if (File)
		{
			Font->Colour(LC_TEXT, LC_WORKSPACE);
			GDisplayString ds(Font, "Couldn't read from file.");
			ds.Draw(pDC, 5, 5);
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

bool GHexView::GetCursorFromLoc(int x, int y, int64 &Cur, int &Nib)
{
	uint64 Start = ((uint64)(VScroll ? VScroll->Value() : 0)) * 16;
	GRect c = GetClient();

	int _x1 = (((x - c.x1) / CharSize.x) - HEX_COLUMN);
	int _x2 = (((x - c.x1) / CharSize.x) - TEXT_COLUMN);
	int cx = _x1 / 3;
	int n = _x1 % 3 > 0;
	int cy = (y - c.y1) / CharSize.y;

	if (cx >= 0 && cx < 16)
	{
		Cur = Start + (cy * 16) + cx;
		Nib = n;
		Pane = HexPane;
		return true;
	}
	else if (_x2 >= 0 && _x2 < 16)
	{
		Cur = Start + (cy * 16) + _x2;
		Nib = 0;
		Pane = AsciiPane;
		return true;
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
			int64 Cur;
			int Nib;
			if (GetCursorFromLoc(m.x, m.y, Cur, Nib))
			{
				SetCursor(Cur, Nib, m.Shift());
			}
		}
	}
}

void GHexView::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		int64 Cur;
		int Nib;
		if (GetCursorFromLoc(m.x, m.y, Cur, Nib))
		{
			SetCursor(Cur, Nib, true);
		}
	}
}

void GHexView::OnFocus(bool f)
{
	Invalidate();
}

bool GHexView::OnKey(GKey &k)
{
	int Lines = GetClient().Y() / CharSize.y;

	switch (k.vkey)
	{
		default:
		{
			if (k.IsChar && !IsReadOnly)
			{
				if (k.Down())
				{
					if (Pane == HexPane)
					{
						int c = -1;
						if (k.c16 >= '0' && k.c16 <= '9')		c = k.c16 - '0';
						else if (k.c16 >= 'a' && k.c16 <= 'f')	c = k.c16 - 'a' + 10;
						else if (k.c16 >= 'A' && k.c16 <= 'F')	c = k.c16 - 'A' + 10;

						if (c >= 0 && c < 16)
						{
							uchar *Byte = Buf + (Cursor - BufPos);
							if (Nibble)
							{
								*Byte = (*Byte & 0xf0) | c;
							}
							else
							{
								*Byte = (c << 4) | (*Byte & 0xf);
							}

							App->SetDirty(true);

							if (Nibble == 0)
							{
								SetCursor(Cursor, 1);
							}
							else if (Cursor < Size - 1)
							{
								SetCursor(Cursor+1, 0);
							}
						}
					}
					else if (Pane == AsciiPane)
					{
						uchar *Byte = Buf + (Cursor - BufPos);

						*Byte =  k.c16;

						App->SetDirty(true);
						
						SetCursor(Cursor + 1);
					}
				}

				return true;
			}			
			break;
		}
		case VK_RIGHT:
		{
			if (k.Down())
			{
				if (Pane == HexPane)
				{
					if (Nibble == 0)
					{
						SetCursor(Cursor, 1, k.Shift());
					}
					else if (Cursor < Size - 1)
					{
						SetCursor(Cursor+1, 0, k.Shift());
					}
				}
				else
				{
					SetCursor(Cursor+1, 0);
				}
			}
			return true;
			break;
		}
		case VK_LEFT:
		{
			if (k.Down())
			{
				if (Pane == HexPane)
				{
					if (Nibble == 1)
					{
						SetCursor(Cursor, 0, k.Shift());
					}
					else if (Cursor > 0)
					{
						SetCursor(Cursor-1, 1, k.Shift());
					}
				}
				else
				{
					SetCursor(Cursor-1, 0);
				}
			}
			return true;
			break;
		}
		case VK_UP:
		{
			if (k.Down())
			{
				SetCursor(Cursor-16, Nibble, k.Shift());
			}
			return true;
			break;
		}
		case VK_DOWN:
		{
			if (k.Down())
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
					SetCursor(Cursor+16, Nibble, k.Shift());
				}
			}
			return true;
			break;
		}
		case VK_PAGEUP:
		{
			if (k.Down())
			{
				if (k.Ctrl())
				{
					SetCursor(Cursor - (Lines * 16 * 16), Nibble, k.Shift());
				}
				else
				{
					SetCursor(Cursor - (Lines * 16), Nibble, k.Shift());
				}
			}
			return true;
			break;
		}
		case VK_PAGEDOWN:
		{
			if (k.Down())
			{
				if (k.Ctrl())
				{
					SetCursor(Cursor + (Lines * 16 * 16), Nibble, k.Shift());
				}
				else
				{
					SetCursor(Cursor + (Lines * 16), Nibble, k.Shift());
				}
			}
			return true;
			break;
		}
		case VK_HOME:
		{
			if (k.Down())
			{
				if (k.Ctrl())
				{
					SetCursor(0, 0, k.Shift());
				}
				else
				{
					SetCursor(Cursor - (Cursor%16), 0, k.Shift());
				}
			}
			return true;
			break;
		}
		case VK_END:
		{
			if (k.Down())
			{
				if (k.Ctrl())
				{
					SetCursor(Size-1, 1, k.Shift());
				}
				else
				{
					SetCursor(Cursor - (Cursor%16) + 15, 1, k.Shift());
				}
			}
			return true;
			break;
		}
		case VK_BACKSPACE:
		{
			if (k.Down())
			{
				if (Pane == HexPane)
				{
					if (Nibble == 0)
					{
						SetCursor(Cursor-1, 1);
					}
					else
					{
						SetCursor(Cursor, 0);
					}
				}
				else
				{
					SetCursor(Cursor - 1);
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
					if (Pane == HexPane)
					{
						Pane = AsciiPane;
					}
					else
					{
						Pane = HexPane;
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
		if (_LoadMenu())
		{
			if (_FileMenu)
			{
				// CmdSaveAs.MenuItem = Menu->FindItem(IDM_SAVEAS);
				GMenuItem *i = Menu->FindItem(IDM_SAVEAS);
				DeleteObj(i);

				CmdSave.MenuItem = Menu->FindItem(IDM_SAVE);
				CmdClose.MenuItem = Menu->FindItem(IDM_CLOSE);
				CmdChangeSize.MenuItem = _FileMenu->AppendItem("Change Size", IDM_CHANGE_FILE_SIZE, true, 3);
				_FileMenu->AppendSeparator(4);
				_FileMenu->AppendItem("Compare With File", IDM_COMPARE, true, 5);
				_FileMenu->AppendSeparator(6);
				CmdFind.MenuItem = _FileMenu->AppendItem("Find\tCtrl+F", IDM_SEARCH, false, 7);
				CmdNext.MenuItem = _FileMenu->AppendItem("Next\tF3", IDM_NEXT, false, 8);
			}

			GSubMenu *Tools = Menu->AppendSub("&Tools");
			if (Tools)
			{
				Tools->AppendItem("Copy Hex", IDM_COPY_HEX, true, -1, "Ctrl+C");
				Tools->AppendItem("Copy Text", IDM_COPY_TEXT, true, -1, "Ctrl+Shift+C");
				Tools->AppendItem("Copy As Code", IDM_COPY_CODE, true, -1, "Ctrl+Alt+C");
				Tools->AppendItem("Paste", IDM_PASTE, true, -1, "Ctrl+V");
				Tools->AppendSeparator();

				Tools->AppendItem("Save To File", IDM_SAVE_SELECTION, true);
				Tools->AppendItem("Fill With Random Bytes", IDM_RND_SELECTION, true);
				Tools->AppendItem("Combine Files", IDM_COMBINE_FILES, true);
			}

			GSubMenu *Help = Menu->AppendSub("&Help");
			if (Help)
			{
				Help->AppendItem("&Help", IDM_HELP, true);
				Help->AppendSeparator();
				Help->AppendItem("&About", IDM_ABOUT, true);
			}
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

		Pour();
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
		Pour();
		
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
	
	CmdClose.Enabled(Doc && Doc->HasFile());
	CmdChangeSize.Enabled(Doc && Doc->HasFile());
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

	Pour();
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

	Pour();
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
			_SaveFile(GetCurFile());
			OnDirty(GetDirty());
			break;
		}
		case IDM_EXIT:
		{
			if (Doc)
			{
				Doc->OpenFile(0, false);
			}

			LgiCloseApp();
			break;
		}
		case IDM_CLOSE:
		{
			if (Doc)
			{
				Doc->OpenFile(0, false);
				CmdFind.Enabled(false);
				CmdNext.Enabled(false);
				CmdVisualise.Enabled(false);
				CmdText.Enabled(false);
				OnDirty(GetDirty());
			}
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
					Doc->SaveSelection(s.Name());
				}
			}
			break;
		}
		case IDM_RND_SELECTION:
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
		case IDM_COMPARE:
		{
			if (Doc && Doc->HasFile())
			{
				GFileSelect s;
				s.Parent(this);
				if (s.Open())
				{
					Doc->CompareFile(s.Name());
				}
			}
			break;
		}
		case IDM_CHANGE_FILE_SIZE:
		{
			if (Doc && Doc->HasFile())
			{
				ChangeSizeDlg Dlg(this, Doc->GetFileSize());
				if (Dlg.DoModal())
				{
					Doc->SetFileSize(Dlg.Size);
				}
			}
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

void AppWnd::Pour()
{
	GDocApp<GOptionsFile>::Pour();
}

bool AppWnd::OpenFile(char *FileName, bool ReadOnly)
{
	bool Status = false;
	if (Doc)
	{
		Status = Doc->OpenFile(FileName, ReadOnly);
		CmdFind.Enabled(Status);
		CmdNext.Enabled(Status);
		CmdVisualise.Enabled(Status);
		CmdText.Enabled(Status);
		OnDirty(GetDirty());
	}
	return Status;
}

bool AppWnd::SaveFile(char *FileName)
{
	bool Status = false;
	if (Doc)
	{
		Status = Doc->SaveFile(FileName);
	}
	return Status;
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
