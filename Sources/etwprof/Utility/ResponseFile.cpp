#include "ResponseFile.hpp"

#include <windows.h>

#include "Utility/Asserts.hpp"
#include "Utility/OnExit.hpp"

namespace ETWP {

namespace {

const wchar_t       kUTF16LEBOM = L'\xFEFF';
const wchar_t       kUTF16BEBOM = L'\xFFFE';
const unsigned char kUTF8BOM[] = { 0xEF, 0xBB, 0xBF };

constexpr DWORD kUTF16BOMNBytes = sizeof (wchar_t);
constexpr DWORD kUTF8BOMNBytes  = sizeof (kUTF8BOM);

constexpr size_t kMaxRespFileSize = 1'024 * 1'024;  // 1 MB
constexpr size_t kMinRespFileSize = 2;    // e.g. "-m" w/ ASCII or UTF-8

}   // namespace

namespace Impl {

class ResponseFileImpl {
public:
    class InitException : public Exception {
    public:
        InitException (const std::wstring& msg);
    };

    explicit ResponseFileImpl (const std::wstring& filePath);
    ~ResponseFileImpl ();

    bool Unpack (std::vector<std::wstring>* pArgumentsOut, std::wstring* pErrorOut);

    Encoding GetEncoding () const;

private:
    HANDLE   m_hFile;
    LONGLONG m_fileSize;

    Encoding m_encoding;
    size_t   m_bomSize;

    std::wstring m_path;

    std::unique_ptr<char[]> m_pRawContent;    // Raw content with \x00\x00 appended at the end

    bool IsValid (std::wstring* pErrorOut) const;

    Encoding GuessEncoding (size_t* pBOMSizeOut) const;

    std::vector<std::wstring> UnpackUTF16LE (size_t bytesToSkip) const;
    std::vector<std::wstring> UnpackUTF8 (size_t bytesToSkip) const;
};

ResponseFileImpl::InitException::InitException (const std::wstring& msg): Exception (msg)
{
}

ResponseFileImpl::ResponseFileImpl (const std::wstring& filePath):
    m_path (filePath)
{
    if (!PathValid (filePath))
        throw InitException (L"Response file path (" + filePath + L") is invalid!");

    if (!PathExists (filePath))
        throw InitException (L"Response file (" + filePath + L") does not exist!");

    if (!IsFile (filePath))
        throw InitException (L"Response file (" + filePath + L") is not a file!");

    m_hFile = CreateFileW (filePath.c_str (), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (m_hFile == INVALID_HANDLE_VALUE)
        throw InitException (L"Unable to open response file (" + filePath + L")!");

    OnExit handleCloserOnException ([&]() { CloseHandle (m_hFile); });

    LARGE_INTEGER fileSize;
    if (ETWP_ERROR (GetFileSizeEx (m_hFile, &fileSize) == FALSE))
        throw InitException (L"Unable to determine file size of response file (" + filePath + L")!");

    m_fileSize = fileSize.QuadPart;

    std::wstring error;
    if (!IsValid (&error))
        throw InitException (error);

    // We might work with m_pRawContent cast to wchar_t* later
    static_assert (__STDCPP_DEFAULT_NEW_ALIGNMENT__ >= alignof (wchar_t), "Unable to satisfy alignment requirement!");

    // We don't know the encoding of the file at this point, so we just slap a double null terminator (might be UTF-16)
    //   at the end...
    m_pRawContent.reset (new char[m_fileSize + sizeof L'\0']);
    m_pRawContent[m_fileSize] = '\0';
    m_pRawContent[m_fileSize + 1] = '\0';

    DWORD bytesRead = 0;
    if (ReadFile (m_hFile, m_pRawContent.get (), static_cast<DWORD> (m_fileSize), &bytesRead, nullptr) == FALSE ||
        bytesRead != m_fileSize)
    {
        throw InitException (L"Unable read contents of response file (" + filePath + L")!");
    }

    m_encoding = GuessEncoding (&m_bomSize);

    handleCloserOnException.Deactivate ();
}

ResponseFileImpl::~ResponseFileImpl ()
{
    CloseHandle (m_hFile);
}

bool ResponseFileImpl::IsValid (std::wstring* pErrorOut) const
{
    if (m_fileSize > kMaxRespFileSize) {
        *pErrorOut = L"Response file (" + m_path + L") exceeds maximum size of " +
            std::to_wstring (kMaxRespFileSize) + L" bytes!";

        return false;
    } else if (m_fileSize < kMinRespFileSize) {
        *pErrorOut = L"Response file (" + m_path + L") falls below the minimum size of " +
            std::to_wstring (kMinRespFileSize) + L" bytes!";

        return false;
    }

    return true;
}

Encoding ResponseFileImpl::GuessEncoding (size_t* pBOMSizeOut) const
{
    // Note: encoding blues. Even though the native encoding is UTF-16 on Windows, the "default" choice of both notepad
    //   and WordPad is ANSI (ACP). Other popular text editors (Sublime Text, Notepad++, VS Code) will default to UTF-8
    //   w/o BOM.
    //   Here's our "strategy": - In case of UTF-16, notepad/WordPad will place a BOM ==> we detect UTF-16 LE by the BOM
    //                          - If we detect a UTF-16 BE BOM, we back off
    //                          - If we detect a UTF-8 BOM ==> UTF-8 (obviously)
    //                          - If there is no BOM, we assume UTF-8. This way ANSI will only work if there are
    //                            only ASCII characters in the file, though...

    // Check for BOMs
    if (m_fileSize >= kUTF16BOMNBytes) {
        if (memcmp (m_pRawContent.get (), &kUTF16LEBOM, kUTF16BOMNBytes) == 0) {
            *pBOMSizeOut = kUTF16BOMNBytes;

            return Encoding::UTF16LE;
        } else if (memcmp (m_pRawContent.get (), &kUTF16BEBOM, kUTF16BOMNBytes) == 0) {
            *pBOMSizeOut = kUTF16BOMNBytes;

            return Encoding::UTF16BE;
        }
    }
    
    if (m_fileSize >= kUTF8BOMNBytes) {
        if (memcmp (m_pRawContent.get (), kUTF8BOM, kUTF8BOMNBytes) == 0) {
            *pBOMSizeOut = kUTF8BOMNBytes;

            return Encoding::UTF8;
        }
    }

    *pBOMSizeOut = 0;   // No BOM detected...

    return Encoding::UTF8;
}

std::vector<std::wstring> ResponseFileImpl::UnpackUTF16LE (size_t bytesToSkip) const
{
    const wchar_t* pContent = reinterpret_cast<const wchar_t*> (m_pRawContent.get () + bytesToSkip);
    int numArgs;
    LPWSTR* pArgs = CommandLineToArgvW (pContent, &numArgs);

    std::vector<std::wstring> result;
    for (int i = 0; i < numArgs; ++i)
        result.push_back (pArgs[i]);

    LocalFree (pArgs);

    return result;
}

std::vector<std::wstring> ResponseFileImpl::UnpackUTF8 (size_t bytesToSkip) const
{
    const char* pContent = m_pRawContent.get () + bytesToSkip;

    // Convert to UTF-16
    // First, determine the required buffer size, and allocate as such
    int bufferSize = MultiByteToWideChar (CP_UTF8, 0, pContent, -1, nullptr, 0);
    std::unique_ptr<wchar_t> pConvertedContent (new wchar_t[bufferSize]);

    // Second, do the actual conversion
    MultiByteToWideChar (CP_UTF8, 0, pContent, -1, pConvertedContent.get (), bufferSize);

    int numArgs;
    LPWSTR* pArgs = CommandLineToArgvW (pConvertedContent.get (), &numArgs);

    std::vector<std::wstring> result;
    for (int i = 0; i < numArgs; ++i)
        result.push_back (pArgs[i]);

    LocalFree (pArgs);

    return result;
}

bool ResponseFileImpl::Unpack (std::vector<std::wstring>* pArgumentsOut, std::wstring* pErrorOut)
{
    // If we detected UTF-16, the file size must be a multiple of sizeof(wchar_t)
    switch (m_encoding) {
        case Encoding::UTF16BE:
        case Encoding::UTF16LE:
            if (ETWP_ERROR ((m_fileSize % sizeof (wchar_t)) != 0)) {
                *pErrorOut = L"UTF-16 response file detected, but file size (" + std::to_wstring (m_fileSize) +
                    L" bytes) for file (" + m_path + L") is not a multiple of 2!";

                return false;
            }
    }
    
    switch (m_encoding) {
        case Encoding::ACP:
            *pErrorOut = L"ANSI-encoded (ACP) response files are unsupported (" + m_path + L")!";

            return false;
        case Encoding::UTF16BE:
            *pErrorOut = L"UTF-16 BE response files are unsupported (" + m_path + L")!";

            return false;
        case Encoding::UTF16LE:
            *pArgumentsOut = UnpackUTF16LE (m_bomSize);

            return true;
        case Encoding::UTF8:
            *pArgumentsOut = UnpackUTF8 (m_bomSize);

            return true;
        default:
            ETWP_DEBUG_BREAK_STR (L"Impossible Encoding value!");
            return false;
    }
}

Encoding ResponseFileImpl::GetEncoding () const
{
    return m_encoding;
}

}   // namespace Impl

ResponseFile::InitException::InitException (const std::wstring& msg) : Exception (msg)
{
}

ResponseFile::ResponseFile (const std::wstring& filePath)
{
    try {
        m_impl.reset (new Impl::ResponseFileImpl (filePath));
    } catch (const Impl::ResponseFileImpl::InitException& e) {
        throw InitException (e.GetMsg ());
    }
}

ResponseFile::~ResponseFile ()
{
}

bool ResponseFile::Unpack (std::vector<std::wstring>* pArgumentsOut, std::wstring* pErrorOut)
{
    return m_impl->Unpack (pArgumentsOut, pErrorOut);
}

Encoding ResponseFile::GetEncoding () const
{
    return m_impl->GetEncoding ();
}

}   // namespace ETWP