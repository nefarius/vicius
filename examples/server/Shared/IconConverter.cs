using System.Buffers.Binary;

namespace Nefarius.Vicius.Example.Server.Shared;

/// <summary>
///     Converts a PNG image to a Windows ICO file and returns it as a Base64 string,
///     suitable for use in <see cref="Nefarius.Vicius.Abstractions.Models.SharedConfig.IconBase64" />.
/// </summary>
/// <remarks>
///     Windows Vista and later support PNG-compressed entries inside .ico files, so no
///     pixel-level re-encoding is required.  The raw PNG bytes are wrapped in the minimal
///     ICONDIR / ICONDIRENTRY framing that the Win32 <c>CreateIconFromResourceEx</c> API
///     expects, and then the whole thing is Base64-encoded for JSON transport.
///     This keeps the server dependency-free while the client code path stays simple: it
///     always deals with a standard .ico buffer, regardless of how the image was originally
///     supplied.
/// </remarks>
internal static class IconConverter
{
    /// <summary>
    ///     Wraps <paramref name="png" /> in a one-entry ICO container and returns the result
    ///     as a standard Base64 string.
    /// </summary>
    /// <param name="png">Raw bytes of a valid PNG file.</param>
    /// <returns>Base64-encoded .ico containing a single PNG-compressed entry.</returns>
    /// <exception cref="ArgumentException">Thrown when <paramref name="png" /> is not a valid PNG.</exception>
    public static string PngToIcoBase64(byte[] png)
    {
        ArgumentNullException.ThrowIfNull(png);

        var (width, height) = ReadPngDimensions(png);

        // ICONDIRENTRY bWidth/bHeight are single bytes where 0 encodes exactly 256.
        // Values above 256 cannot be represented without truncation, so reject them explicitly.
        if (width > 256 || height > 256)
            throw new ArgumentException(
                $"PNG dimensions {width}x{height} exceed the maximum ICO entry size of 256x256.", nameof(png));

        // ICO file layout
        // ───────────────
        // ICONDIR      (6 bytes):  reserved(2), type(2)=1, count(2)=1
        // ICONDIRENTRY (16 bytes): bWidth, bHeight, bColorCount, bReserved,
        //                          wPlanes(2), wBitCount(2), dwBytesInRes(4), dwImageOffset(4)
        // <PNG bytes>
        const int iconDirSize  = 6;
        const int iconEntrySize = 16;
        const int imageOffset  = iconDirSize + iconEntrySize; // 22

        var ico = new byte[imageOffset + png.Length];
        var span = ico.AsSpan();

        // ICONDIR
        BinaryPrimitives.WriteUInt16LittleEndian(span[0..], 0);        // reserved
        BinaryPrimitives.WriteUInt16LittleEndian(span[2..], 1);        // type = icon
        BinaryPrimitives.WriteUInt16LittleEndian(span[4..], 1);        // count = 1

        // ICONDIRENTRY
        span[6] = (byte)(width  >= 256 ? 0 : width);   // bWidth  (0 = 256)
        span[7] = (byte)(height >= 256 ? 0 : height);  // bHeight (0 = 256)
        span[8] = 0;                                    // bColorCount (0 = >8 bpp)
        span[9] = 0;                                    // bReserved
        BinaryPrimitives.WriteUInt16LittleEndian(span[10..], 1);                       // wPlanes
        BinaryPrimitives.WriteUInt16LittleEndian(span[12..], 32);                      // wBitCount
        BinaryPrimitives.WriteUInt32LittleEndian(span[14..], (uint)png.Length);        // dwBytesInRes
        BinaryPrimitives.WriteUInt32LittleEndian(span[18..], (uint)imageOffset);       // dwImageOffset

        png.CopyTo(ico, imageOffset);

        return Convert.ToBase64String(ico);
    }

    /// <summary>Reads width and height from the PNG IHDR chunk (big-endian uint32 at offsets 16 and 20).</summary>
    private static (int width, int height) ReadPngDimensions(byte[] png)
    {
        // PNG signature: 8 bytes, then IHDR chunk: 4-byte length + 4-byte "IHDR" + 4-byte W + 4-byte H
        const int minLength = 24;
        if (png.Length < minLength)
            throw new ArgumentException("Buffer is too small to be a valid PNG.", nameof(png));

        // Verify PNG magic bytes: 0x89 0x50 0x4E 0x47 0x0D 0x0A 0x1A 0x0A
        ReadOnlySpan<byte> magic = [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A];
        if (!png.AsSpan(0, 8).SequenceEqual(magic))
            throw new ArgumentException("Data does not start with a valid PNG signature.", nameof(png));

        var width  = (int)BinaryPrimitives.ReadUInt32BigEndian(png.AsSpan(16, 4));
        var height = (int)BinaryPrimitives.ReadUInt32BigEndian(png.AsSpan(20, 4));
        return (width, height);
    }
}
