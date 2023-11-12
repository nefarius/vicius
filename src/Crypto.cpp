#include "pch.h"
#include "Crypto.h"
#include <wintrust.h>

#define ENCODING (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING)

namespace crypto
{
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
                        _tprintf(_T("CryptDecodeObject failed with %x\n"),
                                 GetLastError());
                        __leave;
                    }

                    // Allocate memory for SPC_SP_OPUS_INFO structure.
                    OpusInfo = static_cast<PSPC_SP_OPUS_INFO>(LocalAlloc(LPTR, dwData));
                    if (!OpusInfo)
                    {
                        _tprintf(_T("Unable to allocate memory for Publisher Info.\n"));
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
                        _tprintf(_T("CryptDecodeObject failed with %x\n"),
                                 GetLastError());
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
                    _tprintf(_T("CryptDecodeObject failed with %x\n"),
                             GetLastError());
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
                _tprintf(_T("CertGetNameString failed.\n"));
                __leave;
            }

            // Allocate memory for Issuer name.
            szName = static_cast<LPTSTR>(LocalAlloc(LPTR, dwData * sizeof(TCHAR)));
            if (!szName)
            {
                _tprintf(_T("Unable to allocate memory for issuer name.\n"));
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
                _tprintf(_T("CertGetNameString failed.\n"));
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
                _tprintf(_T("CertGetNameString failed.\n"));
                __leave;
            }

            // Allocate memory for subject name.
            szName = static_cast<LPTSTR>(LocalAlloc(LPTR, dwData * sizeof(TCHAR)));
            if (!szName)
            {
                _tprintf(_T("Unable to allocate memory for subject name.\n"));
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
                _tprintf(_T("CertGetNameString failed.\n"));
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

    BOOL GetTimeStampSignerInfo(PCMSG_SIGNER_INFO pSignerInfo, PCMSG_SIGNER_INFO* pCounterSignerInfo)
    {
        PCCERT_CONTEXT pCertContext = nullptr;
        BOOL fReturn = FALSE;
        BOOL fResult;
        DWORD dwSize;

        __try
        {
            *pCounterSignerInfo = nullptr;

            // Loop through unathenticated attributes for
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
                        _tprintf(_T("CryptDecodeObject failed with %x\n"),
                                 GetLastError());
                        __leave;
                    }

                    // Allocate memory for CMSG_SIGNER_INFO.
                    *pCounterSignerInfo = static_cast<PCMSG_SIGNER_INFO>(LocalAlloc(LPTR, dwSize));
                    if (!*pCounterSignerInfo)
                    {
                        _tprintf(_T("Unable to allocate memory for timestamp info.\n"));
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
                        _tprintf(_T("CryptDecodeObject failed with %x\n"),
                                 GetLastError());
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
}
