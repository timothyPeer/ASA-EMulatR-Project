#pragma once
// PALcode types
enum PalcodeType
{
    PAL_TYPE_VMS,   // OpenVMS PALcode
    PAL_TYPE_UNIX,  // Tru64/Digital UNIX PALcode
    PAL_TYPE_NT,    // Windows NT PALcode
    PAL_TYPE_SRM,   // SRM Console PALcode
    PAL_TYPE_CUSTOM // Custom PALcode implementation
};
