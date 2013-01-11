#include "precomp.hpp"
#include "nifxml.h"
#include "resource.h"

// #import <msxml2.dll> raw_interfaces_only, named_guids, no_smart_pointers
#import "./msxml.tlb" raw_interfaces_only, named_guids, no_smart_pointers

namespace
{

const BSTR NifNamespace = L"http://schemas.ximeta.com/ndas/2005/01/nif";

HRESULT
GetChildNodeText(
	IXMLDOMNode* pParentNode,
	BSTR NodePath, 
	BSTR* pbstrText)
{
	CComPtr<IXMLDOMNode> pNode;
	HRESULT hr = pParentNode->selectSingleNode(NodePath, &pNode);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);

	if (S_OK == hr)
	{
		hr = pNode->get_text(pbstrText);
	    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);
		return S_OK;
	}
	*pbstrText = CComBSTR(L"").Detach();

	return S_OK;
}

HRESULT
GetNifEntryFromNode(
	IXMLDOMNode* pParentNode,
	NifDeviceEntry& entry)
{
	HRESULT hr = GetChildNodeText(pParentNode, L"./name", &entry.Name);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);

	hr = GetChildNodeText(pParentNode, L"./id", &entry.DeviceId);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);

	hr = GetChildNodeText(pParentNode, L"./writeKey", &entry.WriteKey);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);

	hr = GetChildNodeText(pParentNode, L"./description", &entry.Description);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);

	return S_OK;
}

HRESULT
AppendTextNode(
	IXMLDOMDocument* pXmlDoc,
	IXMLDOMNode* pParentNode,
	BSTR NodeText)
{
	CComPtr<IXMLDOMText> pText;
	HRESULT hr = pXmlDoc->createTextNode(NodeText, &pText);
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	CComQIPtr<IXMLDOMNode> pNode = pText;
	ATLENSURE_RETURN_HR(pNode.p, E_NOINTERFACE);

	CComPtr<IXMLDOMNode> pNewNode;
	hr = pParentNode->appendChild(pText, &pNewNode);
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	return S_OK;
}

HRESULT
CreateElementNode(
	IXMLDOMDocument* pXmlDoc, 
	BSTR NodeName, 
	BSTR NodeText, 
	IXMLDOMNode** ppNode)
{
	ATLENSURE_RETURN_HR(ppNode, E_POINTER);
	*ppNode = NULL;

	CComPtr<IXMLDOMNode> pNode;
	HRESULT hr = pXmlDoc->createNode(
    	CComVariant(NODE_ELEMENT), NodeName, NifNamespace, &pNode);
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	hr = pNode->put_text(NodeText);
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	*ppNode = pNode.Detach();

	return S_OK;
}

HRESULT
AppendElementNode(
	IXMLDOMDocument* pXmlDoc,
	IXMLDOMNode* pParentNode,
	BSTR ElementName,
	BSTR ElementText)
{
	CComPtr<IXMLDOMNode> pNode;
	HRESULT hr = CreateElementNode(pXmlDoc, ElementName, ElementText, &pNode);
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	CComPtr<IXMLDOMNode> pNewNode;
	hr = pParentNode->appendChild(pNode, &pNewNode);
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	return S_OK;
}

HRESULT
CreateNodeFromNifEntry(
	IXMLDOMDocument* pXmlDoc, 
	const NifDeviceEntry& entry, 
	IXMLDOMNode** ppNode)
{
	ATLENSURE_RETURN_HR(ppNode, E_POINTER);
	*ppNode = NULL;

	CComPtr<IXMLDOMNode> pNode;
	HRESULT hr = pXmlDoc->createNode(
		CComVariant(NODE_ELEMENT), L"ndasDevice", NifNamespace, &pNode);
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	CComPtr<IXMLDOMNode> pChildNode;
	CComPtr<IXMLDOMNode> pAppendedChildNode;

	hr = AppendTextNode(pXmlDoc, pNode, L"\r\n");
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);
	
	hr = AppendElementNode(pXmlDoc, pNode, L"name", entry.Name);
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	hr = AppendTextNode(pXmlDoc, pNode, L"\r\n");
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	hr = AppendElementNode(pXmlDoc, pNode, L"id", entry.DeviceId);
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	hr = AppendTextNode(pXmlDoc, pNode, L"\r\n");
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	if (entry.WriteKey && entry.WriteKey[0] != 0)
	{
		hr = AppendElementNode(pXmlDoc, pNode, L"writeKey", entry.WriteKey);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

		hr = AppendTextNode(pXmlDoc, pNode, L"\r\n");
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);
	}

	if (entry.Description && entry.Description[0] != 0)
	{
		hr = AppendElementNode(pXmlDoc, pNode, L"description", entry.Description);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

		hr = AppendTextNode(pXmlDoc, pNode, L"\r\n");
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);
	}

	*ppNode = pNode.Detach();

	return S_OK;
}

HRESULT
AppendNifArrayToNode(
	IXMLDOMDocument* pXmlDoc,					 
	IXMLDOMNode* pParentNode,
	const NifDeviceArray& array)
{
	int len = array.GetSize();
	for (int i = 0; i < len; ++i)
	{
		const NifDeviceEntry& entry = array[i];

		CComPtr<IXMLDOMNode> pNode;
		HRESULT hr = CreateNodeFromNifEntry(pXmlDoc, entry, &pNode);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

		hr = AppendTextNode(pXmlDoc, pParentNode, L"\r\n");
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

		CComPtr<IXMLDOMNode> pNewNode;
		hr = pParentNode->appendChild(pNode, &pNewNode);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

		hr = AppendTextNode(pXmlDoc, pParentNode, L"\r\n");
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);
	}
	return S_OK;
}

HRESULT
GetArrayFromNifNodeList(
	IXMLDOMNodeList* pNodeList, 
	NifDeviceArray& array)
{
    for (CComPtr<IXMLDOMNode> pNode;
        S_OK == pNodeList->nextNode(&pNode);)
	{
		NifDeviceEntry entry;
		HRESULT hr = GetNifEntryFromNode(pNode, entry);
		if (SUCCEEDED(hr))
		{
			array.Add(entry);
		}
		pNode.Release();
	}
	return S_OK;
}

} // anonymous namespace 


NifXmlSerializer::
NifXmlSerializer()
{
}

NifXmlSerializer::
~NifXmlSerializer()
{
}

HRESULT
NifXmlSerializer::
Load(
	BSTR FileName,
	NifDeviceArray& array)
{
    CComPtr<IXMLDOMDocument> pXmlDoc;

	HRESULT hr = pXmlDoc.CoCreateInstance(L"MSXML2.DOMDocument", NULL, CLSCTX_INPROC_SERVER);
	if (FAILED(hr))
	{
		ATLTRACE("MSXML2.DOMDocument is not available, hr=%08X, Use MSXML.DOMDocument\n", hr);
		// ATLASSERT(REGDB_E_CLASSNOTREG == hr);
		// CLSID_DOMDocument
		hr = pXmlDoc.CoCreateInstance(L"MSXML.DOMDocument", NULL, CLSCTX_INPROC_SERVER);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);
	}

    hr = pXmlDoc->put_async(CComVariant(false).boolVal);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);

    CComVariant xmlSource = FileName;
    CComVariant isSuccessful = false;
    hr = pXmlDoc->load(xmlSource, &isSuccessful.boolVal);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);
    if (!isSuccessful.boolVal)
    {
        return E_FAIL;
    }

#ifdef _DEBUG
    CComBSTR bstrRawXml;
    hr = pXmlDoc->get_xml(&bstrRawXml);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);
	ATLTRACE(L"---RawXML---\n%s\n---RawXML---\n", bstrRawXml);
#endif

    CComPtr<IXMLDOMNodeList> pNodeList;
    hr = pXmlDoc->selectNodes(L"/nif/ndasDevice", &pNodeList);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);

	hr = GetArrayFromNifNodeList(pNodeList, array);
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

#ifdef _DEBUG
	{
		int size = array.GetSize();
		for (int i = 0; i < size; ++i)
		{
			ATLTRACE(L"Name:%s\n", array[i].Name);
			ATLTRACE(L"ID:%s\n", array[i].DeviceId);
			ATLTRACE(L"WriteKey:%s\n", array[i].WriteKey);
			ATLTRACE(L"Description:%s\n", array[i].Description);
		}
	}
#endif

    return hr;
}

HRESULT
NifXmlSerializer::
Save(
	BSTR FileName,
	const NifDeviceArray& Array)
{
    CComPtr<IXMLDOMDocument> pXmlDoc;
	HRESULT hr = pXmlDoc.CoCreateInstance(L"MSXML2.DOMDocument", NULL, CLSCTX_INPROC_SERVER);
	if (FAILED(hr))
	{
		ATLTRACE("MSXML2.DOMDocument is not available, hr=%08X, Use MSXML.DOMDocument\n", hr);
		// ATLASSERT(REGDB_E_CLASSNOTREG == hr);
		// CLSID_DOMDocument
		hr = pXmlDoc.CoCreateInstance(L"MSXML.DOMDocument", NULL, CLSCTX_INPROC_SERVER);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);
	}
    //HRESULT hr = pXmlDoc.CoCreateInstance(CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER);
    //ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);
    
    pXmlDoc->put_async(CComVariant(false).boolVal);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);

	CComPtr<IXMLDOMNode> pNewNode;

	CComBSTR Skeleton;
	ATLVERIFY(Skeleton.LoadString(IDS_NIF_SKELETON));
	
    CComVariant xmlSource = FileName;
    CComVariant isSuccessful = false;
    hr = pXmlDoc->loadXML(Skeleton, &isSuccessful.boolVal);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);
    if (!isSuccessful.boolVal)
    {
        return E_FAIL;
    }

    CComPtr<IXMLDOMNode> pNode;
    hr = pXmlDoc->selectSingleNode(L"/nif", &pNode);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);

	hr = AppendNifArrayToNode(pXmlDoc, pNode, Array);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);

#ifdef _DEBUG
    CComBSTR bstrRawXml;
    hr = pXmlDoc->get_xml(&bstrRawXml);
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);
	ATLTRACE(L"---RawXML---\n%s\n---RawXML---", bstrRawXml);
#endif

	hr = pXmlDoc->save(CComVariant(FileName));
    ATLENSURE_RETURN_HR(SUCCEEDED(hr),hr);
 
	return S_OK;
}

