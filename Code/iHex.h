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
#define APP_VER						"1.0"

/////////////////////////////////////////////////////////////////////////////
enum Commands
{
	IDM_START = 100,
	IDM_REWIND,
	IDM_PLAY,
	IDM_PAUSE,
	IDM_STOP,
	IDM_FORWARD,
	IDM_END,
	IDM_VISUALISE,
	IDM_NEW,
	IDM_DELETE,
	IDM_COMPILE,
	IDM_TEXTVIEW,
	IDM_LOCK,
};

enum Controls
{
	IDC_HEX_VIEW = 1000,
	IDC_LIST,
};

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
extern const char *AppName;
// extern char16 *LexCpp(char16 *&s, bool ReturnString = true);

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

	void PourAll();
	bool OpenFile(char *FileName, bool ReadOnly);
	bool SaveFile(char *FileName);
	void OnDocument(bool Valid);

	void Empty();
	bool OnKey(GKey &k);
	void OnPosChange();
	GMessage::Result OnEvent(GMessage *Msg);
	void OnPaint(GSurface *pDC);
	int OnCommand(int Cmd, int Event, OsView Wnd);
	void OnPulse();
	int OnNotify(GViewI *Ctrl, int Flags);
	bool OnRequestClose(bool OsShuttingDown);
	void OnDirty(bool NewValue);
	void Help(const char *File);
	void OnReceiveFiles(GArray<char*> &Files);
};

class SearchDlg : public GDialog
{
	AppWnd *App;

public:
	bool ForHex;
	bool MatchWord;
	bool MatchCase;
	bool SearchUp;
	
	uchar *Bin;
	int64 Length;

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
	char Base[MAX_PATH];

public:
	GVisualiseView(AppWnd *app, char *DefVisual = NULL);
	int OnNotify(GViewI *c, int f);
	void Visualise(char *Data, int Len, bool Little);
};

#endif
