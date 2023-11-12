#pragma once

namespace crypto
{
    typedef struct
    {
        LPWSTR ProgramName;
        LPWSTR PublisherLink;
        LPWSTR MoreInfoLink;
        LPWSTR IssuerName;
        LPWSTR SubjectName;
        LPWSTR SerialNumber;
    } SIGNATURE_INFORMATION, *PSIGNATURE_INFORMATION;

    bool ExtractSignatureInformation(LPCWSTR filePath, PSIGNATURE_INFORMATION info);
    void FreeSignatureInformation(PSIGNATURE_INFORMATION info);
}
