///////////////////////////////////////////////////////////////////////////
//  File:    sh.cpp
//  Version: 1.1.0.4
//  Updated: 19-Jul-1998
//
//  Copyright:  Ferdinand Prantl, portions by Stcherbatchenko Andrei
//  E-mail:     prantl@ff.cuni.cz
//
//  UNIX shell script syntax highlighing definition
//
//  You are free to use or modify this code to the following restrictions:
//  - Acknowledge me somewhere in your about box, simple "Parts of code by.."
//  will be enough. If you can't (or don't want to), contact me personally.
//  - LEAVE THIS HEADER INTACT
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ccrystaltextview.h"
#include "SyntaxColors.h"
#include "string_util.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using CommonKeywords::IsNumeric;

static BOOL IsShKeyword(LPCTSTR pszChars, int nLength)
{
  static LPCTSTR const s_apszShKeywordList[] =
  {
    _T ("alias"),
    _T ("break"),
    _T ("case"),
    _T ("do"),
    _T ("done"),
    _T ("echo"),
    _T ("elif"),
    _T ("else"),
    _T ("esac"),
    _T ("exec"),
    _T ("exit"),
    _T ("export"),
    _T ("fi"),
    _T ("for"),
    _T ("function"),
    _T ("if"),
    _T ("in"),
    _T ("read"),
    _T ("return"),
    _T ("select"),
    _T ("set"),
    _T ("shift"),
    _T ("source"),
    _T ("then"),
    _T ("unalias"),
    _T ("unset"),
    _T ("until"),
    _T ("while"),
  };
  return xiskeyword<_tcsnicmp>(pszChars, nLength, s_apszShKeywordList);
}

static BOOL IsUser1Keyword(LPCTSTR pszChars, int nLength)
{
  static LPCTSTR const s_apszUser1KeywordList[] =
  {
    _T ("chmod"),
    _T ("chown"),
    _T ("logout"),
    _T ("ls"),
    _T ("pwd"),
    _T ("ssh"),
    _T ("telnet"),
    _T ("who"),
  };
  return xiskeyword<_tcsnicmp>(pszChars, nLength, s_apszUser1KeywordList);
}

#define DEFINE_BLOCK(pos, colorindex)   \
ASSERT((pos) >= 0 && (pos) <= nLength);\
if (pBuf != NULL)\
  {\
    if (nActualItems == 0 || pBuf[nActualItems - 1].m_nCharPos <= (pos)){\
        pBuf[nActualItems].m_nCharPos = (pos);\
        pBuf[nActualItems].m_nColorIndex = (colorindex);\
        pBuf[nActualItems].m_nBgColorIndex = COLORINDEX_BKGND;\
        nActualItems ++;}\
  }

#define COOKIE_COMMENT          0x0001
#define COOKIE_PREPROCESSOR     0x0002
#define COOKIE_EXT_COMMENT      0x0004
#define COOKIE_STRING           0x0008
#define COOKIE_CHAR             0x0010

DWORD CCrystalTextView::ParseLineSh(DWORD dwCookie, int nLineIndex, TEXTBLOCK *pBuf, int &nActualItems)
{
  const int nLength = GetLineLength(nLineIndex);
  if (nLength == 0)
    return dwCookie & COOKIE_EXT_COMMENT;

  const LPCTSTR pszChars = GetLineChars(nLineIndex);
  BOOL bFirstChar = (dwCookie & ~COOKIE_EXT_COMMENT) == 0;
  BOOL bRedefineBlock = TRUE;
  BOOL bDecIndex = FALSE;
  int nIdentBegin = -1;
  int nPrevI = -1;
  int I;
  for (I = 0; I <= nLength; nPrevI = I++)
    {
      if (bRedefineBlock)
        {
          int nPos = I;
          if (bDecIndex)
            nPos = nPrevI;
          if (dwCookie & (COOKIE_COMMENT | COOKIE_EXT_COMMENT))
            {
              DEFINE_BLOCK(nPos, COLORINDEX_COMMENT);
            }
          else if (dwCookie & (COOKIE_CHAR | COOKIE_STRING))
            {
              DEFINE_BLOCK(nPos, COLORINDEX_STRING);
            }
          else
            {
              if (xisalnum(pszChars[nPos]) || pszChars[nPos] == '.' && nPos > 0 && (!xisalpha(pszChars[nPos - 1]) && !xisalpha(pszChars[nPos + 1])))
                {
                  DEFINE_BLOCK(nPos, COLORINDEX_NORMALTEXT);
                }
              else
                {
                  DEFINE_BLOCK(nPos, COLORINDEX_OPERATOR);
                  bRedefineBlock = TRUE;
                  bDecIndex = TRUE;
                  goto out;
                }
            }
          bRedefineBlock = FALSE;
          bDecIndex = FALSE;
        }
out:

      // Can be bigger than length if there is binary data
      // See bug #1474782 Crash when comparing SQL with with binary data
      if (I >= nLength)
        break;

      if (dwCookie & COOKIE_COMMENT)
        {
          DEFINE_BLOCK(I, COLORINDEX_COMMENT);
          dwCookie |= COOKIE_COMMENT;
          break;
        }

      //  String constant "...."
      if (dwCookie & COOKIE_STRING)
        {
          if (pszChars[I] == '"' && (I == 0 || I == 1 && pszChars[nPrevI] != '\\' || I >= 2 && (pszChars[nPrevI] != '\\' || pszChars[nPrevI] == '\\' && pszChars[nPrevI - 1] == '\\')))
            {
              dwCookie &= ~COOKIE_STRING;
              bRedefineBlock = TRUE;
            }
          continue;
        }

      //  Char constant '..'
      if (dwCookie & COOKIE_CHAR)
        {
          if (pszChars[I] == '\'' && (I == 0 || I == 1 && pszChars[nPrevI] != '\\' || I >= 2 && (pszChars[nPrevI] != '\\' || pszChars[nPrevI] == '\\' && pszChars[nPrevI - 1] == '\\')))
            {
              dwCookie &= ~COOKIE_CHAR;
              bRedefineBlock = TRUE;
            }
          continue;
        }

      if (pszChars[I] == '#')
        {
          DEFINE_BLOCK(I, COLORINDEX_COMMENT);
          dwCookie |= COOKIE_COMMENT;
          break;
        }

      //  Normal text
      if (pszChars[I] == '"')
        {
          DEFINE_BLOCK(I, COLORINDEX_STRING);
          dwCookie |= COOKIE_STRING;
          continue;
        }
      if (pszChars[I] == '\'')
        {
          // if (I + 1 < nLength && pszChars[I + 1] == '\'' || I + 2 < nLength && pszChars[I + 1] != '\\' && pszChars[I + 2] == '\'' || I + 3 < nLength && pszChars[I + 1] == '\\' && pszChars[I + 3] == '\'')
          if (!I || !xisalnum(pszChars[nPrevI]))
            {
              DEFINE_BLOCK(I, COLORINDEX_STRING);
              dwCookie |= COOKIE_CHAR;
              continue;
            }
        }

      if (bFirstChar)
        {
          if (!xisspace(pszChars[I]))
            bFirstChar = FALSE;
        }

      if (pBuf == NULL)
        continue;               //  We don't need to extract keywords,
      //  for faster parsing skip the rest of loop

      if (xisalnum(pszChars[I]) || pszChars[I] == '.' && I > 0 && (!xisalpha(pszChars[nPrevI]) && !xisalpha(pszChars[I + 1])))
        {
          if (nIdentBegin == -1)
            nIdentBegin = I;
        }
      else
        {
          if (nIdentBegin >= 0)
            {
              if (IsShKeyword(pszChars + nIdentBegin, I - nIdentBegin))
                {
                  DEFINE_BLOCK(nIdentBegin, COLORINDEX_KEYWORD);
                }
              else if (IsUser1Keyword(pszChars + nIdentBegin, I - nIdentBegin))
                {
                  DEFINE_BLOCK(nIdentBegin, COLORINDEX_USER1);
                }
              else if (IsNumeric(pszChars + nIdentBegin, I - nIdentBegin))
                {
                  DEFINE_BLOCK(nIdentBegin, COLORINDEX_NUMBER);
                }
              else
                {
                  bool bFunction = FALSE;

                  for (int j = I; j < nLength; j++)
                    {
                      if (!xisspace(pszChars[j]))
                        {
                          if (pszChars[j] == '(')
                            {
                              bFunction = TRUE;
                            }
                          break;
                        }
                    }
                  if (bFunction)
                    {
                      DEFINE_BLOCK(nIdentBegin, COLORINDEX_FUNCNAME);
                    }
                }
              bRedefineBlock = TRUE;
              bDecIndex = TRUE;
              nIdentBegin = -1;
            }
        }
    }

  if (nIdentBegin >= 0)
    {
      if (IsShKeyword(pszChars + nIdentBegin, I - nIdentBegin))
        {
          DEFINE_BLOCK(nIdentBegin, COLORINDEX_KEYWORD);
        }
      else if (IsUser1Keyword(pszChars + nIdentBegin, I - nIdentBegin))
        {
          DEFINE_BLOCK(nIdentBegin, COLORINDEX_USER1);
        }
      else if (IsNumeric(pszChars + nIdentBegin, I - nIdentBegin))
        {
          DEFINE_BLOCK(nIdentBegin, COLORINDEX_NUMBER);
        }
      else
        {
          bool bFunction = FALSE;

          for (int j = I; j < nLength; j++)
            {
              if (!xisspace(pszChars[j]))
                {
                  if (pszChars[j] == '(')
                    {
                      bFunction = TRUE;
                    }
                  break;
                }
            }
          if (bFunction)
            {
              DEFINE_BLOCK(nIdentBegin, COLORINDEX_FUNCNAME);
            }
        }
    }

  if (pszChars[nLength - 1] != '\\')
    dwCookie &= COOKIE_EXT_COMMENT;
  return dwCookie;
}
