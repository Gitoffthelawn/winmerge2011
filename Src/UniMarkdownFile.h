/**
 *  @file UniMarkdownFile.h
 *
 *  @brief Declaration of UniMarkdownFile class.
 */
// ID line follows -- this is updated by SVN
// $Id$

#include "Common/UniFile.h"

class CMarkdown;

/**
 * @brief XML file reader class.
 */
class UniMarkdownFile : public UniMemFile
{
public:
	UniMarkdownFile();
	static UniFile *CreateInstance(PackingInfo *packingInfo)
	{
		return new UniMarkdownFile;
	}
	virtual bool OpenReadOnly(LPCTSTR filename);
	virtual void Close();
	virtual bool ReadString(String &line, String &eol, bool *lossy);
private:
	void Move();
	String maketstring(LPCSTR lpd, UINT len);
	int m_depth;
	bool m_bMove;
	LPBYTE m_transparent;
	CMarkdown *m_pMarkdown;
};
