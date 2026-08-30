#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>

#include "Fb2PropertyStoreCom.h"

#include <new>

namespace FBShellModern {

Fb2PropertyStoreCom::Fb2PropertyStoreCom() :
    m_referenceCount(1),
    m_initialized(false)
{
}

Fb2PropertyStoreCom::~Fb2PropertyStoreCom() = default;

HRESULT Fb2PropertyStoreCom::CreateInstance(REFIID riid, void** object)
{
    if (object == nullptr)
        return E_POINTER;

    *object = nullptr;

    Fb2PropertyStoreCom* instance = new (std::nothrow) Fb2PropertyStoreCom();
    if (instance == nullptr)
        return E_OUTOFMEMORY;

    const HRESULT hr = instance->QueryInterface(riid, object);
    instance->Release();
    return hr;
}

STDMETHODIMP Fb2PropertyStoreCom::QueryInterface(REFIID riid, void** object)
{
    if (object == nullptr)
        return E_POINTER;

    *object = nullptr;

    if (riid == IID_IUnknown || riid == IID_IPropertyStore)
        *object = static_cast<IPropertyStore*>(this);
    else if (riid == IID_IPropertyStoreCapabilities)
        *object = static_cast<IPropertyStoreCapabilities*>(this);
    else if (riid == IID_IInitializeWithStream)
        *object = static_cast<IInitializeWithStream*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) Fb2PropertyStoreCom::AddRef()
{
    return static_cast<ULONG>(::InterlockedIncrement(&m_referenceCount));
}

STDMETHODIMP_(ULONG) Fb2PropertyStoreCom::Release()
{
    const ULONG referenceCount = static_cast<ULONG>(::InterlockedDecrement(&m_referenceCount));
    if (referenceCount == 0)
        delete this;

    return referenceCount;
}

STDMETHODIMP Fb2PropertyStoreCom::GetCount(DWORD* propertyCount)
{
    if (propertyCount == nullptr)
        return E_POINTER;

    if (!m_initialized)
        return E_UNEXPECTED;

    *propertyCount = static_cast<DWORD>(m_cache.GetCount());
    return S_OK;
}

STDMETHODIMP Fb2PropertyStoreCom::GetAt(DWORD propertyIndex, PROPERTYKEY* key)
{
    if (key == nullptr)
        return E_POINTER;

    if (!m_initialized)
        return E_UNEXPECTED;

    const Fb2PropertyStoreCache::Entry* entry = m_cache.GetAt(static_cast<int>(propertyIndex));
    if (entry == nullptr)
        return E_INVALIDARG;

    *key = entry->propertyKey;
    return S_OK;
}

STDMETHODIMP Fb2PropertyStoreCom::GetValue(REFPROPERTYKEY key, PROPVARIANT* value)
{
    if (value == nullptr)
        return E_POINTER;

    if (!m_initialized)
        return E_UNEXPECTED;

    return m_cache.GetValue(key, *value);
}

STDMETHODIMP Fb2PropertyStoreCom::SetValue(REFPROPERTYKEY, REFPROPVARIANT)
{
    return STG_E_ACCESSDENIED;
}

STDMETHODIMP Fb2PropertyStoreCom::Commit()
{
    return S_OK;
}

STDMETHODIMP Fb2PropertyStoreCom::IsPropertyWritable(REFPROPERTYKEY)
{
    return S_FALSE;
}

STDMETHODIMP Fb2PropertyStoreCom::Initialize(IStream* stream, DWORD)
{
    if (stream == nullptr)
        return E_POINTER;

    if (m_initialized)
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

    const HRESULT hr = InitializeFromStream(stream);
    if (SUCCEEDED(hr))
        m_initialized = true;

    return hr;
}

HRESULT Fb2PropertyStoreCom::InitializeFromStream(IStream* stream)
{
    FB2Metadata::Metadata metadata;
    CString errorMessage;
    if (!FB2Metadata::TryReadStream(stream, metadata, &errorMessage))
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

    if (!m_cache.LoadFromMetadata(metadata))
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

    return S_OK;
}

} // namespace FBShellModern
