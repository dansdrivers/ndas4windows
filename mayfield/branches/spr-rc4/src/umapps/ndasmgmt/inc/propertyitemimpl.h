#ifndef __PROPERTYITEMIMPL__H
#define __PROPERTYITEMIMPL__H

#pragma once

/////////////////////////////////////////////////////////////////////////////
// CPropertyItemImpl - Property implementations for the Property controls
//
// Written by Bjarke Viksoe (bjarke@viksoe.dk)
// Copyright (c) 2001-2002 Bjarke Viksoe.
//
// This code may be used in compiled form in any way you desire. This
// file may be redistributed by any means PROVIDING it is 
// not sold for profit without the authors written consent, and 
// providing that this notice and the authors name is included. 
//
// This file is provided "as is" with no expressed or implied warranty.
// The author accepts no liability if it causes any damage to you or your
// computer whatsoever. It's free, so don't hassle me about it.
//
// Beware of bugs.
//

#ifndef __PROPERTYITEM__H
  #error PropertyItemImpl.h requires PropertyItem.h to be included first
#endif

#ifndef __PROPERTYITEMEDITORS__H
  #error PropertyItemImpl.h requires PropertyItemEditors.h to be included first
#endif

#ifndef __ATLBASE_H__
  #error PropertyItem.h requires atlbase.h to be included first
#endif



/////////////////////////////////////////////////////////////////////////////
// Base CProperty class

class CProperty : public IProperty
{
protected:
   HWND   m_hWndOwner;
   LPTSTR m_pszName;
   bool   m_fEnabled;
   LPARAM m_lParam;

public:
   CProperty(LPCTSTR pstrName, LPARAM lParam) : m_fEnabled(true), m_lParam(lParam), m_hWndOwner(NULL)
   {
      ATLASSERT(!::IsBadStringPtr(pstrName,-1));
      ATLTRY(m_pszName = new TCHAR[ (::lstrlen(pstrName) * sizeof(TCHAR)) + 1 ]);
      ATLASSERT(m_pszName);
      ::lstrcpy(m_pszName, pstrName);
   }
   virtual ~CProperty()
   {
      delete [] m_pszName;
   }
   virtual void SetOwner(HWND hWnd, LPVOID /*pData*/)
   {
      ATLASSERT(::IsWindow(hWnd));
      ATLASSERT(m_hWndOwner==NULL); // Cannot set it twice
      m_hWndOwner = hWnd;
   }
   virtual LPCTSTR GetName() const
   {
      return m_pszName; // Dangerous!
   }
   virtual void SetEnabled(BOOL bEnable)
   {
      m_fEnabled = bEnable == TRUE;
   }
   virtual BOOL IsEnabled() const
   {
      return m_fEnabled;
   }
   virtual void SetItemData(LPARAM lParam)
   {
      m_lParam = lParam;
   }
   virtual LPARAM GetItemData() const
   {
      return m_lParam;
   }
   virtual void DrawName(PROPERTYDRAWINFO& di)
   {
      CDCHandle dc(di.hDC);
      COLORREF clrBack, clrFront;
      if( di.state & ODS_DISABLED ) {
         clrFront = di.clrDisabled;
         clrBack = di.clrBack;
      }
      else if( di.state & ODS_SELECTED ) {
         clrFront = di.clrSelText;
         clrBack = di.clrSelBack;
      }
      else {
         clrFront = di.clrText;
         clrBack = di.clrBack;
      }
      RECT rcItem = di.rcItem;
      dc.FillSolidRect(&rcItem, clrBack);
      rcItem.left += 2; // Indent text
      dc.SetBkMode(TRANSPARENT);
      dc.SetBkColor(clrBack);
      dc.SetTextColor(clrFront);
      dc.DrawText(m_pszName, -1, &rcItem, DT_LEFT | DT_SINGLELINE | DT_EDITCONTROL | DT_NOPREFIX | DT_VCENTER);
   }
   virtual void DrawValue(PROPERTYDRAWINFO& /*di*/) 
   { 
   }
   virtual HWND CreateInplaceControl(HWND /*hWnd*/, const RECT& /*rc*/) 
   { 
      return NULL; 
   }
   virtual BOOL Activate(UINT /*action*/, LPARAM /*lParam*/) 
   { 
      return TRUE; 
   }
   virtual BOOL GetDisplayValue(LPTSTR /*pstr*/, UINT /*cchMax*/) const 
   { 
      return FALSE; 
   }
   virtual UINT GetDisplayValueLength() const 
   { 
      return 0; 
   }
   virtual BOOL GetValue(VARIANT* /*pValue*/) const 
   { 
      return FALSE; 
   }
   virtual BOOL SetValue(const VARIANT& /*value*/) 
   { 
      ATLASSERT(false);
      return FALSE; 
   }
   virtual BOOL SetValue(HWND /*hWnd*/) 
   { 
      ATLASSERT(false);
      return FALSE; 
   }
};


/////////////////////////////////////////////////////////////////////////////
// Simple property (displays text)

class CPropertyItem : public CProperty
{
protected:
   CComVariant m_val;

public:
   CPropertyItem(LPCTSTR pstrName, LPARAM lParam) : CProperty(pstrName, lParam)
   {
   }
   BYTE GetKind() const 
   { 
      return PROPKIND_SIMPLE; 
   }
   void DrawValue(PROPERTYDRAWINFO& di)
   {
      UINT cchMax = GetDisplayValueLength() + 1;
      LPTSTR pszText = (LPTSTR) _alloca(cchMax * sizeof(TCHAR));
      ATLASSERT(pszText);
      if( !GetDisplayValue(pszText, cchMax) ) return;
      CDCHandle dc(di.hDC);
      dc.SetBkMode(TRANSPARENT);
      dc.SetTextColor(di.state & ODS_DISABLED ? di.clrDisabled : di.clrText);
      dc.SetBkColor(di.clrBack);
      RECT rcText = di.rcItem;
      rcText.left += PROP_TEXT_INDENT;
      dc.DrawText(pszText, -1, 
         &rcText, 
         DT_LEFT | DT_SINGLELINE | DT_EDITCONTROL | DT_NOPREFIX | DT_END_ELLIPSIS | DT_VCENTER);
   }
   BOOL GetDisplayValue(LPTSTR pstr, UINT cchMax) const
   {      
      ATLASSERT(!::IsBadStringPtr(pstr, cchMax));
      // Convert VARIANT to string and use that as display string...
      CComVariant v;
      if( FAILED( v.ChangeType(VT_BSTR, &m_val) ) ) return FALSE;
      USES_CONVERSION;
      ::lstrcpyn(pstr, OLE2CT(v.bstrVal), cchMax);
      return TRUE;
   }
   UINT GetDisplayValueLength() const
   {
      // Hmm, need to convert it to display string first and
      // then take the length...
      // TODO: Call GetDisplayValue() instead...
      CComVariant v;
      if( FAILED( v.ChangeType(VT_BSTR, &m_val) ) ) return 0;
      return v.bstrVal == NULL ? 0 : ::lstrlenW(v.bstrVal);
   }
   BOOL GetValue(VARIANT* pVal) const
   {
      return SUCCEEDED( CComVariant(m_val).Detach(pVal) );
   }
   BOOL SetValue(const VARIANT& value)
   {
      m_val = value;
      return TRUE;
   }
};


/////////////////////////////////////////////////////////////////////////////
// ReadOnly property (enhanced display feature)

class CPropertyReadOnlyItem : public CPropertyItem
{
protected:
   UINT m_uStyle;
   HICON m_hIcon;
   COLORREF m_clrBack;
   COLORREF m_clrText;

public:
   CPropertyReadOnlyItem(LPCTSTR pstrName, LPARAM lParam) : 
      CPropertyItem(pstrName, lParam), 
      m_uStyle( DT_LEFT | DT_SINGLELINE | DT_EDITCONTROL | DT_NOPREFIX | DT_END_ELLIPSIS | DT_VCENTER ),
      m_clrBack( (COLORREF) -1 ),
      m_clrText( (COLORREF) -1 ),
      m_hIcon(NULL)
   {
   }
   void DrawValue(PROPERTYDRAWINFO& di)
   {
      // Get property text
      UINT cchMax = GetDisplayValueLength() + 1;
      LPTSTR pszText = (LPTSTR) _alloca(cchMax * sizeof(TCHAR));
      ATLASSERT(pszText);
      if( !GetDisplayValue(pszText, cchMax) ) return;
      // Prepare paint
      RECT rcText = di.rcItem;
      CDCHandle dc(di.hDC);
      dc.SetBkMode(OPAQUE);
      // Set background color
      COLORREF clrBack = di.clrBack;
      if( m_clrBack != (COLORREF) -1 ) clrBack = m_clrBack;
      dc.SetBkColor(clrBack);
      // Set text color
      COLORREF clrText = di.clrText;
      if( m_clrText != (COLORREF) -1 ) clrText = m_clrText;
      if( di.state & ODS_DISABLED ) clrText = di.clrDisabled; 
      dc.SetTextColor(clrText);
      // Draw icon if available
      if( m_hIcon ) {
         POINT pt = { rcText.left + 2, rcText.top + 2 };
         SIZE sz = { ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON) };
         ::DrawIconEx(dc, pt.x, pt.y, m_hIcon, sz.cx, sz.cy, 0, NULL, DI_NORMAL);
         rcText.left += sz.cx + 4;
      }
      // Draw text with custom style
      rcText.left += PROP_TEXT_INDENT;
      dc.DrawText(pszText, -1, 
         &rcText, 
         m_uStyle);
   }

   // Operations

   // NOTE: To use these methods, you must cast the HPROPERTY 
   //       handle back to the CPropertyReadOnlyItem class.
   //       Nasty stuff, but so far I've settled with this approach.

   COLORREF SetBkColor(COLORREF clrBack)
   {
      COLORREF clrOld = m_clrBack;
      m_clrBack = clrBack;
      return clrOld;
   }
   COLORREF SetTextColor(COLORREF clrText)
   {
      COLORREF clrOld = m_clrText;
      m_clrText = clrText;
      return clrOld;
   }
   HICON SetIcon(HICON hIcon)
   {
      HICON hOldIcon = m_hIcon;
      m_hIcon = hIcon;
      return hOldIcon;
   }
   void ModifyDrawStyle(UINT uRemove, UINT uAdd)
   {
      m_uStyle = (m_uStyle & ~uRemove) | uAdd;
   }
};


/////////////////////////////////////////////////////////////////////////////
// Simple Value property

class CPropertyEditItem : public CPropertyItem
{
protected:
   HWND m_hwndEdit;

public:
   CPropertyEditItem(LPCTSTR pstrName, LPARAM lParam) : 
      CPropertyItem(pstrName, lParam), 
      m_hwndEdit(NULL)
   {
   }
   BYTE GetKind() const 
   { 
      return PROPKIND_EDIT; 
   }
   HWND CreateInplaceControl(HWND hWnd, const RECT& rc) 
   {
      // Get default text
      UINT cchMax = GetDisplayValueLength() + 1;
      LPTSTR pszText = (LPTSTR) _alloca(cchMax * sizeof(TCHAR));
      ATLASSERT(pszText);
      if( !GetDisplayValue(pszText, cchMax) ) return NULL;
      // Create EDIT control
      CPropertyEditWindow* win = new CPropertyEditWindow();
      ATLASSERT(win);
      RECT rcWin = rc;
      m_hwndEdit = win->Create(hWnd, rcWin, pszText, WS_VISIBLE | WS_CHILD | ES_LEFT | ES_AUTOHSCROLL);
      ATLASSERT(::IsWindow(m_hwndEdit));
      // Simple hack to validate numbers
      switch( m_val.vt ) {
      case VT_UI1:
      case VT_UI2:
      case VT_UI4:
         win->ModifyStyle(0, ES_NUMBER);
      }
      return m_hwndEdit;
   }
   BOOL SetValue(const VARIANT& value)
   {
      if( m_val.vt == VT_EMPTY ) m_val = value;
      return SUCCEEDED( m_val.ChangeType(m_val.vt, &value) );
   }
   BOOL SetValue(HWND hWnd) 
   { 
      ATLASSERT(::IsWindow(hWnd));
      int len = ::GetWindowTextLength(hWnd) + 1;
      LPTSTR pstr = (LPTSTR) _alloca(len * sizeof(TCHAR));
      ATLASSERT(pstr);
      if( ::GetWindowText(hWnd, pstr, len) == 0 ) {
         // Bah, an empty string AND an error causes the same return code!
         if( ::GetLastError() != 0 ) return FALSE;
      }
      CComVariant v = pstr;
      return SetValue(v);
   }
   BOOL Activate(UINT action, LPARAM /*lParam*/)
   {
      switch( action ) {
      case PACT_TAB:
      case PACT_SPACE:
      case PACT_DBLCLICK:
         if( ::IsWindow(m_hwndEdit) ) {
            ::SetFocus(m_hwndEdit);
            ::SendMessage(m_hwndEdit, EM_SETSEL, 0, -1);
         }
         break;
      }
      return TRUE;
   }
};


/////////////////////////////////////////////////////////////////////////////
// Simple Value property

class CPropertyDateItem : public CPropertyEditItem
{
public:
   CPropertyDateItem(LPCTSTR pstrName, LPARAM lParam) : 
      CPropertyEditItem(pstrName, lParam)
   {
   }
   HWND CreateInplaceControl(HWND hWnd, const RECT& rc) 
   {
      // Get default text
      UINT cchMax = GetDisplayValueLength() + 1;
      LPTSTR pszText = (LPTSTR) _alloca(cchMax * sizeof(TCHAR));
      ATLASSERT(pszText);
      if( !GetDisplayValue(pszText, cchMax) ) return NULL;
      // Create window
      CPropertyDateWindow* win = new CPropertyDateWindow();
      ATLASSERT(win);
      RECT rcWin = rc;
      m_hwndEdit = win->Create(hWnd, rcWin, pszText);
      ATLASSERT(win->IsWindow());
      return *win;
   }
   BOOL GetDisplayValue(LPTSTR pstr, UINT cchMax) const
   {
      if( m_val.date == 0.0 ) {
         ::lstrcpy(pstr, _T(""));
         return TRUE;
      }
      return CPropertyEditItem::GetDisplayValue(pstr, cchMax);
   }
   BOOL SetValue(const VARIANT& value)
   {
      if( value.vt == VT_BSTR && ::SysStringLen(value.bstrVal) == 0 ) {
         m_val.date = 0.0;
         return TRUE;
      }
      return CPropertyEditItem::SetValue(value);
   }
   BOOL SetValue(HWND hWnd)
   {
      if( ::GetWindowTextLength(hWnd) == 0 ) {
         m_val.date = 0.0;
         return TRUE;
      }
      return CPropertyEditItem::SetValue(hWnd);
   }
};


/////////////////////////////////////////////////////////////////////////////
// Checkmark button

class CPropertyCheckButtonItem : public CProperty
{
protected:
   bool m_bValue;

public:
   CPropertyCheckButtonItem(LPCTSTR pstrName, bool bValue, LPARAM lParam) : 
      CProperty(pstrName, lParam),
      m_bValue(bValue)
   {
   }
   BYTE GetKind() const 
   { 
      return PROPKIND_CHECK; 
   }
   BOOL GetValue(VARIANT* pVal) const
   {
      return SUCCEEDED( CComVariant(m_bValue).Detach(pVal) );
   }
   BOOL SetValue(const VARIANT& value)
   {
      // Set a new value
      switch( value.vt ) {
      case VT_BOOL:
         m_bValue = value.boolVal != VARIANT_FALSE;
         return TRUE;
      default:
         ATLASSERT(false);
         return FALSE;
      }
   }
   void DrawValue(PROPERTYDRAWINFO& di)
   {
      int cxThumb = ::GetSystemMetrics(SM_CXMENUCHECK);
      int cyThumb = ::GetSystemMetrics(SM_CYMENUCHECK);

      RECT rcMark = di.rcItem;
      rcMark.left += 10;
      rcMark.right = rcMark.left + cxThumb;
      rcMark.top += 2;
      if( rcMark.top + cyThumb >= rcMark.bottom ) rcMark.top -= rcMark.top + cyThumb - rcMark.bottom + 1;
      rcMark.bottom = rcMark.top + cyThumb;

      UINT uState = DFCS_BUTTONCHECK | DFCS_FLAT;
      if( m_bValue ) uState |= DFCS_CHECKED;
      if( di.state & ODS_DISABLED ) uState |= DFCS_INACTIVE;
      ::DrawFrameControl(di.hDC, &rcMark, DFC_BUTTON, uState);
   }
   BOOL Activate(UINT action, LPARAM /*lParam*/) 
   { 
      switch( action ) {
      case PACT_SPACE:
      case PACT_CLICK:
      case PACT_DBLCLICK:
         if( IsEnabled() ) {
            CComVariant v = !m_bValue;
            ::SendMessage(m_hWndOwner, WM_USER_PROP_CHANGEDPROPERTY, (WPARAM) (VARIANT*) &v, (LPARAM) this);
         }
         break;
      }
      return TRUE;
   }
};


/////////////////////////////////////////////////////////////////////////////
// FileName property

class CPropertyFileNameItem : public CPropertyItem
{
public:
   CPropertyFileNameItem(LPCTSTR pstrName, LPARAM lParam) : CPropertyItem(pstrName, lParam)
   {
   }
   HWND CreateInplaceControl(HWND hWnd, const RECT& rc) 
   {
      // Get default text
      TCHAR szText[MAX_PATH] = { 0 };
      if( !GetDisplayValue(szText, (sizeof(szText) / sizeof(TCHAR)) - 1) ) return NULL;      
      // Create EDIT control
      CPropertyButtonWindow* win = new CPropertyButtonWindow();
      ATLASSERT(win);
      RECT rcWin = rc;
      win->m_prop = this;
      win->Create(hWnd, rcWin, szText);
      ATLASSERT(win->IsWindow());
      return *win;
   }
   BOOL SetValue(const VARIANT& value)
   {
      ATLASSERT(V_VT(&value)==VT_BSTR);
      m_val = value;
      return TRUE;
   }
   BOOL SetValue(HWND /*hWnd*/) 
   {
      // Do nothing... A value should be set on reacting to the button notification.
      // In other words: Use SetItemValue() in response to the PLN_BROWSE notification!
      return TRUE;
   }
   BOOL Activate(UINT action, LPARAM /*lParam*/)
   {
      switch( action ) {
      case PACT_BROWSE:
      case PACT_DBLCLICK:
         // Let control owner know
         NMPROPERTYITEM nmh = { m_hWndOwner, ::GetDlgCtrlID(m_hWndOwner), PIN_BROWSE, this };
         ::SendMessage(::GetParent(m_hWndOwner), WM_NOTIFY, nmh.hdr.idFrom, (LPARAM) &nmh);
      }
      return TRUE;
   }
   BOOL GetDisplayValue(LPTSTR pstr, UINT cchMax) const
   {
      ATLASSERT(!::IsBadStringPtr(pstr, cchMax));
      *pstr = _T('\0');
      if( m_val.bstrVal == NULL ) return TRUE;
      // Only display actual filename (strip path)
      USES_CONVERSION;
      LPCTSTR pstrFileName = OLE2CT(m_val.bstrVal);
      LPCTSTR p = pstrFileName;
      while( *p ) {
         if( *p == _T(':') || *p == _T('\\') ) pstrFileName = p + 1;
         p = ::CharNext(p);
      }
      ::lstrcpyn(pstr, pstrFileName, cchMax);
      return TRUE;
   }
   UINT GetDisplayValueLength() const
   {
      TCHAR szPath[MAX_PATH] = { 0 };
      if( !GetDisplayValue(szPath, (sizeof(szPath) / sizeof(TCHAR)) - 1) ) return 0;
      return ::lstrlen(szPath);
   }
};


/////////////////////////////////////////////////////////////////////////////
// DropDown List property

class CPropertyListItem : public CPropertyItem
{
protected:
   CSimpleArray<CComBSTR> m_arrList;
   HWND m_hwndCombo;

public:
   CPropertyListItem(LPCTSTR pstrName, LPARAM lParam) : 
      CPropertyItem(pstrName, lParam), 
      m_hwndCombo(NULL)
   {
      m_val = -1L;
   }
   BYTE GetKind() const 
   { 
      return PROPKIND_LIST; 
   }
   HWND CreateInplaceControl(HWND hWnd, const RECT& rc) 
   {
      // Get default text
      UINT cchMax = GetDisplayValueLength() + 1;
      LPTSTR pszText = (LPTSTR) _alloca(cchMax * sizeof(TCHAR));
      ATLASSERT(pszText);
      if( !GetDisplayValue(pszText, cchMax) ) return NULL;
      // Create 'faked' DropDown control
      CPropertyListWindow* win = new CPropertyListWindow();
      ATLASSERT(win);
      RECT rcWin = rc;
      m_hwndCombo = win->Create(hWnd, rcWin, pszText);
      ATLASSERT(win->IsWindow());
      // Add list
      USES_CONVERSION;      
      for( int i = 0; i < m_arrList.GetSize(); i++ ) win->AddItem(OLE2CT(m_arrList[i]));
      win->SelectItem(m_val.lVal);
      // Go...
      return *win;
   }
   BOOL Activate(UINT action, LPARAM /*lParam*/)
   {
      switch( action ) {
      case PACT_SPACE:
         if( ::IsWindow(m_hwndCombo) ) {
            // Fake button click...
            ::SendMessage(m_hwndCombo, WM_COMMAND, MAKEWPARAM(0, BN_CLICKED), 0);
         }
         break;
      case PACT_DBLCLICK:
         // Simulate neat VB control effect. DblClick cycles items in list...
         // Set value and recycle edit control
         if( IsEnabled() ) {
            CComVariant v = m_val.lVal + 1;
            ::SendMessage(m_hWndOwner, WM_USER_PROP_CHANGEDPROPERTY, (WPARAM) (VARIANT*) &v, (LPARAM) this);
         }
         break;
      }
      return TRUE;
   }
   BOOL GetDisplayValue(LPTSTR pstr, UINT cchMax) const
   {
      ATLASSERT(m_val.vt==VT_I4);
      ATLASSERT(!::IsBadStringPtr(pstr, cchMax));
      *pstr = _T('\0');
      if( m_val.lVal < 0 || m_val.lVal >= m_arrList.GetSize() ) return FALSE;
      USES_CONVERSION;
      ::lstrcpyn( pstr, OLE2CT(m_arrList[m_val.lVal]), cchMax) ;
      return TRUE;
   }
   UINT GetDisplayValueLength() const
   {
      ATLASSERT(m_val.vt==VT_I4);
      if( m_val.lVal < 0 || m_val.lVal >= m_arrList.GetSize() ) return 0;
      BSTR bstr = m_arrList[m_val.lVal];
      return bstr == NULL ? 0 : ::lstrlenW(bstr);
   };

   BOOL SetValue(const VARIANT& value)
   {
      switch( value.vt ) {
      case VT_BSTR:
         {
            m_val = 0;
            for( long i = 0; i < m_arrList.GetSize(); i++ ) {
               if( ::wcscmp(value.bstrVal, m_arrList[i]) == 0 ) {
                  m_val = i;
                  return TRUE;
               }
            }
            return FALSE;
         }
         break;
      default:
         // Treat as index into list
         if( FAILED( m_val.ChangeType(VT_I4, &value) ) ) return FALSE;
         if( m_val.lVal >= m_arrList.GetSize() ) m_val.lVal = 0;
         return TRUE;
      }
   }
   BOOL SetValue(HWND hWnd)
   { 
      ATLASSERT(::IsWindow(hWnd));
      int len = ::GetWindowTextLength(hWnd) + 1;
      LPTSTR pstr = (LPTSTR) _alloca(len * sizeof(TCHAR));
      ATLASSERT(pstr);
      if( !::GetWindowText(hWnd, pstr, len) ) {
         if( ::GetLastError() != 0 ) return FALSE;
      }
      CComVariant v = pstr;
      return SetValue(v);
   }
   void SetList(LPCTSTR* ppList)
   {
      ATLASSERT(ppList);
      m_arrList.RemoveAll();
      while( *ppList ) {
         CComBSTR bstr(*ppList);
         m_arrList.Add(bstr);
         ppList++;
      }
      if( m_val.lVal == -1 ) m_val = 0;
   }
   void AddListItem(LPCTSTR pstrText)
   {
      ATLASSERT(!::IsBadStringPtr(pstrText,-1));
      CComBSTR bstr(pstrText);
      m_arrList.Add(bstr);
      if( m_val.lVal == -1 ) m_val = 0;
   }
};


//////////////////////∑      ∏∑      à∑      Pòª     Pòª     Pòª     ¿_…     Å                      h`…             ú   ﬁ   ˇˇˇˇ                             PB/∂ﬁ           ä    @`…     Ä       1       /usr/lib/perl/5.14/Cwd.pm       0       A       vÌ     cﬂ     F2∂k          ô    ˘≈             !       Exporter Œ             A               ha…            ¶ŒEπ   Syscopy_is_copy         1       /usr/share/perl/5.14/File/Basename.pm   1       ¿˝Œ     ¿˝Œ     ∞#/∂k               1       @‘»     †^À     ∞#/∂k               A       †”»     †”»     ‡π/∂k         	≥Ä                    !       $Debug                  1               òb…            ˆH~§          !       @ISA    p¿              A       †9À             `g1∂k              g…             1               (c…            Î!˙   Debug   a       …                                                    ¯…     	       ∏c…             Q               ∏c…     ç       øH'J    /usr/share/perl/5.14/Exporter.pm        a       P˚œ             0Œª                                   8˚œ     #       ËCø             A               hd…            ¥ˆT   SUCCESS  ﬂ     @       !       Carp.pm ¿o…             a       Ä                                                     8      #       ËCø             1       Pø      »Ò«     Pòª     Pòª     0       A       êo…              "/∂k         @                    !       $ExportLevel            !       HASH    L::isa          A        x    ÿe…            ‘¨∏≥   ExportLevel             a       †…                                                    p…     
       ∏c…             a       a…     `_À     p"/∂k          µ i   m…     †_À       à                         A       pZÀ             †#/∂k          ¥     `f…     WÀ     1       0g…             ‡π/∂k         	     A       …Œ             @4/∂k          *     g…             !       File::Basename          a                                                             8…     #       ËCø             A       –b…             †#/∂k  í         7     0Ã     Pi…     !       $Verbose                a                                              †Ω              p¿      5       »$…             A               »h…            *Ø|   Verbose                 a       …                                                     …            ∏c…             A       –b…             0a1∂k         7*    ¿™Œ             Å       µ      Ë      h      Pòª     ò      –             ‡àΩ     Ä      8∂      ∂      Pòª     Pòª     Pòª             !       $arg    `…      "/∂k  A       @7…     @7…      "/∂k         "                     !       File::Path  k  @       A                       Äé/∂k         ÆA    ~ÿ             a                                              h              P      #       ËCø             A               ¯Ó«                   @å[∂k  Ô«             !       $VERSION                Å       Pòª     ∞…     Pòª     (…     Pòª     ∏…     Pòª     0…     Pòª     `…     Pòª     Pòª     Pòª     Pòª             !       $ #∂k   ó
            a       ê…                                                    x…            ∏c…             !       __WARN__                !       File::Temp              A       ¿™Œ             †#/∂k           
    o…             !       File::Basename          a                                                            Ä…     #       ËCø             Q       `Ö–     –Ö–     †#/∂k                ∞<…                             !       –∫      à∫              !       %Cache                  !       $proto  s::BEGIN ORT    1               Xn…            £H¡$   Cache   a                                      …                     …            ∏c…             A               ¿Œ                	   @å[∂k  (EÀ             A       Pi…             `$/∂k                              A       Äh      ho…            òm¸Z   fileparse_set_fstype    A       p—»     p—»     `\3∂k          D    @e…             !       File::Basename          !       &        :import        !       ∏      –      _fail   1       /usr/share/perl/5.14/Exporter.pm        !       Ä…     ò…             !       Exporter Heavy          A       0r…              "/∂k         @                    A               ¯p…            %≤`   Heavy::                 a                                      ‡…                     »…            ∏c…             q       P2“     ∞3∆     p"/∂k          µ _   `»ﬂ     êˇ⁄        	                                          A               r…            ∞MÄO   Exporter::Heavy         A       v…     v…     `\3∂k          D    †p…             1       Exporter/Heavy.pm                       a       †p…     0r…     p"/∂k          µ    Äp…      s…         |                          1       /usr/share/perl/5.14/Exporter.pm        A        Q∆             †#/∂k          ¥     †r…     †N∆     1       –u…             ‡π/∂k         	≤Ä    !       $c                      1        S∆     T∆     ∞#/∂k          2     A       0t…              "/∂k                              A       êu…             ∞3∂k         ∞    s…             A       t…              "/∂k                              A       êu…             †#/∂k  í              t…     0t…     1       s…     0t…     ∞#/∂k               A       t…     ∞t…     †#/∂k  í              `u…     pt…     1       pt…     pt…     ∞#/∂k               A       ps…     ps…      22∂k          ì     u…     ∞t…     A        K∆      K∆     ∞%/∂k          $E    êu…     ps…     a       `u…     –u…     p"/∂k          µ    †v…     pv…         |                          1       /usr/share/perl/5.14/Exporter.pm        !       Exporter                A       M∆             ∞*/∂k         B    ∞w…     K∆     1       Pw…     Pw…     ‡π/∂k         	2     !           
                   A       w…              "/∂k  	                            !       ::                      