#include "stdafx.h"
#include "resource.h"
#include "res1.h"

#include "utils.h"
#include "apputils.h"

#include "FBEView.h"
#include "FBDoc.h"
#include "TreeView.h"
#include "RuntimeLocalization.h"
#include "UiMetrics.h"
#include "ThemeManager.h"
#include "toolbars/ToolbarFactory.h"
#include <commoncontrols.h>

extern CElementDescMnr _EDMnr;

static WPARAM TreeCommandWParam(WORD command) { return static_cast<WPARAM>(MAKELONG(0, command)); }

namespace
{
const int kFavoriteScriptImageIndex = 1;

HBITMAP CreateScriptTreeGlyphBitmap(bool favorite, int size)
{
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = size;
	info.bmiHeader.biHeight = -size;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	void* bits = NULL;
	HBITMAP bitmap = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
	HDC dc = bitmap != NULL ? ::CreateCompatibleDC(NULL) : NULL;
	if(dc == NULL) { if(bitmap != NULL) ::DeleteObject(bitmap); return NULL; }
	HGDIOBJ oldBitmap = ::SelectObject(dc, bitmap);
	::SetMapMode(dc, MM_ANISOTROPIC);
	::SetWindowExtEx(dc, 20, 20, NULL);
	::SetViewportExtEx(dc, size, size, NULL);
	RECT canvas = { 0, 0, 20, 20 };
	HBRUSH transparent = ::CreateSolidBrush(RGB(255, 0, 255));
	::FillRect(dc, &canvas, transparent);
	::DeleteObject(transparent);
	HPEN outline = ::CreatePen(PS_SOLID, 1, RGB(67, 53, 32));
	HBRUSH fill = ::CreateSolidBrush(favorite ? RGB(255, 199, 54) : RGB(245, 245, 245));
	HGDIOBJ oldPen = ::SelectObject(dc, outline);
	HGDIOBJ oldBrush = ::SelectObject(dc, fill);
	if(favorite)
	{
		const POINT star[] = { { 10, 2 }, { 12, 7 }, { 18, 7 }, { 14, 11 }, { 16, 17 },
			{ 10, 14 }, { 4, 17 }, { 6, 11 }, { 2, 7 }, { 8, 7 } };
		::Polygon(dc, star, _countof(star));
	}
	else
	{
		::Rectangle(dc, 4, 2, 16, 18);
		::MoveToEx(dc, 7, 7, NULL); ::LineTo(dc, 13, 7);
		::MoveToEx(dc, 7, 10, NULL); ::LineTo(dc, 13, 10);
		::MoveToEx(dc, 7, 13, NULL); ::LineTo(dc, 12, 13);
	}
	::SelectObject(dc, oldBrush);
	::SelectObject(dc, oldPen);
	::DeleteObject(fill);
	::DeleteObject(outline);
	::SelectObject(dc, oldBitmap);
	::DeleteDC(dc);
	DWORD* pixels = static_cast<DWORD*>(bits);
	for(int index = 0; index < size * size; ++index)
		if((pixels[index] & 0x00ffffffu) != RGB(255, 0, 255)) pixels[index] |= 0xff000000u;
	return bitmap;
}
}

namespace
{
bool ScriptVisualsMatch(const std::vector<ScriptTreeVisual>& left, const std::vector<ScriptTreeVisual>& right, size_t itemCount)
{
	for(size_t index = 0; index < itemCount; ++index)
	{
		const HICON leftIcon = index < left.size() ? left[index].icon : NULL;
		const HICON rightIcon = index < right.size() ? right[index].icon : NULL;
		const HBITMAP leftBitmap = index < left.size() ? left[index].bitmap : NULL;
		const HBITMAP rightBitmap = index < right.size() ? right[index].bitmap : NULL;
		if(leftIcon != rightIcon || leftBitmap != rightBitmap) return false;
	}
	return true;
}
}

struct RuntimeTreeMenuBinding
{
	UINT commandId;
	LPCWSTR key;
};

static const RuntimeTreeMenuBinding kDocumentTreeMenuBindings[] = {
	{ ID_DT_VIEW, L"fbe.menu.idr_document_tree.view" },
	{ ID_DT_VIEWSOURCE, L"fbe.menu.idr_document_tree.view_source" },
	{ ID_DT_RIGHT_ONE, L"fbe.menu.idr_document_tree.move_right" },
	{ ID_DT_RIGHT_SMART, L"fbe.menu.idr_document_tree.make_child" },
	{ ID_DT_LEFT, L"fbe.menu.idr_document_tree.move_left" },
	{ ID_DT_DELETE, L"fbe.menu.idr_document_tree.delete" },
};

static void ApplyRuntimeDocumentTreeMenuLocalization(HMENU menu)
{
	if(menu == NULL)
		return;

	const int count = ::GetMenuItemCount(menu);
	for(int i = 0; i < count; ++i)
	{
		const UINT commandId = ::GetMenuItemID(menu, i);
		if(commandId == static_cast<UINT>(-1) || commandId == 0)
			continue;

		for(size_t binding = 0; binding < _countof(kDocumentTreeMenuBindings); ++binding)
		{
			if(kDocumentTreeMenuBindings[binding].commandId != commandId)
				continue;

			CString text = FbeLoadRuntimeStringByKey(kDocumentTreeMenuBindings[binding].key);
			if(!text.IsEmpty())
				::ModifyMenu(menu, i, MF_BYPOSITION | MF_STRING, commandId, text);
			break;
		}
	}
}

// redrawing the tree is _very_ ugly visually, so we first build a copy and compare them
struct TreeNode {
  TreeNode    *parent,*next,*last,*child;
  CString     text;
  int	      img;
  MSHTML::IHTMLElement	*pos;
  TreeNode() : parent(0), next(0), child(0), last(0), img(0), pos(0) { }
  TreeNode(TreeNode *nn,const CString& s,int ii,MSHTML::IHTMLElement *pp) : parent(nn), next(0), last(0), child(0),
    text(s), img(ii), pos(pp)
  {
    if (pos)
      pos->AddRef();
  }
  ~TreeNode() {
    if (pos)
      pos->Release();
    TreeNode *q;
    for (TreeNode *n=child;n;n=q) {
      q=n->next;
      delete n;
    }
  }
  TreeNode *Append(const CString& s,int ii,MSHTML::IHTMLElement *p) {
    TreeNode *n=new TreeNode(this,s,ii,p);
    if (last) {
      last->next=n;
      last=n;
    } else
      last=child=n;
    return n;
  }
};

static bool  SearchUnder(CTreeItem& ret,CTreeItem ii,MSHTML::IHTMLElement *p) {
  CTreeItem   jj(ii.GetChild());
  while (!jj.IsNull()) {
    MSHTML::IHTMLElement    *n=(MSHTML::IHTMLElement *)jj.GetData();
    // IHTMLElement::contains is not a reliable self-match on all MSHTML
    // document modes.  A caret can resolve directly to the structural node.
    if (n && (n == p || n->contains(p))) {
      ret=jj;
      if (jj.HasChildren())
	SearchUnder(ret,jj,p);
      return true;
    }
    jj=jj.GetNextSibling();
  }
  return false;
}

CTreeItem CTreeView::LocatePosition(MSHTML::IHTMLElement *p) {
  CTreeItem   ret(TVI_ROOT,this);
  if (GetCount()==0 || !p)
    return ret; // no items at all

	// sourceIndex is stable for the current MSHTML document.  Walk only the
	// selection's ancestors, rather than asking every tree item contains().
	try {
		for (MSHTML::IHTMLElementPtr current(p); current; current = current->parentElement) {
			const std::map<long, HTREEITEM>::const_iterator found = m_source_index.find(current->sourceIndex);
			if (found != m_source_index.end()) {
				++m_tree_index_lookup_count;
				return CTreeItem(found->second, this);
			}
		}
	}
	catch (_com_error&) {}

	++m_tree_linear_fallback_count;
  SearchUnder(ret,ret,p);

  return ret;
}

void CTreeView::IndexTreeItem(CTreeItem item)
{
	for (CTreeItem current(item); !current.IsNull(); current = current.GetNextSibling())
	{
		MSHTML::IHTMLElementPtr element(reinterpret_cast<MSHTML::IHTMLElement*>(current.GetData()));
		if (element && element->sourceIndex >= 0)
			m_source_index[element->sourceIndex] = current;
		if (current.HasChildren()) IndexTreeItem(current.GetChild());
	}
}

void CTreeView::RebuildSourceIndex()
{
	m_source_index.clear();
	if (GetCount()) IndexTreeItem(CTreeItem(TVI_ROOT, this).GetChild());
}

void  CTreeView::HighlightItemAtPos(MSHTML::IHTMLElement *p) {
  CTreeItem ii(LocatePosition(p));
  if (ii==m_last_lookup_item && static_cast<HTREEITEM>(ii) == m_current_item)
    return;
  m_last_lookup_item=ii;
  const HTREEITEM previous = m_current_item;
  m_current_item = ii != TVI_ROOT ? static_cast<HTREEITEM>(ii) : NULL;
  if(previous) ::InvalidateRect(m_hWnd, NULL, FALSE);
  if(m_current_item) ::InvalidateRect(m_hWnd, NULL, FALSE);
}

LRESULT CTreeView::OnCustomDraw(int, LPNMHDR header, BOOL& bHandled)
{
	if(header == NULL || header->code != NM_CUSTOMDRAW) { bHandled = FALSE; return CDRF_DODEFAULT; }
	LPNMTVCUSTOMDRAW draw = reinterpret_cast<LPNMTVCUSTOMDRAW>(header);
	if(ThemeManager::IsHighContrast()) return CDRF_DODEFAULT;
	const HTREEITEM item = reinterpret_cast<HTREEITEM>(draw->nmcd.dwItemSpec);
	if(draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT && item == m_current_item)
	{
		if(!(GetItemState(m_current_item, TVIS_SELECTED) & TVIS_SELECTED))
		{
			draw->clrTextBk = ThemeManager::HoverColor();
			draw->clrText = ThemeManager::TextColor();
			return CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
		}
		// A selected current item keeps the native selection background, then
		// receives the accent marker at post-paint below.
		return CDRF_NOTIFYPOSTPAINT;
	}
	if(draw->nmcd.dwDrawStage == CDDS_ITEMPOSTPAINT && item == m_current_item)
	{
		RECT bounds = {};
		if(GetItemRect(item, &bounds, FALSE))
		{
			const int markerWidth = (std::max)(1, UiMetrics::ScaleForDpi(3, UiMetrics::DpiForWindow(m_hWnd)));
			bounds.right = (std::min)(bounds.right, bounds.left + markerWidth);
			HBRUSH marker = ::CreateSolidBrush(ThemeManager::AccentColor());
			if(marker) { ::FillRect(draw->nmcd.hdc, &bounds, marker); ::DeleteObject(marker); }
		}
	}
	return CDRF_DODEFAULT;
}


static void MakeNode(TreeNode* parent, MSHTML::IHTMLDOMNodePtr elem)
{
	if (elem->nodeType != 1)
		return;

	MSHTML::IHTMLElementPtr he(elem);
  /*_bstr_t		    nn(he->tagName);
  _bstr_t		    cn(he->className);
  if (U::scmp(nn,L"DIV")==0) {
    // at this point we are interested only in sections/subtitles/poems/stanzas
    int img=0;
    CString txt;
    if (U::scmp(cn,L"poem")==0 || U::scmp(cn,L"stanza")==0) {
      txt=FindTitle(elem);
      img=3;
    } 
	else if (U::scmp(cn,L"body")==0) {
      txt=AU::GetAttrCS(he,L"fbname");
      U::NormalizeInplace(txt);
      if (txt.IsEmpty())
	txt=FindTitle(elem);
      img=0;
    } else if (U::scmp(cn,L"section")==0) {
      txt=FindTitle(elem);
      img=0;
    } else if (U::scmp(cn,L"epigraph")==0 || U::scmp(cn,L"annotation")==0 ||
	       U::scmp(cn,L"history")==0 || U::scmp(cn,L"cite")==0)
    {
      img=0;
    } 
	// Modification by Pilgrim
	else if (U::scmp(cn,L"table")==0 || U::scmp(cn,L"tr")==0 || U::scmp(cn,L"th")==0 || U::scmp(cn,L"td")==0) {
		txt=FindTitle(elem);
		img=3;
	} 
	else if (U::scmp(cn,L"image")==0) {
		txt=GetImageFileName(elem);
		img=3;
	} 	
	else
	{
		elem=elem->firstChild;
		while ((bool)elem) 
		{

			MakeNode(parent,elem);
			elem=elem->nextSibling;
		}
		return;
	}      
    U::NormalizeInplace(txt);
    if (txt.IsEmpty())
      txt.Format(_T("<%s>"),(const TCHAR *)cn);
    parent=parent->Append(txt,img,he);
  } else if (U::scmp(nn,L"P")==0 && U::scmp(cn,L"subtitle")==0) {
    CString txt((const TCHAR *)he->innerText);
    U::NormalizeInplace(txt);
    if (txt.IsEmpty())
      txt=_T("<subtitle>");
    parent=parent->Append(txt,6,he);
  }*/

	_bstr_t cn(he->className);
	CElementDescriptor* ED = 0;
	if(_EDMnr.GetElementDescriptor(he, &ED) && ED->ViewInTree())
	{
		CString txt = ED->GetTitle(he);
		U::NormalizeInplace(txt);
		if(txt.IsEmpty())
			txt.Format(_T("<%s>"), (const TCHAR*)cn);
		parent = parent->Append(txt, ED->GetDTImageID(), he);
	}

	elem = elem->firstChild;
	while((bool)elem)
	{
		MakeNode(parent, elem);
		elem = elem->nextSibling;
	}

	return;
}

static TreeNode  *GetDocTree(const MSHTML::IHTMLDocument2Ptr& view)
{
  TreeNode	*root=new TreeNode();
  try {
    MakeNode(root,view->body);
  }
  catch (_com_error&) {
  }
  if (!root->child) {
    delete root;
    return NULL;
  }
  return root;
}

static void  CompareTreesAndSet(TreeNode *n,CTreeItem ii,bool& fDisableRedraw) {
  bool	fH1=ii==TVI_ROOT ? ii.m_pTreeView->GetCount()>0 : ii.HasChildren()!=0;
  // walk them one by one and check
  CTreeItem   nc=fH1 ? ii.GetChild() : CTreeItem(NULL,ii.m_pTreeView);
  TreeNode    *ic=n->child;
  CString     text;
  while (ic && nc) {
    int	  img1,img2;
    nc.GetImage(img1,img2);
    nc.GetText(text);
    if (text!=ic->text || img1!=ic->img) { // differ
      // copy the item here
      if (!fDisableRedraw) {
	ii.m_pTreeView->SetRedraw(FALSE);
	fDisableRedraw=true;
      }
      nc.SetImage(ic->img,ic->img);
      nc.SetText(ic->text);
    } 
    MSHTML::IHTMLElement    *od=(MSHTML::IHTMLElement *)nc.GetData();
    if (od)
      od->Release();
    if (ic->pos)
      ic->pos->AddRef();
    nc.SetData((LPARAM)ic->pos);
    CompareTreesAndSet(ic,nc,fDisableRedraw);
    ic=ic->next;
    nc=nc.GetNextSibling();
  }
  CTreeItem next;
  if ((nc || ic) && !fDisableRedraw) {
    ii.m_pTreeView->SetRedraw(FALSE);
    fDisableRedraw=true;
  }
  while (nc) { // remove extra children, staring with ic
    next=nc.GetNextSibling();
    nc.Delete();
    nc=next;
  }
  while (ic) { // append children to ii
    nc=ii.AddTail(ic->text,ic->img);
    if (ic->pos)
      ic->pos->AddRef();
    nc.SetData((LPARAM)ic->pos);
    if (ic->child)
      CompareTreesAndSet(ic,nc,fDisableRedraw);
    ic=ic->next;
  }
}

void  CTreeView::GetDocumentStructure(const MSHTML::IHTMLDocument2Ptr& view) {
  m_last_lookup_item=0;

  TreeNode  *root=GetDocTree(view);
  if (!root) {
		m_source_index.clear();
		m_current_item = NULL;
    SetRedraw(FALSE);
    DeleteAllItems();
    SetRedraw(TRUE);
    return;
  }
  bool	fDisableRedraw=false;
  CompareTreesAndSet(root,CTreeItem(TVI_ROOT,this),fDisableRedraw);
  if (fDisableRedraw)
    SetRedraw(TRUE);
  delete root;
	RebuildSourceIndex();
}

void CTreeView::UpdateAll()
{
	::SendMessage(m_main_window, WM_COMMAND, TreeCommandWParam(IDN_TREE_UPDATE_ME), (LPARAM)m_hWnd);
}

void  CTreeView::UpdateDocumentStructure(const MSHTML::IHTMLDocument2Ptr& v,MSHTML::IHTMLDOMNodePtr node) {
  MSHTML::IHTMLElementPtr     ce(node);

  /*CTreeItem selected_item = GetFirstSelectedItem();
  MSHTML::IHTMLElementPtr selected_elem;
  if(!selected_item.IsNull() && selected_item.GetData())
  {
	  selected_elem = (MSHTML::IHTMLElement*)selected_item.GetData();
  }*/

  CTreeItem   ii(LocatePosition(ce));

  if (ii==TVI_ROOT) { // huh?
    GetDocumentStructure(v);
    return;
  }

  MSHTML::IHTMLElementPtr   jj((MSHTML::IHTMLElement *)ii.GetData());

  // shortcut for the most common situation
  // all changes confined to a P, which is not in the title
  if (U::scmp(jj->tagName,L"DIV")==0 && U::scmp(node->nodeName,L"P")==0) {
	  MSHTML::IHTMLElementPtr   tn(U::FindTitleNode(jj));
    if (!(bool)tn || (tn!=ce && tn->contains(ce)!=VARIANT_TRUE))
      return;

	// ???? ???????? ????????? ?????? ? ?????????, ?? ?????? ?????? ????? ?????????
	if ((bool)tn && (tn==ce || tn->contains(ce)==VARIANT_TRUE))
	{
		CString txt = (wchar_t*)tn->innerText;
		U::NormalizeInplace(txt);
		ii.SetText(txt);
		return;
	}
  }

  TreeNode nn;

  try
  {
	MakeNode(&nn,jj);
  }
  catch(_com_error&){ return; }
  
  bool	fDisableRedraw=false;
  // check the item itself
  CString text;
  int	  img1,img2;
  ii.GetImage(img1,img2);
  ii.GetText(text);
  if (text!=nn.child->text || img1!=nn.child->img) { // differ
    // copy the item here
    SetRedraw(FALSE);
    fDisableRedraw=true;
    ii.SetImage(nn.child->img,nn.child->img);
    ii.SetText(nn.child->text);
  }

  CompareTreesAndSet(nn.child,ii,fDisableRedraw);
  if (fDisableRedraw)
    SetRedraw(TRUE);
	RebuildSourceIndex();

 /* if((bool)selected_elem)
  {
	  SelectElement(selected_elem);
  }*/
  
}

BOOL CTreeView::PreTranslateMessage(MSG* pMsg)
{
  pMsg;
  return FALSE;
}

LRESULT CTreeView::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  // "CTreeViewCtrl::OnCreate()"
  LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);
  
  // "OnInitialUpdate"
  m_ImageList.CreateFromImage(IDB_STRUCTURE,16,32,RGB(255,0,255),IMAGE_BITMAP);
	  m_script_image_size = ToolbarFactory::CommandToolbarImageSize(UiMetrics::DpiForWindow(m_hWnd));
	  m_scriptImageList.Create(m_script_image_size,m_script_image_size,ILC_COLOR32|ILC_MASK,16,8);
  SetImageList(m_ImageList,TVSIL_NORMAL);

  SetScrollTime(1);
  FillEDMnr();

  
  bHandled = TRUE;
  
  return lRet;
}

LRESULT CTreeView::OnDestroy(UINT /* unused: uMsg */, WPARAM /* unused: wParam */, LPARAM /* unused: lParam */, BOOL& bHandled)
{
  SetImageList(NULL,TVSIL_NORMAL);
  m_scriptImageList.Destroy();
  m_ImageList.Destroy();
/*  delete m_bodyED;
  delete m_sectionED;
  delete m_imageED;
  delete m_poemED;*/
  
  // Say that we didn't handle it so that the treeview and anyone else
  //  interested gets to handle the message
  bHandled = FALSE;
  return 0;
}

LRESULT CTreeView::OnClick(UINT /* unused: uMsg */, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = SetMultiSelection(wParam, lParam);
  // check if we are going to hit an item
  /*UINT	  flags=0;
  CTreeItem ii(HitTest(CPoint(LOWORD(lParam),HIWORD(lParam)),&flags));
  if (flags&TVHT_ONITEM && !ii.IsNull()) { // try to select and expand it
    CTreeItem	sel(GetSelectedItem());
    if (sel!=ii)
      ii.Select();
    if (!(ii.GetState(TVIS_EXPANDED)&TVIS_EXPANDED))
      ii.Expand();
    SetFocus();
  } //else*/
    //bHandled=FALSE;
  return 0;
}

LRESULT CTreeView::OnDblClick(UINT /* unused: uMsg */, WPARAM /* unused: wParam */, LPARAM lParam, BOOL& /* unused: bHandled */)
{
	if(m_script_mode)
	{
		UINT flags = 0;
		CTreeItem item(HitTest(CPoint(LOWORD(lParam), HIWORD(lParam)), &flags), this);
		if((flags & TVHT_ONITEM) && !item.IsNull())
		{
			SelectItem(item);
			const ScriptDescriptor* script = SelectedScript();
			if(script != NULL && script->isFolder) item.Expand((item.GetState(TVIS_EXPANDED) & TVIS_EXPANDED) ? TVE_COLLAPSE : TVE_EXPAND);
			else RunSelectedScript();
		}
		return 0;
	}
  // check if we double-clicked an already selected item item
  UINT	  flags=0;
  CTreeItem ii(HitTest(CPoint(LOWORD(lParam),HIWORD(lParam)),&flags));
  if (flags&TVHT_ONITEM && !ii.IsNull() && ii==GetSelectedItem())
    ::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_CLICK),(LPARAM)m_hWnd);
  return 0;
}

static void RecursiveExpand(CTreeItem n,bool *fEnable) {
  for (CTreeItem   ch(n.GetChild());!ch.IsNull();ch=ch.GetNextSibling()) {
    if (!ch.HasChildren())
      continue;
    if (!(ch.GetState(TVIS_EXPANDED)&TVIS_EXPANDED)) {
      if (!*fEnable) {
	*fEnable=true;
	ch.GetTreeView()->SetRedraw(FALSE);
      }
      ch.Expand();
    }
    RecursiveExpand(ch,fEnable);
  }
}

LRESULT CTreeView::OnChar(UINT /* unused: uMsg */, WPARAM wParam, LPARAM /* unused: lParam */, BOOL& bHandled)
{
  switch (wParam) {
  case VK_RETURN: // swallow
    break;
  case '*': // expand the entire tree
    if (GetCount()>0) {
      bool  fEnable=false;
      RecursiveExpand(CTreeItem(TVI_ROOT,this),&fEnable);
      if (fEnable)	 
	SetRedraw(TRUE);
    }
    break;
  default: // pass to control
    bHandled=FALSE;
  }
  bHandled=FALSE;
  return 0;
}

LRESULT CTreeView::OnKeyDown(UINT /* unused: uMsg */, WPARAM wParam, LPARAM /* unused: lParam */, BOOL& bHandled)
{  
	if(!m_script_mode && wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
	{
		SelectAllVisibleItems();
		bHandled = TRUE;
		return 0;
	}
	if(m_script_mode && wParam == VK_RETURN)
	{
		const ScriptDescriptor* script = SelectedScript();
		if(script != NULL && script->isFolder) {
			CTreeItem item = GetSelectedItem(); item.Expand((item.GetState(TVIS_EXPANDED) & TVIS_EXPANDED) ? TVE_COLLAPSE : TVE_EXPAND);
		} else RunSelectedScript();
		bHandled = TRUE;
		return 0;
	}
	if(m_script_mode && wParam == 'F' && (GetKeyState(VK_CONTROL) & 0x8000) && ::IsWindow(m_script_search_window))
	{
		::SetFocus(m_script_search_window);
		bHandled = TRUE;
		return 0;
	}
  if (wParam==VK_RETURN)
    ::PostMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_RETURN),(LPARAM)m_hWnd);

  if ( (wParam==VK_UP || wParam==VK_DOWN) && GetKeyState( VK_SHIFT )&0x8000)
	{
		// Initialize the reference item if this is the first shift selection
		if( !m_hItemFirstSel )
		{
			m_hItemFirstSel = GetSelectedItem();
			ClearSelection();
		}

		// Find which item is currently selected
		HTREEITEM hItemPrevSel = GetSelectedItem();

		HTREEITEM hItemNext;
		if ( wParam==VK_UP )
			hItemNext = GetPrevVisibleItem( hItemPrevSel );
		else
			hItemNext = GetNextVisibleItem( hItemPrevSel );

		if ( hItemNext )
		{
			// Determine if we need to reselect previously selected item
			BOOL bReselect =
				!( GetItemState( hItemNext, TVIS_SELECTED ) & TVIS_SELECTED );

			// Select the next item - this will also deselect the previous item
			SelectItem( hItemNext );

			// Reselect the previously selected item
			if ( bReselect )
				SetItemState( hItemPrevSel, TVIS_SELECTED, TVIS_SELECTED );
		}
		bHandled=TRUE;
		return 0;
	}
	else if( wParam >= VK_SPACE )
	{
		m_hItemFirstSel = NULL;
		ClearSelection();
	}
	
  
  bHandled=FALSE;
  return 0;
}

LRESULT CTreeView::OnBegindrag(int /* unused: idCtrl */, LPNMHDR mhdr, BOOL& bHandled)
{
	if(m_script_mode) { bHandled = TRUE; return 0; }
	LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) mhdr;
	
	m_move_from.m_hTreeItem = pnmtv->itemNew.hItem;	
	
	m_himlDrag = TreeView_CreateDragImage(*this, pnmtv->itemNew.hItem);
	ImageList_BeginDrag(this->m_himlDrag, 0, 0, 0);
	ImageList_DragEnter(*this, pnmtv->ptDrag.x,pnmtv->ptDrag.y);
    SetCapture();
	m_drag = true;
	bHandled = false;
	return 0;
}

LRESULT CTreeView::OnLButtonUp(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
	if(m_script_mode) { bHandled = TRUE; return 0; }
	if(m_drag)
	{
		EndDrag();
		::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_ELEMENT),(LPARAM)m_hWnd);
	}
	bHandled = false;
	return 0;
}

LRESULT CTreeView::OnMouseMove(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
{
	if(m_script_mode) { bHandled = TRUE; return 0; }
	if(m_drag)
	{
		POINTS Pos = MAKEPOINTS(lParam);
		POINT point;
		point.x = Pos.x;
		point.y = Pos.y;

		ImageList_DragMove(Pos.x, Pos.y);
		UINT		flags;
		CTreeItem	hitem(this->HitTest(point, &flags), this);		
		if (!hitem.IsNull())
		{
			if(IsDropChangePosition(flags, hitem))
			{		
				CImageList::DragShowNolock(FALSE);
				if(IsParent(m_move_from, hitem))
				{
					SetItemImage(m_move_to, m_drop_item_nimage, m_drop_item_nimage);	
					GetItemImage(hitem, m_drop_item_nimage, m_drop_item_nimage);
					m_insert_type = CTreeView::none;	
					SelectItem(hitem);
				}
                else if(flags & TVHT_ONITEMICON)
				{
					SetItemImage(m_move_to, m_drop_item_nimage, m_drop_item_nimage);
					GetItemImage(hitem, m_drop_item_nimage, m_drop_item_nimage);
					SetItemImage(hitem, m_drop_item_nimage + 1, m_drop_item_nimage + 1);
					m_insert_type = CTreeView::sibling;
					SelectItem(hitem);
				}
				else if(flags & TVHT_ONITEMLABEL)
				{
					SetItemImage(m_move_to, m_drop_item_nimage, m_drop_item_nimage);
					GetItemImage(hitem, m_drop_item_nimage, m_drop_item_nimage);
					SetItemImage(hitem, m_drop_item_nimage + 2, m_drop_item_nimage + 2);				
					m_insert_type = CTreeView::child;
					SelectItem(hitem);
				}
				else
				{
					SetItemImage(m_move_to, m_drop_item_nimage, m_drop_item_nimage);	
					GetItemImage(hitem, m_drop_item_nimage, m_drop_item_nimage);
					m_insert_type = CTreeView::none;										
				}
				m_move_to.m_hTreeItem = hitem;
				
				CImageList::DragShowNolock(TRUE);
			}
			else
			{
				//SetItemImage(m_move_to, m_drop_item_nimage, m_drop_item_nimage);
			}		
		}
	}
	
	bHandled = false;
	return 0;	
}

LRESULT CTreeView::OnRClick(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	if(m_script_mode)
	{
		UINT flags = 0; CTreeItem item(HitTest(CPoint(LOWORD(lParam), HIWORD(lParam)), &flags), this);
		if(!item.IsNull() && (flags & TVHT_ONITEM)) SelectScriptContextItem(item);
		bHandled = TRUE;
		return 0;
	}
	if(m_drag)
	{
		EndDrag();
	}
	UINT flags = 0;
	CTreeItem htItem(HitTest(CPoint(LOWORD(lParam),HIWORD(lParam)),&flags), this);
	if(!htItem.GetState(TVIS_SELECTED))
	{
		ClearSelection();
		SelectItem(htItem);
	}
	bHandled = FALSE;
    return 0;
 }

LRESULT CTreeView::OnContextMenu(UINT /*uMsg*/, WPARAM /* unused: wParam */, LPARAM lParam, BOOL& /*bHandled*/)
{
	if(m_script_mode)
	{
		CPoint point = (CPoint)lParam;
		CTreeItem item;
		if(point.x == -1 && point.y == -1)
		{
			item = GetSelectedItem();
			CRect selected;
			if(item.IsNull() || !GetItemRect(item, &selected, TRUE)) return 0;
			point = selected.CenterPoint(); ClientToScreen(&point);
		}
		else
		{
			CPoint client(point); ScreenToClient(&client);
			item = CTreeItem(HitTest(client, NULL), this);
		}
		if(item.IsNull()) return 0;
		SelectScriptContextItem(item);
		const std::map<HTREEITEM, size_t>::const_iterator target = m_script_nodes.find(item);
		if(target == m_script_nodes.end() || target->second >= m_script_items.size() || m_script_items[target->second].isFolder) return 0;
		const std::vector<const ScriptDescriptor*> scripts = SelectedScripts();
		if(scripts.empty()) return 0;
		++m_script_popup_invocations;
		const bool multiple = scripts.size() > 1;
		const bool allFavorites = SelectedScriptsAreAllFavorites();
		m_script_popup_run_enabled = !multiple;
		m_script_popup_open_enabled = !multiple;
		CMenu menu; menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING | (multiple ? MF_GRAYED : 0), NavigationPopupRunScript, FbeLoadRuntimeStringByKey(L"fbe.document_tree.scripts.run", L"Run"));
		menu.AppendMenu(MF_SEPARATOR);
		m_script_popup_favorite_label = FbeLoadRuntimeStringByKey(allFavorites ? L"fbe.document_tree.scripts.remove_favorite" : L"fbe.document_tree.scripts.add_favorite",
			allFavorites ? L"★ Remove from favorites" : L"★ Add to favorites");
		menu.AppendMenu(MF_STRING, NavigationPopupToggleFavorite, m_script_popup_favorite_label);
		CMenu toolbars; toolbars.CreatePopupMenu();
		for(size_t index = 0; index < m_script_toolbars.size(); ++index)
			toolbars.AppendMenu(MF_STRING, NavigationPopupAddToolbarBase + static_cast<UINT>(index), m_script_toolbars[index].name);
		if(m_script_toolbars.empty()) toolbars.AppendMenu(MF_STRING | MF_GRAYED, static_cast<UINT_PTR>(0), FbeLoadRuntimeStringByKey(L"fbe.document_tree.scripts.no_toolbars", L"No script toolbars"));
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(static_cast<HMENU>(toolbars)), FbeLoadRuntimeStringByKey(L"fbe.document_tree.scripts.add_to_toolbar", L"Add to toolbar >"));
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING | (multiple ? MF_GRAYED : 0), NavigationPopupOpenLocation, FbeLoadRuntimeStringByKey(L"fbe.document_tree.scripts.open_location", L"Open file location"));
		const UINT command = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, point.x, point.y, m_hWnd);
		ExecuteScriptPopupCommand(command);
		return 1;
	}
	CPoint ptMousePos = (CPoint)lParam;
		
	// i	f Shift-F10
	if (ptMousePos.x == -1 && ptMousePos.y == -1)
	{
		ptMousePos = (CPoint)GetMessagePos();		
	}

	ScreenToClient(&ptMousePos);

	UINT uFlags;
	CTreeItem htItem(HitTest( ptMousePos, &uFlags ), this);

	if( htItem.IsNull() )
		return 0;

	//m_hActiveItem = htItem;

	HMENU menu;
	HMENU pPopup;

	// the font popup is stored in a resource
	menu = ::LoadMenu(_Module.GetResourceInstance(), MAKEINTRESOURCEW(IDR_DOCUMENT_TREE));
	pPopup = ::GetSubMenu(menu, 0);
	ApplyRuntimeDocumentTreeMenuLocalization(pPopup);
	ClientToScreen(&ptMousePos);
	::TrackPopupMenu(pPopup, TPM_LEFTALIGN, ptMousePos.x, ptMousePos.y, 0, *this, 0);

	return 1;
}

void CTreeView::SetScriptCatalog(const std::vector<ScriptDescriptor>& items, const std::vector<ScriptTreeVisual>& visuals, const std::vector<ScriptTreeToolbarTarget>& toolbars,
	const std::function<bool(const std::vector<CString>&, const CString&)>& addToToolbar,
	const std::function<void(const CString&)>& openLocation,
	const std::function<void(UINT)>& runScript)
{
	if(m_script_mode && m_script_filter.IsEmpty()) CaptureScriptExpansions();
	LoadFavoriteScripts();
	const bool refreshImages = m_script_images.size() != items.size() || !ScriptVisualsMatch(m_script_visuals, visuals, items.size());
	m_script_items = items; m_script_visuals = visuals; m_script_toolbars = toolbars; m_add_scripts_to_toolbar = addToToolbar; m_open_script_location = openLocation; m_run_script = runScript;
	bool rebound = false;
	for(size_t favorite = 0; favorite < m_favorite_scripts.size(); ++favorite)
	{
		const ScriptDescriptor* match = NULL;
		for(size_t index = 0; index < m_script_items.size(); ++index)
			if(!m_script_items[index].isFolder && m_script_items[index].uid == m_favorite_scripts[favorite].first) { match = &m_script_items[index]; break; }
		if(match == NULL && !m_favorite_scripts[favorite].second.IsEmpty())
			for(size_t index = 0; index < m_script_items.size(); ++index)
				if(!m_script_items[index].isFolder && !m_script_items[index].uid.IsEmpty() &&
					m_script_items[index].relativePath.CompareNoCase(m_favorite_scripts[favorite].second) == 0) { match = &m_script_items[index]; break; }
		if(match != NULL && (m_favorite_scripts[favorite].first != match->uid || m_favorite_scripts[favorite].second != match->relativePath))
		{ m_favorite_scripts[favorite] = std::make_pair(match->uid, match->relativePath); rebound = true; }
	}
	if(rebound) SaveFavoriteScripts();
	if(refreshImages)
	{
		PrepareScriptImages();
		// Recreating a CImageList changes its HIMAGELIST.  The native tree keeps
		// the previous handle until it is explicitly rebound, so refreshes while
		// Scripts mode is visible would otherwise leave item image indices backed
		// by a destroyed list.
		if(m_script_mode) SetImageList(m_scriptImageList,TVSIL_NORMAL);
	}
	if(m_script_mode) RebuildScriptTree();
}

void CTreeView::SetScriptToolbarTargets(const std::vector<ScriptTreeToolbarTarget>& toolbars) { m_script_toolbars = toolbars; }

void CTreeView::SetScriptFilter(const CString& filter)
{
	CString normalized(filter); normalized.Trim();
	if(normalized == m_script_filter) return;
	if(m_script_filter.IsEmpty() && !normalized.IsEmpty() && m_script_mode) CaptureScriptExpansions();
	m_script_filter = normalized;
	if(m_script_mode) RebuildScriptTree();
}

bool CTreeView::IsFavoriteScript(const CString& uid) const
{
	for(size_t index = 0; index < m_favorite_scripts.size(); ++index)
		if(m_favorite_scripts[index].first == uid) return true;
	return false;
}

HTREEITEM CTreeView::FavoriteScriptTreeItem(const CString& uid) const
{
	if(m_favorite_group == NULL) return NULL;
	for(std::map<HTREEITEM, size_t>::const_iterator item = m_script_nodes.begin(); item != m_script_nodes.end(); ++item)
		if(item->second < m_script_items.size() && m_script_items[item->second].uid == uid && GetParentItem(item->first) == m_favorite_group) return item->first;
	return NULL;
}

HTREEITEM CTreeView::FindScriptTreeItem(const CString& relativePath) const
{
	for(std::map<HTREEITEM, size_t>::const_iterator it = m_script_nodes.begin(); it != m_script_nodes.end(); ++it)
		if(it->second < m_script_items.size() && m_script_items[it->second].relativePath == relativePath &&
			(m_favorite_group == NULL || GetParentItem(it->first) != m_favorite_group)) return it->first;
	return NULL;
}

bool CTreeView::HasScriptTreeParent(const CString& relativePath, const CString& parentRelativePath) const
{
	const HTREEITEM item = FindScriptTreeItem(relativePath);
	const HTREEITEM parent = FindScriptTreeItem(parentRelativePath);
	return item != NULL && parent != NULL && GetParentItem(item) == parent;
}

bool CTreeView::HasScriptToolbarTarget(const CString& id, const CString& name) const
{
	for(size_t index = 0; index < m_script_toolbars.size(); ++index)
		if(m_script_toolbars[index].id == id && m_script_toolbars[index].name == name) return true;
	return false;
}

int CTreeView::ScriptTreeImage(HTREEITEM item) const
{
	int normal = 0, selected = 0;
	return item != NULL && GetItemImage(item, normal, selected) ? normal : -1;
}

bool CTreeView::GetScriptTreeMetrics(int& imageSize, int& itemHeight, int& indent, bool& legacyExpanders) const
{
	imageSize = 0;
	itemHeight = GetItemHeight();
	indent = GetIndent();
	legacyExpanders = m_script_mode;
	if(!m_scriptImageList.IsNull())
	{
		IMAGEINFO image = {};
		if(::ImageList_GetImageInfo(m_scriptImageList, 0, &image))
			imageSize = image.rcImage.right - image.rcImage.left;
	}
	return m_script_mode && imageSize > 0 && itemHeight >= imageSize && indent >= imageSize;
}

bool CTreeView::ExecuteScriptPopupCommand(UINT command)
{
	const std::vector<const ScriptDescriptor*> scripts = SelectedScripts();
	if(scripts.empty()) return false;
	if(command == NavigationPopupRunScript) { if(scripts.size() != 1) return false; RunSelectedScript(); return true; }
	if(command == NavigationPopupToggleFavorite)
	{
		const bool remove = SelectedScriptsAreAllFavorites();
		for(size_t index = 0; index < scripts.size(); ++index)
		{
			const ScriptDescriptor* script = scripts[index];
			if(remove)
			{
				for(std::vector<std::pair<CString, CString> >::iterator item = m_favorite_scripts.begin(); item != m_favorite_scripts.end(); ++item)
					if(item->first == script->uid) { m_favorite_scripts.erase(item); break; }
			}
			else if(!IsFavoriteScript(script->uid)) m_favorite_scripts.push_back(std::make_pair(script->uid, script->relativePath));
		}
		SaveFavoriteScripts(); RebuildScriptTree();
		return true;
	}
	if(command == NavigationPopupOpenLocation) { if(scripts.size() != 1) return false; if(m_open_script_location) m_open_script_location(scripts[0]->path); return true; }
	if(command >= NavigationPopupAddToolbarBase && command - NavigationPopupAddToolbarBase < m_script_toolbars.size())
	{
		std::vector<CString> uids;
		for(size_t index = 0; index < scripts.size(); ++index) uids.push_back(scripts[index]->uid);
		return m_add_scripts_to_toolbar && m_add_scripts_to_toolbar(uids, m_script_toolbars[command - NavigationPopupAddToolbarBase].id);
	}
	return false;
}

void CTreeView::SetScriptMode(bool value)
{
	if(m_script_mode == value) return;
	if(value && m_drag) EndDrag();
	m_script_mode = value;
	ApplyModeAppearance();
	if(m_script_mode) { SetImageList(m_scriptImageList,TVSIL_NORMAL); RebuildScriptTree(); }
	else { SetImageList(m_ImageList,TVSIL_NORMAL); UpdateAll(); }
}

void CTreeView::ApplyModeAppearance()
{
	if(!IsWindow()) return;
	if(m_script_mode)
	{
		// A disabled visual style makes the native tree render the familiar
		// square plus/minus expanders, independently of Explorer chevrons.
		::SetWindowTheme(m_hWnd, L" ", L" ");
		const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
		const int imageSize = ToolbarFactory::CommandToolbarImageSize(dpi);
		SetItemHeight(imageSize + UiMetrics::ScaleForDpi(8, dpi));
		SetIndent(imageSize + UiMetrics::ScaleForDpi(2, dpi));
	}
	else
	{
		::SetWindowTheme(m_hWnd, NULL, NULL);
		SetItemHeight(-1);
	}
	::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
}

void CTreeView::UpdateScriptDpiMetrics(UINT dpi)
{
	if(!IsWindow()) return;
	if(dpi == 0) dpi = UiMetrics::DpiForWindow(m_hWnd);
	const int size = ToolbarFactory::CommandToolbarImageSize(dpi);
	if(size != m_script_image_size)
	{
		PrepareScriptImages(dpi);
		if(m_script_mode) SetImageList(m_scriptImageList, TVSIL_NORMAL);
	}
	if(m_script_mode)
	{
		SetItemHeight(size + UiMetrics::ScaleForDpi(8, dpi));
		SetIndent(size + UiMetrics::ScaleForDpi(2, dpi));
	}
	::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
}

void CTreeView::RebuildScriptTree()
{
	m_current_item = NULL;
	DeleteAllItems(); m_script_nodes.clear(); m_source_index.clear(); m_favorite_group = NULL;
	for(size_t favorite = 0; favorite < m_favorite_scripts.size(); ++favorite)
		for(size_t index = 0; index < m_script_items.size(); ++index)
		{
			const ScriptDescriptor& script = m_script_items[index];
			if(script.isFolder || script.uid != m_favorite_scripts[favorite].first || !ScriptMatchesFilter(script)) continue;
			if(m_favorite_group == NULL) m_favorite_group = InsertItem(FbeLoadRuntimeStringByKey(L"fbe.document_tree.scripts.favorites", L"Favorites"), m_favorite_image, m_favorite_image, TVI_ROOT, TVI_FIRST);
			const int image = index < m_script_images.size() ? m_script_images[index] : 0;
			HTREEITEM item = InsertItem(script.name, image, image, m_favorite_group, TVI_LAST);
			m_script_nodes[item] = index;
			break;
		}
	if(m_favorite_group != NULL) Expand(m_favorite_group, TVE_EXPAND);
	// The pane caption already identifies Scripts.  Keep catalog descriptors at
	// TVI_ROOT so the tree does not add a redundant synthetic parent node.
	BuildScriptChildren(TVI_ROOT, CString());
}

void CTreeView::RefreshLocalizedScriptGroup()
{
	if(m_script_mode && m_favorite_group != NULL)
		SetItemText(m_favorite_group, FbeLoadRuntimeStringByKey(L"fbe.document_tree.scripts.favorites", L"Favorites"));
}

void CTreeView::CaptureScriptExpansions()
{
	m_expanded_script_paths.clear();
	for(std::map<HTREEITEM, size_t>::const_iterator node = m_script_nodes.begin(); node != m_script_nodes.end(); ++node)
		if(node->second < m_script_items.size() && m_script_items[node->second].isFolder && (GetItemState(node->first, TVIS_EXPANDED) & TVIS_EXPANDED))
			m_expanded_script_paths.insert(m_script_items[node->second].relativePath);
}

bool CTreeView::ScriptMatchesFilter(const ScriptDescriptor& script) const
{
	if(m_script_filter.IsEmpty()) return true;
	if(script.isFolder) return false;
	const int slash = script.relativePath.ReverseFind(L'/');
	const CString fileName = script.relativePath.Mid(slash + 1);
	CString probe(script.name + L"\n" + fileName); probe.MakeLower();
	CString query(m_script_filter); query.MakeLower();
	return probe.Find(query) >= 0;
}

bool CTreeView::ScriptOrDescendantMatches(const ScriptDescriptor& script) const
{
	if(!script.isFolder) return ScriptMatchesFilter(script);
	for(size_t index = 0; index < m_script_items.size(); ++index)
		if(m_script_items[index].parentId == script.id && ScriptOrDescendantMatches(m_script_items[index])) return true;
	return false;
}

void CTreeView::LoadFavoriteScripts()
{
	if(m_favorites_loaded) return;
	m_favorites_loaded = true;
	CString saved(_Settings.GetFavoriteScripts());
	int cursor = 0;
	while(cursor < saved.GetLength())
	{
		const int end = saved.Find(L'\n', cursor);
		CString row = saved.Mid(cursor, end < 0 ? saved.GetLength() - cursor : end - cursor);
		row.TrimRight(L"\r"); cursor = end < 0 ? saved.GetLength() : end + 1;
		const int tab = row.Find(L'\t');
		if(tab != 36 || row.GetLength() > 4096) continue;
		CString uid(row.Left(tab)), path(row.Mid(tab + 1));
		bool valid = true;
		for(int index = 0; index < uid.GetLength(); ++index)
			if((index == 8 || index == 13 || index == 18 || index == 23) ? uid[index] != L'-' : !iswxdigit(uid[index])) { valid = false; break; }
		if(valid && !IsFavoriteScript(uid)) m_favorite_scripts.push_back(std::make_pair(uid, path));
	}
}

void CTreeView::SaveFavoriteScripts()
{
	CString saved;
	for(size_t index = 0; index < m_favorite_scripts.size(); ++index)
		saved += m_favorite_scripts[index].first + L"\t" + m_favorite_scripts[index].second + L"\n";
	_Settings.SetFavoriteScripts(saved, true);
}

void CTreeView::PrepareScriptImages(UINT dpi)
{
	// Script sidecars do not share the legacy structural strip. Rebuild this
	// image list at the same DPI scale as the main command toolbar.
	m_script_image_size = ToolbarFactory::CommandToolbarImageSize(dpi ? dpi : UiMetrics::DpiForWindow(m_hWnd));
	m_scriptImageList.Destroy();
	m_scriptImageList.Create(m_script_image_size,m_script_image_size,ILC_COLOR32|ILC_MASK,static_cast<int>(m_script_items.size())+2,8);
	HBITMAP fallback = CreateScriptTreeGlyphBitmap(false, m_script_image_size);
	if(fallback != NULL) { AddScriptImage(fallback); ::DeleteObject(fallback); }
	HBITMAP favorite = CreateScriptTreeGlyphBitmap(true, m_script_image_size);
	m_favorite_image = favorite != NULL ? AddScriptImage(favorite) : -1;
	if(m_favorite_image != kFavoriteScriptImageIndex) m_favorite_image = -1;
	if(favorite != NULL) ::DeleteObject(favorite);
	m_script_images.assign(m_script_items.size(), 0);
	for(size_t index = 0; index < m_script_items.size() && index < m_script_visuals.size(); ++index)
	{
		const ScriptTreeVisual& visual = m_script_visuals[index];
		const ScriptDescriptor& script = m_script_items[index];
		CString sidecar(script.path);
		while(!sidecar.IsEmpty() && (sidecar[sidecar.GetLength() - 1] == L'\\' || sidecar[sidecar.GetLength() - 1] == L'/'))
			sidecar.Delete(sidecar.GetLength() - 1);
		if(!script.isFolder && sidecar.GetLength() >= 3) sidecar.Delete(sidecar.GetLength() - 3, 3);
		sidecar += L".ico";
		if(visual.bitmap != NULL)
		{
			BITMAP sourceSize = {};
			HICON largerSource = NULL;
			if(::GetObject(visual.bitmap, sizeof(sourceSize), &sourceSize) == sizeof(sourceSize) &&
				(sourceSize.bmWidth < m_script_image_size || sourceSize.bmHeight < m_script_image_size) &&
				::GetFileAttributes(sidecar) != INVALID_FILE_ATTRIBUTES)
				largerSource = static_cast<HICON>(::LoadImage(NULL, sidecar, IMAGE_ICON, m_script_image_size, m_script_image_size, LR_LOADFROMFILE));
			m_script_images[index] = largerSource != NULL ? AddScriptIcon(largerSource) : AddScriptImage(visual.bitmap);
			if(largerSource != NULL) ::DestroyIcon(largerSource);
		}
		else if(visual.icon != NULL)
		{
			HICON source = NULL;
			if(::GetFileAttributes(sidecar) != INVALID_FILE_ATTRIBUTES)
				source = static_cast<HICON>(::LoadImage(NULL, sidecar, IMAGE_ICON, m_script_image_size, m_script_image_size, LR_LOADFROMFILE));
			if(source == NULL && ::GetFileAttributes(script.path) != INVALID_FILE_ATTRIBUTES)
			{
				SHFILEINFOW shell = {};
				const DWORD attributes = script.isFolder ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
				if(::SHGetFileInfoW(script.isFolder ? L"folder" : L"script.js", attributes, &shell, sizeof(shell),
					SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES))
				{
					IImageList* images = NULL;
					const int kind = m_script_image_size > 32 ? SHIL_EXTRALARGE : SHIL_LARGE;
					if(SUCCEEDED(::SHGetImageList(kind, IID_IImageList, reinterpret_cast<void**>(&images))))
					{
						images->GetIcon(shell.iIcon, ILD_TRANSPARENT, &source);
						images->Release();
					}
				}
			}
			m_script_images[index] = AddScriptIcon(source != NULL ? source : visual.icon);
			if(source != NULL) ::DestroyIcon(source);
		}
		if(m_script_images[index] < 0) m_script_images[index] = 0;
	}
}

int CTreeView::AddScriptImage(HBITMAP bitmap)
{
	if(bitmap == NULL) return -1;
	BITMAP source = {};
	if(::GetObject(bitmap, sizeof(source), &source) != sizeof(source) || source.bmWidth <= 0 || source.bmHeight <= 0 ||
		source.bmWidth > 8192 || source.bmHeight > 8192) return -1;
	if(source.bmWidth == m_script_image_size && source.bmHeight == m_script_image_size)
		return m_scriptImageList.Add(bitmap, RGB(255, 0, 255));
	// Keep small bitmap sidecars at their native resolution. Upscaling a 16px
	// drawing to the tree's DPI cell produces an indistinct icon.
	const int width = (std::min)(static_cast<int>(source.bmWidth), m_script_image_size);
	const int height = (std::min)(static_cast<int>(source.bmHeight), m_script_image_size);
	BITMAPINFO sourceInfo = {}; sourceInfo.bmiHeader.biSize = sizeof(sourceInfo.bmiHeader);
	sourceInfo.bmiHeader.biWidth = source.bmWidth; sourceInfo.bmiHeader.biHeight = -source.bmHeight;
	sourceInfo.bmiHeader.biPlanes = 1; sourceInfo.bmiHeader.biBitCount = 32; sourceInfo.bmiHeader.biCompression = BI_RGB;
	std::vector<DWORD> sourcePixels(static_cast<size_t>(source.bmWidth) * source.bmHeight);
	HDC screen = ::GetDC(NULL);
	const int lines = screen != NULL ? ::GetDIBits(screen, bitmap, 0, source.bmHeight,
		sourcePixels.data(), &sourceInfo, DIB_RGB_COLORS) : 0;
	if(screen != NULL) ::ReleaseDC(NULL, screen);
	if(lines != source.bmHeight) return -1;
	bool hasAlpha = false;
	for(size_t index = 0; index < sourcePixels.size(); ++index)
		if((sourcePixels[index] & 0xff000000u) != 0) { hasAlpha = true; break; }
	BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = m_script_image_size; info.bmiHeader.biHeight = -m_script_image_size;
	info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
	void* pixels = NULL;
	HBITMAP normalized = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
	if(normalized == NULL || pixels == NULL) { if(normalized != NULL) ::DeleteObject(normalized); return -1; }
	DWORD* imagePixels = static_cast<DWORD*>(pixels);
	for(int index = 0; index < m_script_image_size * m_script_image_size; ++index)
		imagePixels[index] = RGB(255, 0, 255);
	for(int y = 0; y < height; ++y) for(int x = 0; x < width; ++x)
	{
		const DWORD pixel = sourcePixels[static_cast<size_t>(y * source.bmHeight / height) * source.bmWidth +
			static_cast<size_t>(x * source.bmWidth / width)];
		if((pixel & 0x00ffffffu) == RGB(255, 0, 255)) continue;
		imagePixels[(y + (m_script_image_size - height) / 2) * m_script_image_size + x + (m_script_image_size - width) / 2] =
			hasAlpha ? pixel : pixel | 0xff000000u;
	}
	const int image = m_scriptImageList.Add(normalized, RGB(255,0,255));
	::DeleteObject(normalized);
	return image;
}

int CTreeView::AddScriptIcon(HICON icon)
{
	if(icon == NULL) return -1;
	HICON normalized = static_cast<HICON>(::CopyImage(icon, IMAGE_ICON, m_script_image_size, m_script_image_size, 0));
	if(normalized == NULL) return -1;
	const int image = m_scriptImageList.AddIcon(normalized);
	::DestroyIcon(normalized);
	return image;
}

void CTreeView::BuildScriptChildren(HTREEITEM parent, const CString& parentId)
{
	for(size_t index = 0; index < m_script_items.size(); ++index)
	{
		const ScriptDescriptor& script = m_script_items[index];
		if(script.parentId != parentId) continue;
		if(!ScriptOrDescendantMatches(script)) continue;
		const int image = index < m_script_images.size() ? m_script_images[index] : 0;
		CTreeItem item = InsertItem(script.name, image, image, parent, TVI_LAST);
		m_script_nodes[item] = index;
		if(script.isFolder)
		{
			BuildScriptChildren(item, script.id);
			if(m_script_filter.IsEmpty() ? m_expanded_script_paths.find(script.relativePath) != m_expanded_script_paths.end() : true) Expand(item, TVE_EXPAND);
		}
	}
}

const ScriptDescriptor* CTreeView::SelectedScript()
{
	std::map<HTREEITEM, size_t>::const_iterator found = m_script_nodes.find(GetSelectedItem());
	return found == m_script_nodes.end() || found->second >= m_script_items.size() ? NULL : &m_script_items[found->second];
}

std::vector<const ScriptDescriptor*> CTreeView::SelectedScripts() const
{
	std::vector<const ScriptDescriptor*> scripts;
	std::set<CString> seen;
	std::function<void(HTREEITEM)> visit = [&](HTREEITEM item) {
		for(; item != NULL; item = TreeView_GetNextSibling(m_hWnd, item))
		{
			const std::map<HTREEITEM, size_t>::const_iterator found = m_script_nodes.find(item);
			if((GetItemState(item, TVIS_SELECTED) & TVIS_SELECTED) && found != m_script_nodes.end() && found->second < m_script_items.size())
			{
				const ScriptDescriptor& script = m_script_items[found->second];
				if(!script.isFolder && script.commandId > 0 && !script.uid.IsEmpty() && seen.insert(script.uid).second) scripts.push_back(&script);
			}
			visit(TreeView_GetChild(m_hWnd, item));
		}
	};
	visit(TreeView_GetRoot(m_hWnd));
	return scripts;
}

void CTreeView::SelectScriptContextItem(HTREEITEM item)
{
	if(item == NULL || (GetItemState(item, TVIS_SELECTED) & TVIS_SELECTED)) return;
	ClearSelection();
	SelectItem(item);
}

bool CTreeView::SelectedScriptsAreAllFavorites() const
{
	const std::vector<const ScriptDescriptor*> scripts = SelectedScripts();
	if(scripts.empty()) return false;
	for(size_t index = 0; index < scripts.size(); ++index)
		if(!IsFavoriteScript(scripts[index]->uid)) return false;
	return true;
}

void CTreeView::RunSelectedScript()
{
	const std::vector<const ScriptDescriptor*> scripts = SelectedScripts();
	if(scripts.size() == 1)
	{
		const ScriptDescriptor* script = scripts[0];
		m_run_script ? m_run_script(script->commandId) : ::SendMessage(m_main_window, WM_COMMAND, MAKEWPARAM(ID_SCRIPT_BASE + script->commandId, 0), 0);
	}
}


bool CTreeView::IsDropChangePosition(UINT flags, HTREEITEM hitem)
{
	if(hitem != m_move_to.m_hTreeItem)
		return true;

	if(flags & TVHT_ONITEMICON) 
		if(m_insert_type != CTreeView::sibling)
			return true;
		else
			return false;

	if(flags & TVHT_ONITEMLABEL) 
		if (m_insert_type != CTreeView::child)
			return true;
		else
			return false;

	if (m_insert_type != CTreeView::none)
		return true;
	else
		return false;
}

bool CTreeView::IsParent(CTreeItem	parent, CTreeItem child)
{
	if(parent == child)
		return true;
	while((child = child.GetParent()) && !child.IsNull())
	{
        if(parent == child)
			return true;
	}
	return false;
}

bool CTreeView::IsSibling(CTreeItem	item, CTreeItem	sibling)
{
	return (IsOneOfNextSibling(item, sibling) || IsOneOfPrevSibling(item, sibling));
}

bool CTreeView::IsOneOfNextSibling(CTreeItem	item, CTreeItem	sibling)
{
	if(item == sibling)
		return true;
	while((sibling = sibling.GetNextSibling()) && !sibling.IsNull())
	{
        if(item == sibling)
			return true;
	}
	return false;
}
bool CTreeView::IsOneOfPrevSibling(CTreeItem	item, CTreeItem	sibling)
{
	if(item == sibling)
		return true;
	while((sibling = sibling.GetPrevSibling()), !sibling.IsNull())
	{
        if(item == sibling)
			return true;
	}
	return false;
}

void CTreeView::EndDrag()
{
	ImageList_DragLeave(*this);
	ImageList_EndDrag();
	ReleaseCapture();  	
    
	SetItemImage(m_move_to, m_drop_item_nimage, m_drop_item_nimage);
	SelectDropTarget(NULL);
	m_drag = false;	        			
}

LRESULT CTreeView::OnCut(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	// remove last selection
	SetItemState(m_move_from, 0, TVIS_CUT);
	HTREEITEM hitem = GetSelectedItem();
	m_move_from = hitem;
	SetItemState(hitem, TVIS_CUT, TVIS_CUT);	
	return 0;	
}
LRESULT CTreeView::OnPaste(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	// remove selection
	SetItemState(m_move_from, 0, TVIS_CUT);
	m_move_to = GetSelectedItem();
	m_insert_type = CTreeView::child;
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_ELEMENT),(LPARAM)m_hWnd);
	return 0;
}
LRESULT CTreeView::OnView(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_VIEW_ELEMENT),(LPARAM)m_hWnd);
	return 0;
}

LRESULT CTreeView::OnViewSource(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_VIEW_ELEMENT_SOURCE),(LPARAM)m_hWnd);
	return 0;
}

LRESULT CTreeView::OnDelete(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_DELETE_ELEMENT),(LPARAM)m_hWnd);
	return 0;
}

LRESULT CTreeView::OnRight(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	//m_move_from = GetSelectedItem();
	
	// ???? ????????? ??????? ?????? ?????? ????????
	HTREEITEM from = GetSelectedItem();
	HTREEITEM item = GetSelectedItem();// = TreeView_GetNextSibling(*this, m_move_from);
	HTREEITEM prevItem = 0;
	item = TreeView_GetNextSibling(*this, item);
	while(item)
	{
		prevItem = item;
		item = TreeView_GetNextSibling(*this, item);
	}

	item = prevItem;

	m_move_to = GetSelectedItem();
	m_insert_type = CTreeView::child;
	item = TreeView_GetPrevSibling(*this, item);
	while(item)
	{
		if(prevItem == from)
			break;

		m_move_from = prevItem;
		::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_ELEMENT),(LPARAM)m_hWnd);
		prevItem = item;
		item = TreeView_GetPrevSibling(*this, item);
	}

	m_move_from = from;
	m_move_to = TreeView_GetPrevSibling(*this, m_move_from);

	if(m_move_to)
	{
		item = TreeView_GetPrevSibling(*this, m_move_from);
		if(!item || ! m_move_from)
			return 0;

		if(TreeView_GetChild(*this, item))
		{
			item = TreeView_GetChild(*this, item);
			while (item)
			{		
				m_move_to = item;
				item = TreeView_GetNextSibling(*this, item);		
			}
			m_insert_type = CTreeView::sibling;
		}
		else
		{
			m_move_to = item;
			m_insert_type = CTreeView::child;
		}
	}
	else
	{

	}
	
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_ELEMENT),(LPARAM)m_hWnd);
	return 0;
}

LRESULT CTreeView::OnRightOne(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_ELEMENT_ONE),(LPARAM)m_hWnd);
	/*HTREEITEM item = GetFirstSelectedItem();
	if(!item)
		return 0;

	do
	{
		MoveRightOne(item);		
	}while(item = GetNextSelectedItem(item));*/
	
	return 0;
}


LRESULT CTreeView::OnRightSmart(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_ELEMENT_SMART),(LPARAM)m_hWnd);
	return 0;
}

LRESULT CTreeView::OnLeftOne(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_LEFT_ONE),(LPARAM)m_hWnd);
	return 0;
}

LRESULT CTreeView::OnLeftWithChildren(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_LEFT),(LPARAM)m_hWnd);
	return 0;
}
LRESULT CTreeView::OnMerge(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	::SendMessage(m_main_window, WM_COMMAND, TreeCommandWParam(IDN_TREE_MERGE), (LPARAM)m_hWnd);
	return 0;
}

LRESULT CTreeView::OnLeft(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_script_mode) return 0;
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_LEFT),(LPARAM)m_hWnd);
	/*m_move_from = GetSelectedItem();
	m_move_to = TreeView_GetParent(*this, m_move_from);
	m_insert_type = InsertType::sibling;
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_ELEMENT),(LPARAM)m_hWnd);*/
	return 0;
}

bool CTreeView::SetMultiSelection(UINT nFlags, CPoint point)
{
	// Set focus to control if key strokes are needed.
	// Focus is not automatically given to control on lbuttondown

	//m_dwDragStart = GetTickCount();
	
	UINT flag;
	HTREEITEM hItem = HitTest( point, &flag );

	if(nFlags & MK_SHIFT)
	{
		// Shift key is down	

		// Initialize the reference item if this is the first shift selection
		if( !m_hItemFirstSel )
			m_hItemFirstSel = GetSelectedItem();

		// Select new item
		if( GetSelectedItem() == hItem )
			SelectItem( NULL );			// to prevent edit
//		OnLButtonDown(nFlags, point);

		if( m_hItemFirstSel )
		{
			SelectItems( m_hItemFirstSel, hItem, !(BOOL)(nFlags & MK_CONTROL));
			return true;
		}
	}
	else if(nFlags & MK_CONTROL )
	{
		// Control key is down
		if( hItem )
		{			
			// Toggle selection state
			UINT uNewSelState =
				GetItemState(hItem, TVIS_SELECTED) & TVIS_SELECTED ?
							0 : TVIS_SELECTED;

			// The native caret is single-select.  Keep the complete logical
			// selection before moving it and restore every selected item below.
			std::vector<HTREEITEM> selectedItems;
			for(CTreeItem selected(GetRootItem(), this); !selected.IsNull(); selected = GetNextItem(selected))
				if(GetItemState(selected, TVIS_SELECTED) & TVIS_SELECTED) selectedItems.push_back(selected);

			// Select new item
			if( GetSelectedItem() == hItem )
			{
				SelectItem( NULL );		// to prevent edit				
			}
//			OnLButtonDown(nFlags, point);

			// Set proper selection (highlight) state for new item
			SelectItem(hItem);
			SetItemState(hItem, uNewSelState,  TVIS_SELECTED);

			// Restore every pre-existing selection, except an item toggled off.
			for(size_t index = 0; index < selectedItems.size(); ++index)
				if(selectedItems[index] != hItem || uNewSelState != 0)
					SetItemState(selectedItems[index], TVIS_SELECTED, TVIS_SELECTED);

			m_hItemFirstSel = NULL;

			return true;
		}
	}
	else
	{
		// Normal - remove all selection and let default 
		// handler do the rest
		ClearSelection();
		//SelectItem(hItem);
		if(flag & TVHT_ONITEMICON || flag & TVHT_ONITEMLABEL)
			TreeView_SetItemState(*this, hItem, TVIS_SELECTED, TVIS_SELECTED);
		m_hItemFirstSel = NULL;
	}
	//		OnLButtonDown(nFlags, point);
	return false;
}

void CTreeView::ClearSelection()
{
	// This can be time consuming for very large trees 
	// and is called every time the user does a normal selection
	// If performance is an issue, it may be better to maintain 
	// a list of selected items
	for (CTreeItem hItem(GetRootItem(), this); hItem!=NULL; hItem=GetNextItem(hItem))
		if ( GetItemState( hItem, TVIS_SELECTED ) & TVIS_SELECTED )
			SetItemState( hItem, 0, TVIS_SELECTED );
}

void CTreeView::SelectAllVisibleItems()
{
	ClearSelection();
	HTREEITEM first = GetRootItem();
	for(HTREEITEM item = first; item != NULL; item = GetNextVisibleItem(item))
		SetItemState(item, TVIS_SELECTED, TVIS_SELECTED);
	if(first != NULL) SelectItem(first);
	// TreeView has one native caret item; preserve the complete logical
	// selection after moving that caret to the first visible item.
	for(HTREEITEM item = first; item != NULL; item = GetNextVisibleItem(item))
		SetItemState(item, TVIS_SELECTED, TVIS_SELECTED);
	m_hItemFirstSel = first;
}

// SelectItems	- Selects items from hItemFrom to hItemTo. Does not
//		- select child item if parent is collapsed. Removes
//		- selection from all other items
// hItemFrom	- item to start selecting from
// hItemTo	- item to end selection at.
BOOL CTreeView::SelectItems(HTREEITEM hItemFrom, HTREEITEM hItemTo, bool clearPrevSelection)
{
	HTREEITEM hItem = GetRootItem();

	// Clear selection upto the first item
	while ( hItem && hItem!=hItemFrom && hItem!=hItemTo )
	{
		hItem = GetNextVisibleItem( hItem );
		if(clearPrevSelection)
		{
			SetItemState( hItem, 0, TVIS_SELECTED );
		}
	}	

	if ( !hItem )
		return FALSE;	// Item is not visible

	SelectItem( hItemTo );

	// Rearrange hItemFrom and hItemTo so that hItemFirst is at top
	if( hItem == hItemTo )
	{
		hItemTo = hItemFrom;
		hItemFrom = hItem;
	}


	// Go through remaining visible items
	BOOL bSelect = TRUE;
	while ( hItem )
	{
		// Select or remove selection depending on whether item
		// is still within the range.
		SetItemState( hItem, bSelect ? TVIS_SELECTED : 0, TVIS_SELECTED );

		// Do we need to start removing items from selection
		if( hItem == hItemTo )
			bSelect = FALSE;

		hItem = GetNextVisibleItem( hItem );
	}

	return TRUE;
}

CTreeItem CTreeView::GetFirstSelectedItem()
{
	for ( CTreeItem hItem(GetRootItem(), this); !hItem.IsNull(); hItem = GetNextItem( hItem ) )
		if ( GetItemState( hItem, TVIS_SELECTED ) & TVIS_SELECTED )
			return hItem;

	return NULL;
}

CTreeItem CTreeView::GetLastSelectedItem()
{
	CTreeItem ret;
	for ( CTreeItem hItem(GetRootItem(), this); !hItem.IsNull(); hItem = GetNextItem( hItem ) )
		if ( GetItemState( hItem, TVIS_SELECTED ) & TVIS_SELECTED )
		{
			ret = hItem;
		}

	return ret;
}

CTreeItem CTreeView::GetNextSelectedItem( CTreeItem hItem )
{
	for ( hItem = GetNextItem( hItem ); !hItem.IsNull(); hItem = GetNextItem( hItem ) )
		if ( GetItemState( hItem, TVIS_SELECTED ) & TVIS_SELECTED )
			return hItem;

	return NULL;
}

CTreeItem CTreeView::GetPrevSelectedItem( CTreeItem hItem )
{
	for ( hItem = GetPrevItem( hItem ); !hItem.IsNull(); hItem = GetPrevItem( hItem ) )
		if ( GetItemState( hItem, TVIS_SELECTED ) & TVIS_SELECTED )
			return hItem;

	return NULL;
}

CTreeItem CTreeView::GetNextItem(CTreeItem hitem)
{
	//????????? ???? ?? child
	CTreeItem child = hitem.GetChild();
	if(!child.IsNull())
		return child;

	// ????????? ???? ?? sibling
	CTreeItem sibling = hitem.GetNextSibling();
	if(!sibling.IsNull())
		return sibling;
	
	// ???? ?????? sibling parent'?
	CTreeItem parent = hitem;
	while(parent = parent.GetParent())
	{
		CTreeItem parent_sibling = parent.GetNextSibling();
		if(parent_sibling)
		{
			return parent_sibling;
		}
	}

	return 0;
}

CTreeItem CTreeView::GetPrevItem(CTreeItem hitem)
{
	// ????????? ???? ?? prev sibling
	CTreeItem sibling = hitem.GetPrevSibling();
	if(!sibling.IsNull())
	{
		// ?????????? ?????? ?????????? ??????
		CTreeItem next_item = sibling;
		CTreeItem ret = sibling;
		while(next_item = GetNextItem(next_item))
		{
			if(next_item == hitem)
				return ret;

			ret = next_item;
		}
		return 0;
	}

	// ????????? ???? ?? parent
	CTreeItem parent = hitem.GetParent();
	if(!parent.IsNull())
	{
		return parent;
	}

	return 0;
}

/*bool CTreeView::MoveRightOne(HTREEITEM item)
{
	// ?????? ???? ???????? ?????? ??????????? ?????
	// ????? ???? ????? ????? ?????? ?????? ????????

	// ???? ?? ????? ??????????? ????, ?? ?? ?????? ??????
	HTREEITEM prev_sibling = GetPrevSiblingItem(item);
	HTREEITEM child = GetChildItem(prev_sibling);
	if(!prev_sibling)
		return false;

	// ?????? ???? ????????? ???????? ?????? ??????????? ?????
	if(!child)
	{
		m_move_to = prev_sibling;
		m_insert_type = InsertType::child;
	}
	else
	{
		m_move_to = GetLastSiblingItem(child);
		m_insert_type = InsertType::sibling;
	}
	m_move_from = item;		
	::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_ELEMENT),(LPARAM)m_hWnd);

	HTREEITEM parent = GetParentItem(item);
	HTREEITEM nextSibling = GetNextSiblingItem(item);		
	
	child = GetChildItem(item);
	if(child)
	{
		m_move_to = item;
		m_insert_type = InsertType::sibling;
		HTREEITEM last_child = GetLastSiblingItem(child);
		HTREEITEM prev_child = 0;
		
        do
		{
			m_move_from = last_child;
			last_child = GetPrevSiblingItem(last_child);
			::SendMessage(m_main_window,WM_COMMAND,TreeCommandWParam(IDN_TREE_MOVE_ELEMENT),(LPARAM)m_hWnd);
		}while(last_child);		
	}
	
	return true;
}*/

CTreeItem CTreeView::GetLastSiblingItem(CTreeItem item)
{
	if(!item)
		return 0;

	CTreeItem ret = item;
	while(item = item.GetNextSibling())
	{
		ret = item;
	}
	return ret;
}

CTreeItem CTreeView::GetNextSelectedSibling(CTreeItem item)
{
	if(item.IsNull())
		return 0;

	CTreeItem next = GetNextSelectedItem(item);
	while(!next.IsNull())
	{
		if(IsSibling(item, next))
			return next;
		next = GetNextSelectedItem(next);
	}
	return 0;
}

void CTreeView::SelectElement(MSHTML::IHTMLElement *p)
{
	CTreeItem item = this->LocatePosition(p);
	SelectItem(item);
}

void CTreeView::ExpandElem(MSHTML::IHTMLElement *p, UINT mode)
{
	CTreeItem item = this->LocatePosition(p);
	Expand(item, mode);
}

void CTreeView::Collapse(CTreeItem item, int level2Collapse, bool mode)
{
	if(item.IsNull())
		item = GetRootItem();// root ? ??? ??? 0 - ?? ???????
    
	do
	{
		if(level2Collapse == 0)
		{
			item.Expand(mode ? TVE_EXPAND : TVE_COLLAPSE);
		}
		else
		{
			CTreeItem child = item.GetChild();
			if(!child.IsNull())
				Collapse(child, level2Collapse - 1, mode);
		}
		item = item.GetNextSibling();
	}while(!item.IsNull());
}

void CTreeView::FillEDMnr()
{
	/*m_bodyED = new CBodyED;
	m_sectionED = new CSectionED;
	m_imageED =new CImageED;
	m_poemED = new CPoemED;
	_EDMnr.AddElementDescriptor(m_bodyED);
	_EDMnr.AddElementDescriptor(m_sectionED);
	_EDMnr.AddElementDescriptor(m_imageED);
	_EDMnr.AddElementDescriptor(m_poemED);*/
}

int CTreeView::AddImage(HANDLE img)
{
	return m_ImageList.Add((HBITMAP)img);
}

int CTreeView::AddIcon(HANDLE icon)
{
	return m_ImageList.AddIcon((HICON)icon);
}
