// NAuthenticode.h: Functions for checking signatures in files
//
// Copyright (c) 2008-2012, Nikolaos D. Bougalis <nikb@bougalis.net>

#pragma once

#include <wintrust.h>
#include <softpub.h>
#include <imagehlp.h>

struct NSIGINFO
{
    LONG lValidationResult;

    LPTSTR lpszPublisher;
    LPTSTR lpszPublisherEmail;
    LPTSTR lpszPublisherUrl;
    LPTSTR lpszAuthority;
    LPTSTR lpszFriendlyName;
    LPTSTR lpszProgramName;
    LPTSTR lpszPublisherLink;
    LPTSTR lpszMoreInfoLink;
    LPTSTR lpszSignature;
    LPTSTR lpszSerial;
    BOOL bHasSigTime;
    SYSTEMTIME stSigTime;
};

VOID NCertFreeSigInfo(NSIGINFO* pSigInfo);

BOOL NVerifyFileSignature(LPCTSTR lpszFileName, NSIGINFO* pSigInfo, HANDLE hHandle = INVALID_HANDLE_VALUE);

BOOL NCertGetNameString(PCCERT_CONTEXT pCertContext, DWORD dwType,
                        DWORD dwFlags, LPTSTR* lpszNameString);

BOOL NCheckFileCertificates(HANDLE hFile,
                            VOID (*pCallback)(PCCERT_CONTEXT, LPVOID), PVOID pParam);
