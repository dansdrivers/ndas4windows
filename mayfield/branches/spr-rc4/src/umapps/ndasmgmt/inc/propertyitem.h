ner);
      ctrl.SetRedraw(FALSE);
      idx++;
      while( m_arrItems.GetSize() > 0 ) {
         IProperty* prop = m_arrItems[0];
         ATLASSERT(prop);
         m_arrItems.RemoveAt(0);
         int item = ctrl.InsertString(idx++, prop->GetName());
         ctrl.SetItemData(item, (DWORD) prop);
      }
      m_fExpanded = true;
      ctrl.SetRedraw(TRUE);
      ctrl.Invalidate();
      return TRUE;
   }
   BOOL Collapse(int idx)
   {
      ATLASSERT(::IsWindow(m_hWndOwner));
      CListBox ctrl(m_hWndOwner);
      ctrl.SetRedraw(FALSE);
      idx++;
      while( idx < ctrl.GetCount() ) {
         IProperty* prop = reinterpret_cast<IProperty*>(ctrl.GetItemData(idx));
         ATLASSERT(prop);
         if( prop->GetKind() == PROPKIND_CATEGORY ) break;
         ctrl.SetItemData(idx, 0); // Clear data now, so WM_DELETEITEM doesn't delete
                                   // the IProperty in the DeleteString() call below
         ctrl.DeleteString(idx);
         m_arrItems.Add(prop);
      }
      m_fExpanded = false;
      ctrl.SetRedraw(TRUE);
      ctrl.Invalidate();
      return TRUE;
   }
   IProperty* GetProperty(int iIndex) const
   {
      if( iIndex < 0 || iIndex >= m_arrItems.GetSize() ) return NULL;
      return m_arrItems[iIndex];
   }
};

inline HPROPERTY PropCreateCategory(LPCTSTR pstrName, LPARAM lParam=0)
{
   return new CCategoryProperty(pstrName, lParam);
}


/////////////////////////////////////////////////////////////////////////////
// CPropertyList control

template< class T, class TBase = CListBox, class TWinTraits = CWinTraitsOR<LBS_OWNERDRAWVARIABLE|LBS_NOTIFY> >
class ATL_NO_VTABLE CPropertyListImpl : 
   public CWindowImpl< T, TBase, TWinTraits >,
   public COwnerDraw< CPropertyListImpl >
{
public:
   DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())

   enum { CATEGORY_INDENT = 16 };

   PROPERTYDRAWINFO m_di;
   HWND m_hwndInplace;
   int  m_iInplaceIndex;
   DWORD m_dwExtStyle;
   CFont m_TextFont;
   CFont m_CategoryFont;
   CPen m_BorderPen;
   int m_iPrevious;
   int m_iPrevXGhostBar;
   int m_iMiddle;
   bool m_bColumnFixed;

   CPropertyListImpl() : 
      m_hwndInplace(NULL), 
      m_iInplaceIndex(-1), 
      m_dwExtStyle(0UL),
      m_iMiddle(0),
      m_bColumnFixed(false),
      m_iPrevious(0),
      m_iPrevXGhostBar(0)
   {
   }

   // Operations

   BOOL SubclassWindow(HWND hWnd)
   {
      ATLASSERT(m_hWnd==NULL);
      ATLASSERT(::IsWindow(hWnd));
      BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
      if( bRet ) _Init();
      return bRet;
   }

   void SetExtendedListStyle(DWORD dwExtStyle)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Assign styles
      if( dwExtStyle & PLS_EX_SORTED ) {
         ATLASSERT((dwExtStyle & PLS_EX_CATEGORIZED)==0); // We don't support sorted categories!
         ATLASSERT(GetStyle() & LBS_SORT);
         ATLASSERT(GetStyle() & LBS_HASSTRINGS);
      }
      m_dwExtStyle = dwExtStyle;
      // Recalc colours and fonts
      SendMessage(WM_SETTINGCHANGE);
   }
   DWORD GetExtendedListStyle() const
   {
      return m_dwExtStyle;
   }

   void ResetContent()
   {
      ATLASSERT(::IsWindow(m_hWnd));
      _DestroyInplaceWindow();
      TBase::ResetContent();
   }
   HPROPERTY AddItem(HPROPERTY prop)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ATLASSERT(prop);
      if( prop == NULL ) return NULL;
      prop->SetOwner(m_hWnd, NULL);
      int nItem = TBase::AddString(prop->GetName());
      if( nItem == LB_ERR ) return NULL;
      TBase::SetItemData(nItem, (DWORD_PTR) prop);
      return prop;
   }
   BOOL DeleteItem(HPROPERTY prop)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ATLASSERT(prop);
      ATLASSERT(prop->GetKind()!=PROPKIND_CATEGORY);
      // Delete *visible* property!
      int iIndex = FindProperty(prop);
      if( iIndex == -1 ) return FALSE;
      return TBase::DeleteString((UINT) iIndex) != LB_ERR;
   }
   HPROPERTY GetProperty(int index) const 
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ATLASSERT(index!=-1);
      IPro