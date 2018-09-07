#include "Lgi.h"
#include "iHex.h"
#include "resdefs.h"

SearchDlg::SearchDlg(AppWnd *app)
{
	SetParent(App = app);
	ForHex = false;
	MatchWord = false;
	MatchCase = false;
	SearchUp = false;
	Bin = 0;
	Length = 0;

	if (LoadFromResource(IDD_SEARCH))
	{
		SetCtrlValue(IDC_SEARCH_TXT, 1);
		MoveToCenter();
	}
}

SearchDlg::~SearchDlg()
{
	DeleteArray(Bin);
}

void SearchDlg::OnCreate()
{
	GViewI *v = FindControl(IDC_TEXT);
	if (v) v->Focus(true);
}

int SearchDlg::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDC_TEXT:
		{
			SetCtrlValue(IDC_SEARCH_TXT, 1);
			break;
		}
		case IDC_HEX:
		{
			SetCtrlValue(IDC_SEARCH_HEX, 1);
			break;
		}
		case IDOK:
		{
			ForHex = GetCtrlValue(IDC_SEARCH_HEX) != 0;
			MatchWord = GetCtrlValue(IDC_MATCH_WORD);
			MatchCase = GetCtrlValue(IDC_MATCH_CASE);
			SearchUp = GetCtrlValue(IDC_SEARCH_UP);

			char *Str = GetCtrlName(ForHex ? IDC_HEX : IDC_TEXT);
			if (Str)
			{
				if (ForHex)
				{
					GStringPipe p;
					char h[3] = {0, 0, 0};
					int i = 0;

					for (char *s=Str; *s; s++)
					{
						if
						(
							(*s >= '0' && *s <= '9')
							||
							(*s >= 'a' && *s <= 'f')
							||
							(*s >= 'A' && *s <= 'F')
						)
						{
							h[i++] = *s;
						}

						if (i == 2)
						{
							char c = htoi(h);
							i = 0;
							p.Push(&c, 1);
						}
					}

					Length = p.GetSize();
					Bin = (uchar*)p.NewStr();
				}
				else
				{
					Length = strlen(Str);
					Bin = (uchar*)NewStr(Str);
				}
			}
		}
		case IDCANCEL:
		{
			EndModal(c->GetId());
			break;
		}
	}

	return 0;
}
