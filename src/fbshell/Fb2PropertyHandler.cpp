#include <windows.h>
#include <initguid.h>
#include <new>

#include "Fb2PropertyHandler.h"

// {D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}
DEFINE_GUID(CLSID_Fb2PropertyHandler,
0xd4a47f38, 0x1e5a, 0x4f0d, 0xb1, 0xc9, 0x6d, 0x2a, 0x4a, 0x6b, 0x1f, 0x42);

Fb2PropertyHandler::Fb2PropertyHandler() :
    m_store(nullptr)
{
}

HRESULT Fb2PropertyHandler::FinalConstruct()
{
    m_store = new (std::nothrow) FBShellModern::Fb2PropertyStoreCom();
    return m_store != nullptr ? S_OK : E_OUTOFMEMORY;
}

void Fb2PropertyHandler::FinalRelease()
{
    if (m_store != nullptr) {
        m_store->Release();
        m_store = nullptr;
    }
}

STDMETHODIMP Fb2PropertyHandler::GetCount(DWORD* propertyCount)
{
    return m_store != nullptr ? m_store->GetCount(propertyCount) : E_UNEXPECTED;
}

STDMETHODIMP Fb2PropertyHandler::GetAt(DWORD propertyIndex, PROPERTYKEY* key)
{
    return m_store != nullptr ? m_store->GetAt(propertyIndex, key) : E_UNEXPECTED;
}

STDMETHODIMP Fb2PropertyHandler::GetValue(REFPROPERTYKEY key, PROPVARIANT* value)
{
    return m_store != nullptr ? m_store->GetValue(key, value) : E_UNEXPECTED;
}

STDMETHODIMP Fb2PropertyHandler::SetValue(REFPROPERTYKEY key, REFPROPVARIANT value)
{
    return m_store != nullptr ? m_store->SetValue(key, value) : E_UNEXPECTED;
}

STDMETHODIMP Fb2PropertyHandler::Commit()
{
    return m_store != nullptr ? m_store->Commit() : E_UNEXPECTED;
}

STDMETHODIMP Fb2PropertyHandler::IsPropertyWritable(REFPROPERTYKEY key)
{
    return m_store != nullptr ? m_store->IsPropertyWritable(key) : E_UNEXPECTED;
}

STDMETHODIMP Fb2PropertyHandler::Initialize(IStream* stream, DWORD mode)
{
    return m_store != nullptr ? m_store->Initialize(stream, mode) : E_UNEXPECTED;
}
