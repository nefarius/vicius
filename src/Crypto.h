#pragma once

namespace crypto
{
    typedef struct
    {
        LPWSTR lpszProgramName;
        LPWSTR lpszPublisherLink;
        LPWSTR lpszMoreInfoLink;
    } SPROG_PUBLISHERINFO, *PSPROG_PUBLISHERINFO;

    BOOL
    GetProgAndPublisherInfo(
        PCMSG_SIGNER_INFO pSignerInfo,
        PSPROG_PUBLISHERINFO Info
    );

    BOOL
    GetDateOfTimeStamp(
        PCMSG_SIGNER_INFO pSignerInfo,
        SYSTEMTIME* st
    );

    BOOL
    PrintCertificateInfo(
        PCCERT_CONTEXT pCertContext
    );

    BOOL
    GetTimeStampSignerInfo(
        PCMSG_SIGNER_INFO pSignerInfo,
        PCMSG_SIGNER_INFO* pCounterSignerInfo
    );
}
