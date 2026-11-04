#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FACECONTROLFILE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FACECONTROLFILE_H

#include <optional>
#include <string_view>

#include "facelook.hpp"

namespace FalloutScript
{
    /// What the face screen takes from the games' facegen\si.ctl: the directions of its sliders and the directions of
    /// the age. The layout was read by FalloutNV.exe 0xAAA7D0 and checked byte for byte against the file by a parser
    /// of the Mac that ends exactly at the end of the file (Mac round 12BB A):
    ///   char[8] "FRCTL001", u32 id of the geometry basis, u32 id of the texture basis, u32[4] lengths of the rows
    ///   (50, 30, 50, 0), then four lists of controls (the symmetric shape, the asymmetric shape, the symmetric tone
    ///   and the asymmetric tone, which is empty): u32 count, and for each control float[length] and a u32 length with
    ///   that many bytes of name. Then the fan controls: 5 types x 2 groups x 2 kinds of (float[50], float offset), 20
    ///   records of (float[50], float[50], float offset) for the pairs of 5 types, and 5 records of 60400 bytes that
    ///   nothing reads. Only the first type is ever used; the files of both games are the same.
    struct FaceControlFile
    {
        FaceControls mControls;
        FaceAge mAge;
        /// The number of controls of the asymmetric shape list, which no slider reads
        int mAsymmetricCount = 0;
    };

    /// Nothing when the bytes are not that file (anything but its exact size and layout)
    std::optional<FaceControlFile> readFaceControlFile(std::string_view bytes);
}

#endif
