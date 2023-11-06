// NAuthenticode.cpp: Various routines related to validating file signatures
//
// Copyright (c) 2008-2012, Nikolaos D. Bougalis <nikb@bougalis.net>

#include "pch.h"
#include "NAuthenticode.h"

#define ASSERT _ASSERT

//////////////////////////////////////////////////////////////////////////
#pragma comment(lib, "crypt32")
#pragma comment(lib, "imagehlp")
#pragma comment(lib, "wintrust")

//////////////////////////////////////////////////////////////////////////
#define SIG_ENCODING (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING)

//////////////////////////////////////////////////////////////////////////
// Some utility functions
LPVOID NHeapAlloc(SIZE_T dwBytes)
{
    if (dwBytes == 0)
        return nullptr;

    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytes);
}

//////////////////////////////////////////////////////////////////////////
LPVOID NHeapFree(LPVOID lpMem)
{
    if (lpMem != nullptr)
        HeapFree(GetProcessHeap(), 0, lpMem);

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
LPSTR NConvertW2A(LPCWSTR lpszString, int nLen, UINT nCodePage)
{
    ASSERT(lpszString != nullptr);

    int ret = WideCharToMultiByte(nCodePage, 0, lpszString, nLen, nullptr, 0, nullptr, nullptr);

    if (ret <= 0)
        return nullptr;

    auto lpszOutString = static_cast<LPSTR>(NHeapAlloc((ret + 1) * sizeof(CHAR)));

    if (lpszOutString == nullptr)
        return nullptr;

    ret = WideCharToMultiByte(nCodePage, 0, lpszString, nLen, lpszOutString, ret, nullptr, nullptr);

    if (ret <= 0)
        lpszOutString = static_cast<LPSTR>(NHeapFree(lpszOutString));

    return lpszOutString;
}

//////////////////////////////////////////////////////////////////////////
LPWSTR NDupString(LPCWSTR lpszString, int nLen)
{
    if (nLen == -1)
        nLen = static_cast<int>(wcslen(lpszString));

    auto lpszOutString = static_cast<LPWSTR>(NHeapAlloc((2 + nLen) * sizeof(WCHAR)));

    if ((lpszOutString != nullptr) && (nLen != 0))
        wcsncpy_s(lpszOutString, nLen, lpszString, nLen + 1);

    return lpszOutString;
}

//////////////////////////////////////////////////////////////////////////
LPTSTR NConvertW2T(LPCWSTR lpszString, int nLen = -1, UINT nCodePage = 0)
{
    ASSERT(lpszString != nullptr);

#ifndef UNICODE
    return (LPTSTR)NConvertW2A(lpszString, nLen, nCodePage);
#else
    return NDupString(lpszString, nLen);
#endif
}

//////////////////////////////////////////////////////////////////////////
LPWSTR NConvertA2W(LPCSTR lpszString, int nLen, UINT nCodePage)
{
    ASSERT(lpszString != nullptr);

    int ret = MultiByteToWideChar(nCodePage, 0, lpszString, nLen, nullptr, 0);

    if (ret <= 0)
        return nullptr;

    auto lpszOutString = static_cast<LPWSTR>(NHeapAlloc((ret + 1) * sizeof(WCHAR)));

    if (lpszOutString == nullptr)
        return nullptr;

    ret = MultiByteToWideChar(nCodePage, 0, lpszString, nLen, lpszOutString, ret);

    if (ret <= 0)
        lpszOutString = static_cast<LPWSTR>(NHeapFree(lpszOutString));

    return lpszOutString;
}

//////////////////////////////////////////////////////////////////////////
LPWSTR NConvertT2W(LPCTSTR lpszString, int nLen = -1, UINT nCodePage = 0)
{
    ASSERT(lpszString != nullptr);

#ifndef UNICODE
    return NConvertA2W((LPCSTR)lpszString, nLen, nCodePage);
#else
    return NDupString(lpszString, nLen);
#endif
}

//////////////////////////////////////////////////////////////////////////
VOID NCertFreeSigInfo(NSIGINFO* pSigInfo)
{
    if (pSigInfo == nullptr)
        return;

    __try
    {
        // Be extra careful
        if (pSigInfo->lpszPublisher)
            pSigInfo->lpszPublisher = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszPublisher));

        if (pSigInfo->lpszPublisherEmail)
            pSigInfo->lpszPublisherEmail = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszPublisherEmail));

        if (pSigInfo->lpszPublisherUrl)
            pSigInfo->lpszPublisherUrl = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszPublisherUrl));

        if (pSigInfo->lpszAuthority)
            pSigInfo->lpszAuthority = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszAuthority));

        if (pSigInfo->lpszProgramName)
            pSigInfo->lpszProgramName = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszPublisher));

        if (pSigInfo->lpszPublisherLink)
            pSigInfo->lpszPublisherLink = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszPublisher));

        if (pSigInfo->lpszMoreInfoLink)
            pSigInfo->lpszMoreInfoLink = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszMoreInfoLink));

        if (pSigInfo->lpszSignature)
            pSigInfo->lpszSignature = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszSignature));

        if (pSigInfo->lpszSerial)
            pSigInfo->lpszSerial = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszSerial));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

//////////////////////////////////////////////////////////////////////////
static BOOL NCertGetNameString(PCCERT_CONTEXT pCertContext, DWORD dwType, DWORD dwFlags, LPTSTR* lpszNameString)
{
    if (pCertContext == nullptr)
        return FALSE;

    DWORD dwData = CertGetNameString(pCertContext, dwType, 0, nullptr, nullptr, 0);

    if (dwData == 0)
        return FALSE;

    *lpszNameString = static_cast<LPTSTR>(NHeapAlloc((dwData + 1) * sizeof(TCHAR)));

    if (*lpszNameString == nullptr)
        return FALSE;

    dwData = CertGetNameString(pCertContext, dwType, dwFlags, nullptr, *lpszNameString, dwData);

    if (dwData == 0)
    {
        NHeapFree(*lpszNameString);
        return FALSE;
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////
static BOOL NCryptDecodeObject(__in LPCSTR lpszObjectId, __in_bcount(cbEncoded) const BYTE* pbEncoded,
                               __in DWORD cbEncoded,
                               __inout DWORD& dwBuffer, __out void* pBuffer = nullptr, __in DWORD dwFlags = 0)
{
    if (((pBuffer == nullptr) && (dwBuffer != 0)) || ((dwBuffer == 0) && (pBuffer != nullptr)))
    {
        // What? You're passing a NULL pointer an a non-zero size? You so crazy!!!!
        ASSERT(FALSE);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return CryptDecodeObject(SIG_ENCODING, lpszObjectId, pbEncoded, cbEncoded, dwFlags, pBuffer, &dwBuffer);
}

//////////////////////////////////////////////////////////////////////////
static BOOL NCryptDecodeObject(__in LPCSTR lpszObjectId, __in PCRYPT_ATTR_BLOB pObject,
                               __inout DWORD& dwBuffer, __out void* pBuffer = nullptr, __in DWORD dwFlags = 0)
{
    if ((pObject == nullptr) || ((dwBuffer == 0) && (pBuffer != nullptr)) || ((dwBuffer != 0) && (pBuffer == nullptr)))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return CryptDecodeObject(SIG_ENCODING, lpszObjectId, pObject->pbData, pObject->cbData, dwFlags, pBuffer, &dwBuffer);
}

//////////////////////////////////////////////////////////////////////////
static BOOL WGetSignTimestamp(PCRYPT_ATTRIBUTES pAttributes, SYSTEMTIME& stTime, LPCSTR lpszObjId)
{
    if ((pAttributes == nullptr) || (pAttributes->cAttr == 0) || (lpszObjId == nullptr) || (*lpszObjId == 0))
        return FALSE;

    for (DWORD dwAttr = 0; dwAttr < pAttributes->cAttr; dwAttr++)
    {
        if (strcmp(lpszObjId, pAttributes->rgAttr[dwAttr].pszObjId) == 0)
        {
            DWORD dwSize = sizeof(FILETIME);
            FILETIME ftCert;

            if (NCryptDecodeObject(lpszObjId, &pAttributes->rgAttr[dwAttr].rgValue[0], dwSize, &ftCert))
            {
                FILETIME ftLocal;

                if (FileTimeToLocalFileTime(&ftCert, &ftLocal) && FileTimeToSystemTime(&ftLocal, &stTime))
                    return TRUE;
            }
        }
    }

    return FALSE;
}

//////////////////////////////////////////////////////////////////////////
static BOOL NVerifyFileSignatureWorker(LPWSTR lpszFileName, WINTRUST_DATA& wtData, NSIGINFO* pSigInfo)
{
    if (pSigInfo != nullptr)
        memset(pSigInfo, 0, sizeof(NSIGINFO));

    GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    BOOL bVerified = FALSE;

    LONG lRet = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &guidAction, &wtData);

    if (lRet != 0)
    {
        if (pSigInfo != nullptr)
            pSigInfo->lValidationResult = lRet;

        return FALSE;
    }

    if (pSigInfo == nullptr)
        return TRUE;

    HCERTSTORE hStore = nullptr;
    HCRYPTMSG hMsg = nullptr;

    if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, lpszFileName, CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                          CERT_QUERY_FORMAT_FLAG_BINARY, 0, nullptr, nullptr, nullptr, &hStore, &hMsg, nullptr))
        return FALSE;

    PCMSG_SIGNER_INFO pSignerInfo = nullptr, pCounterSignerInfo = nullptr;
    DWORD dwSignerInfo = 0, dwCounterSignerInfo = 0;

    if (CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &dwSignerInfo) && (dwSignerInfo != 0))
        pSignerInfo = static_cast<PCMSG_SIGNER_INFO>(NHeapAlloc(dwSignerInfo));

    if ((pSignerInfo != nullptr) && CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, pSignerInfo, &dwSignerInfo))
    {
        for (DWORD dwAttr = 0; dwAttr < pSignerInfo->AuthAttrs.cAttr; dwAttr++)
        {
            if ((strcmp(SPC_SP_OPUS_INFO_OBJID, pSignerInfo->AuthAttrs.rgAttr[dwAttr].pszObjId) != 0))
                continue;

            PSPC_SP_OPUS_INFO pOpus = nullptr;
            DWORD dwData = 0;

            if (NCryptDecodeObject(SPC_SP_OPUS_INFO_OBJID, &pSignerInfo->AuthAttrs.rgAttr[dwAttr].rgValue[0], dwData) &&
                (dwData != 0))
                pOpus = static_cast<PSPC_SP_OPUS_INFO>(NHeapAlloc(dwData));

            if ((pOpus != nullptr) && NCryptDecodeObject(
                SPC_SP_OPUS_INFO_OBJID, &pSignerInfo->AuthAttrs.rgAttr[dwAttr].rgValue[0], dwData, pOpus))
            {
                pSigInfo->lpszProgramName = NConvertW2T(pOpus->pwszProgramName);

                if (pOpus->pPublisherInfo != nullptr)
                {
                    switch (pOpus->pPublisherInfo->dwLinkChoice)
                    {
                    case SPC_URL_LINK_CHOICE:
                        pSigInfo->lpszPublisherLink = NConvertW2T(pOpus->pPublisherInfo->pwszUrl);
                        break;

                    case SPC_FILE_LINK_CHOICE:
                        pSigInfo->lpszPublisherLink = NConvertW2T(pOpus->pPublisherInfo->pwszFile);
                        break;
                    }
                }

                if (pOpus->pMoreInfo != nullptr)
                {
                    switch (pOpus->pMoreInfo->dwLinkChoice)
                    {
                    case SPC_URL_LINK_CHOICE:
                        pSigInfo->lpszMoreInfoLink = NConvertW2T(pOpus->pMoreInfo->pwszUrl);
                        break;

                    case SPC_FILE_LINK_CHOICE:
                        pSigInfo->lpszMoreInfoLink = NConvertW2T(pOpus->pMoreInfo->pwszFile);
                        break;
                    }
                }
            }

            if (pOpus != nullptr)
                NHeapFree(pOpus);

            break;
        }

        CERT_INFO ci;

        ci.Issuer = pSignerInfo->Issuer;
        ci.SerialNumber = pSignerInfo->SerialNumber;

        PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(hStore, SIG_ENCODING, 0, CERT_FIND_SUBJECT_CERT, &ci,
                                                                 nullptr);

        if (pCertContext != nullptr)
        {
            if (pCertContext->pCertInfo->SerialNumber.cbData != 0)
            {
                pSigInfo->lpszSerial = static_cast<LPTSTR>(NHeapAlloc(
                    ((pCertContext->pCertInfo->SerialNumber.cbData * 2) + 1) * sizeof(TCHAR)));

                if (pSigInfo->lpszSerial != nullptr)
                {
                    LPTSTR lpszPointer = pSigInfo->lpszSerial;

                    for (DWORD dwCount = pCertContext->pCertInfo->SerialNumber.cbData; dwCount != 0; dwCount--)
                        lpszPointer += _stprintf_s(lpszPointer, pCertContext->pCertInfo->SerialNumber.cbData,
                                                   _T("%02X"),
                                                   pCertContext->pCertInfo->SerialNumber.pbData[dwCount - 1]);
                }
            }

            if (!NCertGetNameString(pCertContext, CERT_NAME_FRIENDLY_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG,
                                    &pSigInfo->lpszFriendlyName))
                pSigInfo->lpszFriendlyName = nullptr;

            if (!NCertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG,
                                    &pSigInfo->lpszAuthority))
                pSigInfo->lpszAuthority = nullptr;

            if (!NCertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, &pSigInfo->lpszPublisher))
                pSigInfo->lpszPublisher = nullptr;

            if (!NCertGetNameString(pCertContext, CERT_NAME_URL_TYPE, 0, &pSigInfo->lpszPublisherUrl))
                pSigInfo->lpszPublisherUrl = nullptr;

            if (!NCertGetNameString(pCertContext, CERT_NAME_EMAIL_TYPE, 0, &pSigInfo->lpszPublisherEmail))
                pSigInfo->lpszPublisherEmail = nullptr;

            CertFreeCertificateContext(pCertContext);
        }

        for (DWORD dwAttr = 0, dwData; dwAttr < pSignerInfo->AuthAttrs.cAttr; dwAttr++)
        {
            if ((strcmp(szOID_RSA_signingTime, pSignerInfo->AuthAttrs.rgAttr[dwAttr].pszObjId) == 0) && (pSignerInfo->
                AuthAttrs.rgAttr[dwAttr].cValue != 0))
            {
                FILETIME ftCert;

                dwData = sizeof(FILETIME);

                if (NCryptDecodeObject(szOID_RSA_signingTime, &pSignerInfo->AuthAttrs.rgAttr[dwAttr].rgValue[0], dwData,
                                       &ftCert))
                {
                    FILETIME ftLocal;

                    if (!FileTimeToLocalFileTime(&ftCert, &ftLocal))
                    {
                        if (!FileTimeToSystemTime(&ftLocal, &pSigInfo->stSigTime))
                            memset(&pSigInfo->stSigTime, 0, sizeof(SYSTEMTIME));
                    }
                }
            }
        }

        for (DWORD dwAttr = 0; dwAttr < pSignerInfo->UnauthAttrs.cAttr; dwAttr++)
        {
            if (strcmp(pSignerInfo->UnauthAttrs.rgAttr[dwAttr].pszObjId, szOID_RSA_counterSign) == 0)
            {
                if (NCryptDecodeObject(PKCS7_SIGNER_INFO, &pSignerInfo->UnauthAttrs.rgAttr[dwAttr].rgValue[0],
                                       dwCounterSignerInfo) && (dwCounterSignerInfo != 0))
                    pCounterSignerInfo = static_cast<PCMSG_SIGNER_INFO>(NHeapAlloc(dwCounterSignerInfo));

                if ((pCounterSignerInfo != nullptr) && !NCryptDecodeObject(
                    PKCS7_SIGNER_INFO, &pSignerInfo->UnauthAttrs.rgAttr[dwAttr].rgValue[0], dwCounterSignerInfo,
                    pCounterSignerInfo))
                    pCounterSignerInfo = static_cast<PCMSG_SIGNER_INFO>(NHeapFree(pCounterSignerInfo));

                break;
            }
        }

        if (pCounterSignerInfo != nullptr)
        {
            pSigInfo->bHasSigTime = WGetSignTimestamp(&pCounterSignerInfo->AuthAttrs, pSigInfo->stSigTime,
                                                      szOID_RSA_signingTime);

            if (!pSigInfo->bHasSigTime)
                memset(&pSigInfo->stSigTime, 0, sizeof(SYSTEMTIME));
        }
    }

    if (pSignerInfo != nullptr)
        NHeapFree(pSignerInfo);

    if (pCounterSignerInfo != nullptr)
        NHeapFree(pCounterSignerInfo);

    if (hStore != nullptr)
        CertCloseStore(hStore, 0);

    if (hMsg != nullptr)
        CryptMsgClose(hMsg);

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////
BOOL NVerifyFileSignature(LPCTSTR lpszFileName, NSIGINFO* pSigInfo, HANDLE hHandle)
{
    if (pSigInfo != nullptr)
        memset(pSigInfo, 0, sizeof(NSIGINFO));

    if (lpszFileName == nullptr)
        return FALSE;

    if ((lpszFileName[0] != 0) && (_tcsnicmp(lpszFileName, _T("\\??\\"), 4) == 0))
        lpszFileName += 4;

    if (lpszFileName[0] == 0)
        return FALSE;

    LPWSTR lpwszFileName = NConvertT2W(lpszFileName);

    if (lpwszFileName == nullptr)
        return FALSE;

    BOOL bOK = FALSE;

    __try
    {
        // be very careful... 
        WINTRUST_FILE_INFO wtFileInfo;
        memset(&wtFileInfo, 0, sizeof(WINTRUST_FILE_INFO));

        wtFileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
        wtFileInfo.pcwszFilePath = lpwszFileName;

        if (hHandle != INVALID_HANDLE_VALUE)
            wtFileInfo.hFile = hHandle;

        WINTRUST_DATA wtData;
        memset(&wtData, 0, sizeof(WINTRUST_DATA));
        wtData.cbStruct = sizeof(WINTRUST_DATA);
        wtData.dwUIChoice = WTD_UI_NONE;
        wtData.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
        wtData.dwUnionChoice = WTD_CHOICE_FILE;
        wtData.pFile = &wtFileInfo;

        if (NVerifyFileSignatureWorker(lpwszFileName, wtData, pSigInfo))
            bOK = TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (pSigInfo != nullptr)
        {
            if (pSigInfo->lpszPublisher)
                pSigInfo->lpszPublisher = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszPublisher));

            if (pSigInfo->lpszAuthority)
                pSigInfo->lpszAuthority = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszAuthority));

            if (pSigInfo->lpszProgramName)
                pSigInfo->lpszProgramName = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszPublisher));

            if (pSigInfo->lpszPublisherLink)
                pSigInfo->lpszPublisherLink = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszPublisher));

            if (pSigInfo->lpszMoreInfoLink)
                pSigInfo->lpszMoreInfoLink = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszMoreInfoLink));

            if (pSigInfo->lpszSignature)
                pSigInfo->lpszSignature = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszSignature));

            if (pSigInfo->lpszSerial)
                pSigInfo->lpszSerial = static_cast<LPTSTR>(NHeapFree(pSigInfo->lpszSerial));
        }

        bOK = FALSE;
    }

    NHeapFree(lpwszFileName);

    return bOK;
}

//////////////////////////////////////////////////////////////////////////
BOOL NCheckFileCertificates(HANDLE hFile, VOID (*pCallback)(PCCERT_CONTEXT, LPVOID), PVOID pParam)
{
    DWORD dwCerts = 0;

    if (!ImageEnumerateCertificates(hFile, CERT_SECTION_TYPE_ANY, &dwCerts, nullptr, 0))
        return FALSE;

    for (DWORD dwCount = 0; dwCount < dwCerts; dwCount++)
    {
        WIN_CERTIFICATE wcHdr;
        memset(&wcHdr, 0, sizeof(WIN_CERTIFICATE));
        wcHdr.dwLength = 0;
        wcHdr.wRevision = WIN_CERT_REVISION_1_0;

        if (!ImageGetCertificateHeader(hFile, dwCount, &wcHdr))
            return FALSE;

        DWORD dwLen = sizeof(WIN_CERTIFICATE) + wcHdr.dwLength;

        auto pWinCert = static_cast<WIN_CERTIFICATE*>(NHeapAlloc(dwLen));

        if (pWinCert == nullptr)
            return FALSE;

        if (!ImageGetCertificateData(hFile, dwCount, pWinCert, &dwLen))
        {
            // problem getting certificate, return failure
            NHeapFree(pWinCert);
            return FALSE;
        }

        // extract the PKCS7 signed data     
        CRYPT_VERIFY_MESSAGE_PARA cvmp;
        memset(&cvmp, 0, sizeof(CRYPT_VERIFY_MESSAGE_PARA));
        cvmp.cbSize = sizeof(CRYPT_VERIFY_MESSAGE_PARA);
        cvmp.dwMsgAndCertEncodingType = SIG_ENCODING;

        PCCERT_CONTEXT pCertContext = nullptr;

        if (!CryptVerifyMessageSignature(&cvmp, dwCount, pWinCert->bCertificate, pWinCert->dwLength, nullptr, nullptr,
                                         &pCertContext))
        {
            NHeapFree(pWinCert);
            return FALSE;
        }

        // Now, pass this context on to our callback function (if any)
        if (pCallback != nullptr)
            pCallback(pCertContext, pParam);

        if (!CertFreeCertificateContext(pCertContext))
        {
            NHeapFree(pWinCert);
            return FALSE;
        }

        NHeapFree(pWinCert);
    }

    return TRUE;
}
