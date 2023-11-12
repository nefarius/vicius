#include "pch.h"
#include "Crypto.h"
#include <wintrust.h>

#define ENCODING (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING)

typedef struct
{
    LPWSTR lpszProgramName;
    LPWSTR lpszPublisherLink;
    LPWSTR lpszMoreInfoLink;
} SPROG_PUBLISHERINFO, *PSPROG_PUBLISHERINFO;

static LPWSTR AllocateAndCopyWideString(LPCWSTR inputString)
{
    LPWSTR outputString = nullptr;

    outputString = static_cast<LPWSTR>(LocalAlloc(
            LPTR,
            (wcslen(inputString) + 1) * sizeof(WCHAR))
    );

    if (outputString != nullptr)
    {
        lstrcpyW(outputString, inputString);
    }

    return outputString;
}

BOOL GetProgAndPublisherInfo(
    PCMSG_SIGNER_INFO pSignerInfo,
    PSPROG_PUBLISHERINFO Info
)
{
    BOOL fReturn = FALSE;
    PSPC_SP_OPUS_INFO OpusInfo = nullptr;
    DWORD dwData;
    BOOL fResult;

    __try
    {
        // Loop through authenticated attributes and find
        // SPC_SP_OPUS_INFO_OBJID OID.
        for (DWORD n = 0; n < pSignerInfo->AuthAttrs.cAttr; n++)
        {
            if (lstrcmpA(SPC_SP_OPUS_INFO_OBJID,
                         pSignerInfo->AuthAttrs.rgAttr[n].pszObjId) == 0)
            {
                // Get Size of SPC_SP_OPUS_INFO structure.
                fResult = CryptDecodeObject(
                    ENCODING,
                    SPC_SP_OPUS_INFO_OBJID,
                    pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].pbData,
                    pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].cbData,
                    0,
                    nullptr,
                    &dwData
                );
                if (!fResult)
                {
                    spdlog::error("CryptDecodeObject failed with {0:#x}", GetLastError());
                    __leave;
                }

                // Allocate memory for SPC_SP_OPUS_INFO structure.
                OpusInfo = static_cast<PSPC_SP_OPUS_INFO>(LocalAlloc(LPTR, dwData));
                if (!OpusInfo)
                {
                    spdlog::error("Unable to allocate memory for Publisher Info");
                    __leave;
                }

                // Decode and get SPC_SP_OPUS_INFO structure.
                fResult = CryptDecodeObject(
                    ENCODING,
                    SPC_SP_OPUS_INFO_OBJID,
                    pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].pbData,
                    pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].cbData,
                    0,
                    OpusInfo,
                    &dwData
                );

                if (!fResult)
                {
                    spdlog::error("CryptDecodeObject failed with {0:#x}", GetLastError());
                    __leave;
                }

                // Fill in Program Name if present.
                if (OpusInfo->pwszProgramName)
                {
                    Info->lpszProgramName =
                        AllocateAndCopyWideString(OpusInfo->pwszProgramName);
                }
                else
                    Info->lpszProgramName = nullptr;

                // Fill in Publisher Information if present.
                if (OpusInfo->pPublisherInfo)
                {
                    switch (OpusInfo->pPublisherInfo->dwLinkChoice)
                    {
                    case SPC_URL_LINK_CHOICE:
                        Info->lpszPublisherLink =
                            AllocateAndCopyWideString(OpusInfo->pPublisherInfo->pwszUrl);
                        break;

                    case SPC_FILE_LINK_CHOICE:
                        Info->lpszPublisherLink =
                            AllocateAndCopyWideString(OpusInfo->pPublisherInfo->pwszFile);
                        break;

                    default:
                        Info->lpszPublisherLink = nullptr;
                        break;
                    }
                }
                else
                {
                    Info->lpszPublisherLink = nullptr;
                }

                // Fill in More Info if present.
                if (OpusInfo->pMoreInfo)
                {
                    switch (OpusInfo->pMoreInfo->dwLinkChoice)
                    {
                    case SPC_URL_LINK_CHOICE:
                        Info->lpszMoreInfoLink =
                            AllocateAndCopyWideString(OpusInfo->pMoreInfo->pwszUrl);
                        break;

                    case SPC_FILE_LINK_CHOICE:
                        Info->lpszMoreInfoLink =
                            AllocateAndCopyWideString(OpusInfo->pMoreInfo->pwszFile);
                        break;

                    default:
                        Info->lpszMoreInfoLink = nullptr;
                        break;
                    }
                }
                else
                {
                    Info->lpszMoreInfoLink = nullptr;
                }

                fReturn = TRUE;

                break; // Break from for loop.
            } // lstrcmp SPC_SP_OPUS_INFO_OBJID                
        } // for
    }
    __finally
    {
        if (OpusInfo != nullptr) LocalFree(OpusInfo);
    }

    return fReturn;
}

BOOL GetDateOfTimeStamp(PCMSG_SIGNER_INFO pSignerInfo, SYSTEMTIME* st)
{
    BOOL fResult;
    FILETIME lft, ft;
    DWORD dwData;
    BOOL fReturn = FALSE;

    // Loop through authenticated attributes and find
    // szOID_RSA_signingTime OID.
    for (DWORD n = 0; n < pSignerInfo->AuthAttrs.cAttr; n++)
    {
        if (lstrcmpA(szOID_RSA_signingTime,
                     pSignerInfo->AuthAttrs.rgAttr[n].pszObjId) == 0)
        {
            // Decode and get FILETIME structure.
            dwData = sizeof(ft);
            fResult = CryptDecodeObject(
                ENCODING,
                szOID_RSA_signingTime,
                pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].pbData,
                pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].cbData,
                0,
                &ft,
                &dwData
            );

            if (!fResult)
            {
                spdlog::error("CryptDecodeObject failed with {0:#x}", GetLastError());
                break;
            }

            // Convert to local time.
            FileTimeToLocalFileTime(&ft, &lft);
            FileTimeToSystemTime(&lft, st);

            fReturn = TRUE;

            break; // Break from for loop.
        } //lstrcmp szOID_RSA_signingTime
    } // for

    return fReturn;
}

BOOL PrintCertificateInfo(PCCERT_CONTEXT pCertContext)
{
    BOOL fReturn = FALSE;
    LPTSTR szName = nullptr;
    DWORD dwData;

    __try
    {
        // Print Serial Number.
        _tprintf(_T("Serial Number: "));
        dwData = pCertContext->pCertInfo->SerialNumber.cbData;
        for (DWORD n = 0; n < dwData; n++)
        {
            _tprintf(_T("%02x "),
                     pCertContext->pCertInfo->SerialNumber.pbData[dwData - (n + 1)]);
        }
        _tprintf(_T("\n"));

        // Get Issuer name size.
        if (!(dwData = CertGetNameString(
            pCertContext,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            CERT_NAME_ISSUER_FLAG,
            nullptr,
            nullptr,
            0
        )))
        {
            spdlog::error("CertGetNameString failed with {0:#x}", GetLastError());
            __leave;
        }

        // Allocate memory for Issuer name.
        szName = static_cast<LPTSTR>(LocalAlloc(LPTR, dwData * sizeof(TCHAR)));
        if (!szName)
        {
            spdlog::error("Unable to allocate memory for timestamp info");
            __leave;
        }

        // Get Issuer name.
        if (!(CertGetNameString(
            pCertContext,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            CERT_NAME_ISSUER_FLAG,
            nullptr,
            szName,
            dwData
        )))
        {
            spdlog::error("CertGetNameString failed with {0:#x}", GetLastError());
            __leave;
        }

        // print Issuer name.
        _tprintf(_T("Issuer Name: %s\n"), szName);
        LocalFree(szName);
        szName = nullptr;

        // Get Subject name size.
        if (!(dwData = CertGetNameString(
            pCertContext,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            0,
            nullptr,
            nullptr,
            0
        )))
        {
            spdlog::error("CertGetNameString failed with {0:#x}", GetLastError());
            __leave;
        }

        // Allocate memory for subject name.
        szName = static_cast<LPTSTR>(LocalAlloc(LPTR, dwData * sizeof(TCHAR)));
        if (!szName)
        {
            spdlog::error("Unable to allocate memory for timestamp info");
            __leave;
        }

        // Get subject name.
        if (!(CertGetNameString(
            pCertContext,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            0,
            nullptr,
            szName,
            dwData
        )))
        {
            spdlog::error("CertGetNameString failed with {0:#x}", GetLastError());
            __leave;
        }

        // Print Subject Name.
        _tprintf(_T("Subject Name: %s\n"), szName);

        fReturn = TRUE;
    }
    __finally
    {
        if (szName != nullptr) LocalFree(szName);
    }

    return fReturn;
}

BOOL ExtractCertificateInfo(PCCERT_CONTEXT pCertContext, crypto::PSIGNATURE_INFORMATION info)
{
    BOOL fReturn = FALSE;
    LPTSTR szName = nullptr;
    DWORD dwData;

    __try
    {
        // Print Serial Number.
        _tprintf(_T("Serial Number: "));
        dwData = pCertContext->pCertInfo->SerialNumber.cbData;
        // allocate two wide characters per serial byte and NULL terminator
        info->SerialNumber = static_cast<LPTSTR>(LocalAlloc(LPTR, ((dwData * 2) + 1) * sizeof(TCHAR)));
        LPTSTR lpszPointer = info->SerialNumber;

        for(DWORD n = 0; n < dwData; n++)
        {
            lpszPointer += _stprintf_s(
                lpszPointer,
                dwData,
                _T("%02X"),
                pCertContext->pCertInfo->SerialNumber.pbData[dwData - (n + 1)]
            );

            //_tprintf(_T("%02x "),
            //         pCertContext->pCertInfo->SerialNumber.pbData[dwData - (n + 1)]);
        }

        // Get Issuer name size.
        if (!(dwData = CertGetNameString(
            pCertContext,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            CERT_NAME_ISSUER_FLAG,
            nullptr,
            nullptr,
            0
        )))
        {
            spdlog::error("CertGetNameString failed with {0:#x}", GetLastError());
            __leave;
        }

        // Allocate memory for Issuer name.
        szName = static_cast<LPTSTR>(LocalAlloc(LPTR, dwData * sizeof(TCHAR)));
        if (!szName)
        {
            spdlog::error("Unable to allocate memory for timestamp info");
            __leave;
        }

        // Get Issuer name.
        if (!(CertGetNameString(
            pCertContext,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            CERT_NAME_ISSUER_FLAG,
            nullptr,
            szName,
            dwData
        )))
        {
            spdlog::error("CertGetNameString failed with {0:#x}", GetLastError());
            __leave;
        }

        info->IssuerName = AllocateAndCopyWideString(szName);
        LocalFree(szName);
        szName = nullptr;

        // Get Subject name size.
        if (!(dwData = CertGetNameString(
            pCertContext,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            0,
            nullptr,
            nullptr,
            0
        )))
        {
            spdlog::error("CertGetNameString failed with {0:#x}", GetLastError());
            __leave;
        }

        // Allocate memory for subject name.
        szName = static_cast<LPTSTR>(LocalAlloc(LPTR, dwData * sizeof(TCHAR)));
        if (!szName)
        {
            spdlog::error("Unable to allocate memory for timestamp info");
            __leave;
        }

        // Get subject name.
        if (!(CertGetNameString(
            pCertContext,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            0,
            nullptr,
            szName,
            dwData
        )))
        {
            spdlog::error("CertGetNameString failed with {0:#x}", GetLastError());
            __leave;
        }

        info->SubjectName = AllocateAndCopyWideString(szName);

        fReturn = TRUE;
    }
    __finally
    {
        if (szName != nullptr) LocalFree(szName);
    }

    return fReturn;
}

BOOL GetTimeStampSignerInfo(PCMSG_SIGNER_INFO pSignerInfo, PCMSG_SIGNER_INFO* pCounterSignerInfo)
{
    PCCERT_CONTEXT pCertContext = nullptr;
    BOOL fReturn = FALSE;
    BOOL fResult;
    DWORD dwSize;

    __try
    {
        *pCounterSignerInfo = nullptr;

        // Loop through unauthenticated attributes for
        // szOID_RSA_counterSign OID.
        for (DWORD n = 0; n < pSignerInfo->UnauthAttrs.cAttr; n++)
        {
            if (lstrcmpA(pSignerInfo->UnauthAttrs.rgAttr[n].pszObjId,
                         szOID_RSA_counterSign) == 0)
            {
                // Get size of CMSG_SIGNER_INFO structure.
                fResult = CryptDecodeObject(
                    ENCODING,
                    PKCS7_SIGNER_INFO,
                    pSignerInfo->UnauthAttrs.rgAttr[n].rgValue[0].pbData,
                    pSignerInfo->UnauthAttrs.rgAttr[n].rgValue[0].cbData,
                    0,
                    nullptr,
                    &dwSize
                );

                if (!fResult)
                {
                    spdlog::error("CryptDecodeObject failed with {0:#x}", GetLastError());
                    __leave;
                }

                // Allocate memory for CMSG_SIGNER_INFO.
                *pCounterSignerInfo = static_cast<PCMSG_SIGNER_INFO>(LocalAlloc(LPTR, dwSize));
                if (!*pCounterSignerInfo)
                {
                    spdlog::error("Unable to allocate memory for timestamp info");
                    __leave;
                }

                // Decode and get CMSG_SIGNER_INFO structure
                // for timestamp certificate.
                fResult = CryptDecodeObject(
                    ENCODING,
                    PKCS7_SIGNER_INFO,
                    pSignerInfo->UnauthAttrs.rgAttr[n].rgValue[0].pbData,
                    pSignerInfo->UnauthAttrs.rgAttr[n].rgValue[0].cbData,
                    0,
                    *pCounterSignerInfo,
                    &dwSize
                );

                if (!fResult)
                {
                    spdlog::error("CryptDecodeObject failed with {0:#x}", GetLastError());
                    __leave;
                }

                fReturn = TRUE;

                break; // Break from for loop.
            }
        }
    }
    __finally
    {
        // Clean up.
        if (pCertContext != nullptr) CertFreeCertificateContext(pCertContext);
    }

    return fReturn;
}

namespace crypto
{
    bool ExtractSignatureInformation(LPCWSTR filePath, PSIGNATURE_INFORMATION info)
    {
        WCHAR szFileName[MAX_PATH];
        HCERTSTORE hStore = nullptr;
        HCRYPTMSG hMsg = nullptr;
        PCCERT_CONTEXT pCertContext = nullptr;
        BOOL fResult = FALSE;
        DWORD dwEncoding, dwContentType, dwFormatType;
        PCMSG_SIGNER_INFO pSignerInfo = nullptr;
        PCMSG_SIGNER_INFO pCounterSignerInfo = nullptr;
        DWORD dwSignerInfo;
        CERT_INFO CertInfo;
        SPROG_PUBLISHERINFO ProgPubInfo;
        SYSTEMTIME st;

        ZeroMemory(&ProgPubInfo, sizeof(ProgPubInfo));
        __try
        {
            lstrcpynW(szFileName, filePath, MAX_PATH);

            // Get message handle and store handle from the signed file.
            fResult = CryptQueryObject(
                CERT_QUERY_OBJECT_FILE,
                szFileName,
                CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                CERT_QUERY_FORMAT_FLAG_BINARY,
                0,
                &dwEncoding,
                &dwContentType,
                &dwFormatType,
                &hStore,
                &hMsg,
                nullptr
            );

            if (!fResult)
            {
                spdlog::error("CryptQueryObject failed with {0:#x}", GetLastError());
                __leave;
            }

            // Get signer information size.
            fResult = CryptMsgGetParam(
                hMsg,
                CMSG_SIGNER_INFO_PARAM,
                0,
                nullptr,
                &dwSignerInfo
            );

            if (!fResult)
            {
                spdlog::error("CryptMsgGetParam failed with {0:#x}", GetLastError());
                __leave;
            }

            // Allocate memory for signer information.
            pSignerInfo = static_cast<PCMSG_SIGNER_INFO>(LocalAlloc(LPTR, dwSignerInfo));
            if (!pSignerInfo)
            {
                spdlog::error("Unable to allocate memory for Signer Info");
                __leave;
            }

            // Get Signer Information.
            fResult = CryptMsgGetParam(
                hMsg,
                CMSG_SIGNER_INFO_PARAM,
                0,
                pSignerInfo,
                &dwSignerInfo
            );

            if (!fResult)
            {
                spdlog::error("CryptMsgGetParam failed with {0:#x}", GetLastError());
                __leave;
            }

            // Get program name and publisher information from
            // signer info structure.
            if (GetProgAndPublisherInfo(pSignerInfo, &ProgPubInfo))
            {
                if (ProgPubInfo.lpszProgramName != nullptr)
                {
                    info->ProgramName = AllocateAndCopyWideString(ProgPubInfo.lpszProgramName);
                }

                if (ProgPubInfo.lpszPublisherLink != nullptr)
                {
                    info->PublisherLink = AllocateAndCopyWideString(ProgPubInfo.lpszPublisherLink);
                }

                if (ProgPubInfo.lpszMoreInfoLink != nullptr)
                {
                    info->MoreInfoLink = AllocateAndCopyWideString(ProgPubInfo.lpszMoreInfoLink);
                }
            }

            _tprintf(_T("\n"));

            // Search for the signer certificate in the temporary
            // certificate store.
            CertInfo.Issuer = pSignerInfo->Issuer;
            CertInfo.SerialNumber = pSignerInfo->SerialNumber;

            pCertContext = CertFindCertificateInStore(
                hStore,
                ENCODING,
                0,
                CERT_FIND_SUBJECT_CERT,
                &CertInfo,
                nullptr
            );

            if (!pCertContext)
            {
                _tprintf(_T("CertFindCertificateInStore failed with %x\n"),
                         GetLastError());
                __leave;
            }

            // Print Signer certificate information.
            _tprintf(_T("Signer Certificate:\n\n"));
            ExtractCertificateInfo(pCertContext, info);
            _tprintf(_T("\n"));

            // Get the timestamp certificate signerinfo structure.
            if (GetTimeStampSignerInfo(pSignerInfo, &pCounterSignerInfo))
            {
                // Search for Timestamp certificate in the temporary
                // certificate store.
                CertInfo.Issuer = pCounterSignerInfo->Issuer;
                CertInfo.SerialNumber = pCounterSignerInfo->SerialNumber;

                pCertContext = CertFindCertificateInStore(
                    hStore,
                    ENCODING,
                    0,
                    CERT_FIND_SUBJECT_CERT,
                    &CertInfo,
                    nullptr
                );

                if (!pCertContext)
                {
                    _tprintf(_T("CertFindCertificateInStore failed with %x\n"),
                             GetLastError());
                    __leave;
                }

                // TODO: test and implement me

                // Print timestamp certificate information.
                //_tprintf(_T("TimeStamp Certificate:\n\n"));
                //ExtractCertificateInfo(pCertContext, info);
                //_tprintf(_T("\n"));

                // Find Date of timestamp.
                if (GetDateOfTimeStamp(pCounterSignerInfo, &st))
                {
                    _tprintf(_T("Date of TimeStamp : %02d/%02d/%04d %02d:%02d\n"),
                             st.wMonth,
                             st.wDay,
                             st.wYear,
                             st.wHour,
                             st.wMinute);
                }
                _tprintf(_T("\n"));
            }
        }
        __finally
        {
            // Clean up.
            if (ProgPubInfo.lpszProgramName != nullptr)
                LocalFree(ProgPubInfo.lpszProgramName);
            if (ProgPubInfo.lpszPublisherLink != nullptr)
                LocalFree(ProgPubInfo.lpszPublisherLink);
            if (ProgPubInfo.lpszMoreInfoLink != nullptr)
                LocalFree(ProgPubInfo.lpszMoreInfoLink);

            if (pSignerInfo != nullptr) LocalFree(pSignerInfo);
            if (pCounterSignerInfo != nullptr) LocalFree(pCounterSignerInfo);
            if (pCertContext != nullptr) CertFreeCertificateContext(pCertContext);
            if (hStore != nullptr) CertCloseStore(hStore, 0);
            if (hMsg != nullptr) CryptMsgClose(hMsg);
        }

        return fResult;
    }

    void FreeSignatureInformation(PSIGNATURE_INFORMATION info)
    {
        if (info->ProgramName != nullptr)
            LocalFree(info->ProgramName);
        if (info->PublisherLink != nullptr)
            LocalFree(info->PublisherLink);
        if (info->MoreInfoLink != nullptr)
            LocalFree(info->MoreInfoLink);
    }
}
