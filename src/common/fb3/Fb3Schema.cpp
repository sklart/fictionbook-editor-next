#include "Fb3Schema.h"
#include "Fb3Package.h"

namespace FbeFb3 {
namespace {

CComBSTR ValidatePart(const Part& part, const CComVariant& cache) {
    const auto document = LoadXmlPart(part);
    CComPtr<IXMLDOMParseError> error;
    if (FAILED(document->putref_schemas(cache)) || FAILED(document->validate(&error)) || !error)
        throw Error{L"Cannot validate FB3 part " + part.name};
    long code = 0;
    if (FAILED(error->get_errorCode(&code))) throw Error{L"Cannot read FB3 schema result for " + part.name};
    if (code) {
        CComBSTR reason;
        error->get_reason(&reason);
        throw Error{L"FB3 schema validation failed for " + part.name + L": " + (reason ? std::wstring(reason, reason.Length()) : L"")};
    }
    CComPtr<IXMLDOMElement> root;
    CComVariant id;
    if (FAILED(document->get_documentElement(&root)) || !root || FAILED(root->getAttribute(CComBSTR(L"id"), &id)) || id.vt != VT_BSTR || !id.bstrVal)
        throw Error{L"No FB3 book ID in " + part.name};
    return CComBSTR(id.bstrVal);
}

}

void ValidatePackageSchemas(const Package& package, const std::filesystem::path& schemaDirectory) {
    const auto directory = std::filesystem::absolute(schemaDirectory);
    CComPtr<IXMLDOMSchemaCollection2> schemas;
    if (FAILED(schemas.CoCreateInstance(__uuidof(XMLSchemaCache60)))) throw Error{L"Cannot create the FB3 schema cache."};
    // Body imports the description and link schemas through distributed relative paths.
    const auto bodySchema = directory / L"fb3_body.xsd";
    const auto descriptionSchema = directory / L"fb3_descr.xsd";
    if (FAILED(schemas->add(CComBSTR(BodyNamespace), CComVariant(bodySchema.c_str()))) ||
        FAILED(schemas->add(CComBSTR(DescriptionNamespace), CComVariant(descriptionSchema.c_str()))))
        throw Error{L"Cannot load the FictionBook 3 schemas from " + directory.wstring()};
    const CComVariant cache(static_cast<IDispatch*>(schemas));
    const auto description = ValidatePart(package.Description(), cache);
    const auto body = ValidatePart(package.Body(), cache);
    if (::CompareStringOrdinal(description, -1, body, -1, TRUE) != CSTR_EQUAL)
        throw Error{L"The FB3 description and body have different book IDs."};
}

}
