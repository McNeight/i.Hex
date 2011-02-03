/*hdr
**      FILE:           iHex.h
**      AUTHOR:         Matthew Allen
**      DESCRIPTION:    Main header
**
**      Copyright (C) 2005, Matthew Allen
**              fret@memecode.com
*/

#ifndef __IDISK_H
#define __IDISK_H

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "Lgi.h"
#include "GAbout.h"
#include "GDocApp.h"
#include "GOptionsFile.h"
#include "GScripting.h"

/////////////////////////////////////////////////////////////////////////////
#define APP_VER						0.97

/////////////////////////////////////////////////////////////////////////////
#define IDM_START					100
#define IDM_REWIND					101
#define IDM_PLAY					102
#define IDM_PAUSE					103
#define IDM_STOP					104
#define IDM_FORWARD					105
#define IDM_END						106
#define IDM_CHANGE_FILE_SIZE		107
#define IDM_SEARCH					108
#define IDM_NEXT					109
#define IDM_SAVE_SELECTION			110
#define IDM_VISUALISE				111
#define IDM_NEW						112
#define IDM_DELETE					113
#define IDM_COMPILE					114
#define IDM_TEXTVIEW				115
#define IDM_LOCK					116
#define IDM_COMPARE					117
#define IDM_RND_SELECTION			118
#define IDM_COMBINE_FILES			119
#define IDM_COPY					120
#define IDM_PASTE					121

#define IDM_ABOUT					901
#define IDM_HELP					902

#define IDC_HEX_VIEW				2000
#define IDC_LIST					2001

#define MAX_SIZES					8

#define C_WHITE						Rgb24(255, 255, 255)
#ifdef WIN32
#define C_HIGHLIGHT					GetSysColor(COLOR_HIGHLIGHT)
#define C_TEXT						GetSysColor(COLOR_BTNTEXT)
#define C_WND_TEXT					GetSysColor(COLOR_WINDOWTEXT)
#else
#define C_HIGHLIGHT					Rgb24(0xC0, 0xC0, 0xC0)
#define C_TEXT						Rgb24(0, 0, 0)
#define C_WND_TEXT					Rgb24(0, 0, 0)
#endif

/////////////////////////////////////////////////////////////////////////////
extern char *AppName;
extern char16 *LexCpp(char16 *&s, bool ReturnString = true);

/////////////////////////////////////////////////////////////////////////////
class AppWnd : public GDocApp<GOptionsFile>, public GScriptContext
{
	// state
	bool Active;

	// views
	GToolBar *Tools;
	class GHexView *Doc;
	class IHexBar *Bar;
	class GVisualiseView *Visual;
	class GTextView3 *TextView;
	GCommand CmdSave;
	GCommand CmdSaveAs;
	GCommand CmdClose;
	GCommand CmdChangeSize;
	GCommand CmdFind;
	GCommand CmdNext;
	GCommand CmdVisualise;
	GCommand CmdText;
	GSplitter *Split;

	class SearchDlg *Search;

	GStatusBar *Status;
	GStatusPane *StatusInfo[3];

	void ToggleVisualise();
	void ToggleTextView();

	GHostFunc *GetCommands() { return 0; }
	void SetEngine(GScriptEngine *Eng) {}
	char *GetIncludeFile(char *FileName) { return 0; }

public:
	AppWnd();
	~AppWnd();

	void SetStatus(int Pos, char *Text);

	void Pour();
	bool OpenFile(char *FileName, bool ReadOnly);
	bool SaveFile(char *FileName);

	bool OnKey(GKey &k);
	void OnPosChange();
	int OnEvent(GMessage *Msg);
	void OnPaint(GSurface *pDC);
	int OnCommand(int Cmd, int Event, OsView Wnd);
	void OnPulse();
	int OnNotify(GViewI *Ctrl, int Flags);
	bool OnRequestClose(bool OsShuttingDown);
	void OnDirty(bool NewValue);
	void Help(char *File);
};

class SearchDlg : public GDialog
{
	AppWnd *App;

public:
	bool ForHex;
	bool MatchWord;
	bool MatchCase;
	
	uchar *Bin;
	int Length;

	SearchDlg(AppWnd *app);
	~SearchDlg();

	int OnNotify(GViewI *c, int f);
	void OnCreate();
};

#include "GTextView3.h"
class GVisualiseView : public GSplitter
{
	AppWnd *App;
	class GMapWnd *Map;
	GTextView3 *Txt;
	char Base[300];

public:
	GVisualiseView(AppWnd *app);
	int OnNotify(GViewI *c, int f);
	void Visualise(char *Data, int Len, bool Little);
};

#endif
