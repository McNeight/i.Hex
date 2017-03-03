#ifndef _HEX_VIEW_H_
#define _HEX_VIEW_H_

///////////////////////////////////////////////////////////////////////////////////////////////
enum PaneType
{
	HexPane,
	AsciiPane
};

enum FormatType
{
	FmtText,
	FmtHex,
	FmtCode
};

class IHexBar;
class GHexView;

class GHexBuffer
{
	GHexView *View;

public:
	// File
	GFile *File;
	int64 Size;
	int Used;
	bool IsReadOnly;	// Data is read only

	// Buffer
	uchar *Buf;			// Buffer for data from the file
	int BufLen;			// Length of the data buffer
	int BufUsed;		// Length of the buffer used
	int64 BufPos;		// Where the start of the buffer came from in the file

	// Position
	GRect Pos;

	GHexBuffer(GHexView *view)
	{
		View = view;

		File = NULL;
		Buf = NULL;
		BufLen = 0;
		BufUsed = 0;
		BufPos = 0;
		Size = 0;
		Used = 0;
		IsReadOnly = false;
	}

	~GHexBuffer()
	{
		Empty();
	}

	int64 SetSize(int64 sz)
	{
		if (File)
			Size = File->SetSize(sz);
		else
			Size = sz;

		return Size;
	}

	void Empty()
	{
		DeleteObj(File);
		DeleteArray(Buf);
		BufLen = 0;
		BufUsed = 0;
		BufPos = 0;
		Used = 0;
		Size = 0;
	}

	bool HasData()
	{
		return File != 0 ||
			(Buf != 0 &&  BufUsed > 0);
	}

	bool GetData(int64 Start, int Len);
	void OnPaint(GSurface *pDC, int64 Start, int64 Len);
};

struct GHexCursor
{
	GHexBuffer *Buf;
	int64 Index;		// Offset into the file that the cursor is over
	int Nibble;			// 0 or 1, defining the nibble pointed to
						// 0: 0xc0, 1: 0x0c etc
	PaneType Pane;		// 0 = hex, 1 = ascii
	bool Flash;

	GArray<GRect> Pos;

	GHexCursor()
	{
		Empty();
	}

	GHexCursor &operator =(const GHexCursor &c)
	{
		Buf = c.Buf;
		Index = c.Index;
		Nibble = c.Nibble;

		return *this;
	}

	void Empty()
	{
		Buf = NULL;
		Index = -1;
		Nibble = 0;
		Pane = HexPane;
		Flash = true;
		Pos.Length(0);
	}
};

class GHexView : public GLayout
{
	friend class GHexBuffer;

	AppWnd *App;
	IHexBar *Bar;
	GFont *Font;
	GArray<GRect> DbgRect;
	
	// General display parameters
		bool IsHex; // Display offsets in hex
		GdcPt2 CharSize; // Size of character in pixels
		int BytesPerLine; // Number of bytes to display on each line
		int IntWidth; // Number of bytes to display in one contiguous number

	// Data buffers
	GArray<GHexBuffer*> Buf;

	// Comparison file
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
	
	// void PaintLayout(GSurface *pDC, Layout &l, GRect &client);

	// Cursors
	GHexCursor Cursor, Selection;

	void SwapBytes(void *p, int Len);

public:
	GHexView(AppWnd *app, IHexBar *bar);
	~GHexView();

	bool CreateFile(int64 Len);
	bool OpenFile(char *FileName, bool ReadOnly);
	bool SaveFile(GHexBuffer *b, char *FileName);
	bool HasFile();
	bool Empty();
	void SaveSelection(GHexBuffer *b, char *File);
	void SelectionFillRandom(GStream *Rnd);
	void SelectAll();
	void CompareFile(char *File);

	void Copy(FormatType Fmt);
	void Paste();

	bool HasSelection();
	int GetSelectedNibbles();
	void UpdateScrollBar();
	GHexBuffer *GetCursorBuffer();
	void SetCursor(GHexBuffer *b, int64 cursor, int nibble = 0, bool select = false);
	void SetIsHex(bool i);
	int64 GetFileSize();
	bool SetFileSize(int64 Size);
	void DoInfo();
	int64 Search(SearchDlg *For, uchar *Bytes, int Len);
	void DoSearch(SearchDlg *For);
	bool GetCursorFromLoc(int x, int y, GHexCursor &c);
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

#endif