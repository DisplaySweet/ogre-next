/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include "OgrePixelFormatGpuUtils.h"
#include "OgreTextureBox.h"
#include "OgreMath.h"
#include "OgreBitwise.h"
#include "OgreCommon.h"
#include "OgreException.h"

namespace Ogre
{
#if OGRE_COMPILER == OGRE_COMPILER_MSVC && OGRE_COMP_VER < 1800
    inline float roundf( float x )
    {
        return x >= 0.0f ? floorf( x + 0.5f ) : ceilf( x - 0.5f );
    }
#endif

    inline const PixelFormatGpuUtils::PixelFormatDesc& PixelFormatGpuUtils::getDescriptionFor(
            const PixelFormatGpu fmt )
    {
        const int idx = (int)fmt;
        assert( idx >=0 && idx < PFG_COUNT );

        return msPixelFormatDesc[idx];
    }
    //-----------------------------------------------------------------------------------
    size_t PixelFormatGpuUtils::getBytesPerPixel( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return desc.bytesPerPixel;
    }
    //-----------------------------------------------------------------------------------
    size_t PixelFormatGpuUtils::getNumberOfComponents( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return desc.components;
    }
    //-----------------------------------------------------------------------------------
    size_t PixelFormatGpuUtils::getSizeBytes( uint32 width, uint32 height, uint32 depth,
                                              uint32 slices, PixelFormatGpu format,
                                              uint32 rowAlignment )
    {
        if( !isCompressed( format ) )
        {
            size_t retVal = width * getBytesPerPixel( format );
            retVal = alignToNextMultiple( retVal, rowAlignment );

            retVal *= height * depth * slices;

            return retVal;
        }
        else
        {
            switch( format )
            {
            // BCn formats work by dividing the image into 4x4 blocks, then
            // encoding each 4x4 block with a certain number of bytes.
            case PFG_BC1_UNORM:
            case PFG_BC1_UNORM_SRGB:
            case PFG_BC4_UNORM:
            case PFG_BC4_SNORM:
            case PFG_EAC_R11_UNORM:
            case PFG_EAC_R11_SNORM:
            case PFG_ETC1_RGB8_UNORM:
            case PFG_ETC2_RGB8_UNORM_SRGB:
            case PFG_ETC2_RGB8A1_UNORM:
            case PFG_ETC2_RGB8A1_UNORM_SRGB:
            case PFG_ATC_RGB:
                return ( (width + 3u) / 4u ) * ( (height + 3u) / 4u ) * 8u * depth * slices;
            case PFG_BC2_UNORM:
            case PFG_BC2_UNORM_SRGB:
            case PFG_BC3_UNORM:
            case PFG_BC3_UNORM_SRGB:
            case PFG_BC5_SNORM:
            case PFG_BC5_UNORM:
            case PFG_BC6H_SF16:
            case PFG_BC6H_UF16:
            case PFG_BC7_UNORM:
            case PFG_BC7_UNORM_SRGB:
            case PFG_ETC2_RGBA8_UNORM:
            case PFG_ETC2_RGBA8_UNORM_SRGB:
            case PFG_EAC_R11G11_UNORM:
            case PFG_EAC_R11G11_SNORM:
            case PFG_ATC_RGBA_EXPLICIT_ALPHA:
            case PFG_ATC_RGBA_INTERPOLATED_ALPHA:
                return ( (width + 3u) / 4u) * ( (height + 3u) / 4u ) * 16u * depth * slices;
            // Size calculations from the PVRTC OpenGL extension spec
            // http://www.khronos.org/registry/gles/extensions/IMG/IMG_texture_compression_pvrtc.txt
            // Basically, 32 bytes is the minimum texture size.  Smaller textures are padded up to 32 bytes
            case PFG_PVRTC_RGB2:
            case PFG_PVRTC_RGBA2:
            case PFG_PVRTC2_2BPP:
                return (std::max<uint32>( width, 16u ) * std::max<uint32>( height, 8u ) * 2u + 7u) / 8u
                        * depth * slices;
            case PFG_PVRTC_RGB4:
            case PFG_PVRTC_RGBA4:
            case PFG_PVRTC2_4BPP:
                return (std::max<uint32>( width, 8u ) * std::max<uint32>( height, 8u ) * 4u + 7u) / 8u
                        * depth * slices;
            default:
                OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS,
                             "Invalid compressed pixel format",
                             "PixelFormatGpuUtils::getSizeBytes" );
            }
        }
    }
    //-----------------------------------------------------------------------------------
    size_t PixelFormatGpuUtils::calculateSizeBytes( uint32 width, uint32 height, uint32 depth,
                                                    uint32 slices, PixelFormatGpu format,
                                                    uint8 numMipmaps, uint32 rowAlignment )
    {
        size_t totalBytes = 0;
        while( (width > 1u || height > 1u || depth > 1u) && numMipmaps > 0 )
        {
            totalBytes += PixelFormatGpuUtils::getSizeBytes( width, height, depth, slices,
                                                             format, rowAlignment );
            width   = std::max( 1u, width  >> 1u );
            height  = std::max( 1u, height >> 1u );
            depth   = std::max( 1u, depth  >> 1u );
            --numMipmaps;
        }

        return totalBytes;
    }
    //-----------------------------------------------------------------------
    uint8 PixelFormatGpuUtils::getMaxMipmapCount( uint32 maxResolution )
    {
        if( !maxResolution ) //log( 0 ) is undefined.
            return 0;

        uint8 numMipmaps;
#if (ANDROID || (OGRE_COMPILER == OGRE_COMPILER_MSVC && OGRE_COMP_VER < 1800))
        numMipmaps = static_cast<uint8>( floorf( logf( static_cast<float>(maxResolution) ) /
                                                 logf( 2.0f ) ) );
#else
        numMipmaps = static_cast<uint8>( floorf( log2f( static_cast<float>(maxResolution) ) ) );
#endif
        return numMipmaps + 1u;
    }
    //-----------------------------------------------------------------------
    uint8 PixelFormatGpuUtils::getMaxMipmapCount( uint32 width, uint32 height )
    {
        return getMaxMipmapCount( std::max( width, height ) );
    }
    //-----------------------------------------------------------------------
    uint8 PixelFormatGpuUtils::getMaxMipmapCount( uint32 width, uint32 height, uint32 depth )
    {
        return getMaxMipmapCount( std::max( std::max( width, height ), depth ) );
    }
    //-----------------------------------------------------------------------
    uint32 PixelFormatGpuUtils::getCompressedBlockWidth( PixelFormatGpu format, bool apiStrict )
    {
        switch( format )
        {
            // These formats work by dividing the image into 4x4 blocks, then encoding each
            // 4x4 block with a certain number of bytes.
            case PFG_BC1_UNORM: case PFG_BC1_UNORM_SRGB:
            case PFG_BC2_UNORM: case PFG_BC2_UNORM_SRGB:
            case PFG_BC3_UNORM: case PFG_BC3_UNORM_SRGB:
            case PFG_BC4_UNORM: case PFG_BC4_SNORM:
            case PFG_BC5_UNORM: case PFG_BC5_SNORM:
            case PFG_BC6H_UF16: case PFG_BC6H_SF16:
            case PFG_BC7_UNORM: case PFG_BC7_UNORM_SRGB:
            case PFG_ETC2_RGB8_UNORM: case PFG_ETC2_RGB8_UNORM_SRGB:
            case PFG_ETC2_RGBA8_UNORM: case PFG_ETC2_RGBA8_UNORM_SRGB:
            case PFG_ETC2_RGB8A1_UNORM: case PFG_ETC2_RGB8A1_UNORM_SRGB:
            case PFG_EAC_R11_UNORM: case PFG_EAC_R11_SNORM:
            case PFG_EAC_R11G11_UNORM: case PFG_EAC_R11G11_SNORM:
            case PFG_ATC_RGB:
            case PFG_ATC_RGBA_EXPLICIT_ALPHA:
            case PFG_ATC_RGBA_INTERPOLATED_ALPHA:
                return 4u;

            case PFG_ETC1_RGB8_UNORM:
                return apiStrict ? 0u : 4u;

            // Size calculations from the PVRTC OpenGL extension spec
            // http://www.khronos.org/registry/gles/extensions/IMG/IMG_texture_compression_pvrtc.txt
            //  "Sub-images are not supportable because the PVRTC
            //  algorithm uses significant adjacency information, so there is
            //  no discrete block of texels that can be decoded as a standalone
            //  sub-unit, and so it follows that no stand alone sub-unit of
            //  data can be loaded without changing the decoding of surrounding
            //  texels."
            // In other words, if the user wants atlas, they can't be automatic
            case PFG_PVRTC_RGB2: case PFG_PVRTC_RGBA2:
            case PFG_PVRTC_RGB4: case PFG_PVRTC_RGBA4:
            case PFG_PVRTC2_2BPP: case PFG_PVRTC2_4BPP:
                return 0u;

            default:
                assert( !isCompressed( format ) );
                return 1u;
        }
    }
    //-----------------------------------------------------------------------
    uint32 PixelFormatGpuUtils::getCompressedBlockHeight( PixelFormatGpu format, bool apiStrict )
    {
        return getCompressedBlockWidth( format, apiStrict );
    }
    //-----------------------------------------------------------------------------------
    const char* PixelFormatGpuUtils::toString( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return desc.name;
    }
    //-----------------------------------------------------------------------------------
    PixelFormatGpu PixelFormatGpuUtils::getFormatFromName( const String &name, uint32 exclusionFlags )
    {
        return getFormatFromName( name.c_str(), exclusionFlags );
    }
    //-----------------------------------------------------------------------------------
    PixelFormatGpu PixelFormatGpuUtils::getFormatFromName( const char *name, uint32 exclusionFlags )
    {
        for( int i=0; i<PFG_COUNT; ++i )
        {
            PixelFormatGpu format = static_cast<PixelFormatGpu>(i);

            const PixelFormatDesc &desc = getDescriptionFor( format );

            if( (desc.flags & exclusionFlags) == 0 )
            {
                if( strcmp( name, desc.name ) == 0 )
                    return format;
            }
        }

        return PFG_UNKNOWN;
    }
    //-----------------------------------------------------------------------------------
    float PixelFormatGpuUtils::toSRGB( float x )
    {
        if( x <= 0.0031308f )
            return 12.92f * x;
        else
            return 1.055f * powf( x, (1.0f / 2.4f) ) - 0.055f;
    }
    //-----------------------------------------------------------------------------------
    float PixelFormatGpuUtils::fromSRGB( float x )
    {
        if( x <= 0.040449907f )
            return x / 12.92f;
        else
            return powf( (x + 0.055f) / 1.055f, 2.4f );
    }
    //-----------------------------------------------------------------------------------
    template <typename T>
    void PixelFormatGpuUtils::convertFromFloat( const float *rgbaPtr, void *dstPtr,
                                                size_t numComponents, uint32 flags )
    {
        for( size_t i=0; i<numComponents; ++i )
        {
            if( flags & PFF_FLOAT )
                ((float*)dstPtr)[i] = rgbaPtr[i];
            else if( flags & PFF_HALF )
                ((uint16*)dstPtr)[i] = Bitwise::floatToHalf( rgbaPtr[i] );
            else if( flags & PFF_NORMALIZED )
            {
                float val = rgbaPtr[i];
                if( !(flags & PFF_SIGNED) )
                {
                    val = Math::saturate( val );
                    if( flags & PFF_SRGB )
                        val = toSRGB( val );
                    val *= (float)std::numeric_limits<T>::max();
                    ((T*)dstPtr)[i] = static_cast<T>( roundf( val ) );
                }
                else
                {
                    val = Math::Clamp( val, -1.0f, 1.0f );
                    val *= (float)std::numeric_limits<T>::max();
                    ((T*)dstPtr)[i] = static_cast<T>( roundf( val ) );
                }
            }
            else
                ((T*)dstPtr)[i] = static_cast<T>( roundf( rgbaPtr[i] ) );
        }
    }
    //-----------------------------------------------------------------------------------
    template <typename T>
    void PixelFormatGpuUtils::convertToFloat( float *rgbaPtr, const void *srcPtr,
                                              size_t numComponents, uint32 flags )
    {
        for( size_t i=0; i<numComponents; ++i )
        {
            if( flags & PFF_FLOAT )
                rgbaPtr[i] = ((const float*)srcPtr)[i];
            else if( flags & PFF_HALF )
                rgbaPtr[i] = Bitwise::halfToFloat( ((const uint16*)srcPtr)[i] );
            else if( flags & PFF_NORMALIZED )
            {
                const float val = static_cast<float>( ((const T*)srcPtr)[i] );
                float rawValue = val / (float)std::numeric_limits<T>::max();
                if( !(flags & PFF_SIGNED) )
                {
                    if( flags & PFF_SRGB )
                        rawValue = fromSRGB( rawValue );
                    rgbaPtr[i] = rawValue;
                }
                else
                {
                    // -128 & -127 and -32768 & -32767 both map to -1 according to D3D10 rules.
                    rgbaPtr[i] = Ogre::max( rawValue, -1.0f );
                }
            }
            else
                rgbaPtr[i] = static_cast<float>( ((const T*)srcPtr)[i] );
        }

        //Set remaining components to 0, and alpha to 1
        for( size_t i=numComponents; i<3u; ++i )
            rgbaPtr[i] = 0.0f;
        if( numComponents < 4u )
            rgbaPtr[3] = 1.0f;
    }
    //-----------------------------------------------------------------------------------
    void PixelFormatGpuUtils::packColour( const float *rgbaPtr, PixelFormatGpu pf, void *dstPtr )
    {
        const uint32 flags = getFlags( pf );
        switch( pf )
        {
        case PFG_RGBA32_FLOAT:
            convertFromFloat<float>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGBA32_UINT:
            convertFromFloat<uint32>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGBA32_SINT:
            convertFromFloat<int32>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGB32_FLOAT:
            convertFromFloat<float>( rgbaPtr, dstPtr, 3u, flags );
            break;
        case PFG_RGB32_UINT:
            convertFromFloat<uint32>( rgbaPtr, dstPtr, 3u, flags );
            break;
        case PFG_RGB32_SINT:
            convertFromFloat<int32>( rgbaPtr, dstPtr, 3u, flags );
            break;
        case PFG_RGBA16_FLOAT:
            convertFromFloat<uint16>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGBA16_UNORM:
            convertFromFloat<uint16>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGBA16_UINT:
            convertFromFloat<uint16>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGBA16_SNORM:
            convertFromFloat<int16>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGBA16_SINT:
            convertFromFloat<int16>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RG32_FLOAT:
            convertFromFloat<float>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_RG32_UINT:
            convertFromFloat<uint32>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_RG32_SINT:
            convertFromFloat<int32>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_D32_FLOAT_S8X24_UINT:
            ((float*)dstPtr)[0] = rgbaPtr[0];
            ((uint32*)dstPtr)[1] = static_cast<uint32>( rgbaPtr[1] ) << 24u;
            break;
        case PFG_R10G10B10A2_UNORM:
        {
            const uint16 ir = static_cast<uint16>( Math::saturate( rgbaPtr[0] ) * 1023.0f + 0.5f );
            const uint16 ig = static_cast<uint16>( Math::saturate( rgbaPtr[1] ) * 1023.0f + 0.5f );
            const uint16 ib = static_cast<uint16>( Math::saturate( rgbaPtr[2] ) * 1023.0f + 0.5f );
            const uint16 ia = static_cast<uint16>( Math::saturate( rgbaPtr[3] ) * 3.0f + 0.5f );

            ((uint32*)dstPtr)[0] = (ia << 30u) | (ib << 20u) | (ig << 10u) | (ir);
            break;
        }
        case PFG_R10G10B10A2_UINT:
        {
            const uint16 ir = static_cast<uint16>( Math::Clamp( rgbaPtr[0], 0.0f, 1023.0f ) );
            const uint16 ig = static_cast<uint16>( Math::Clamp( rgbaPtr[1], 0.0f, 1023.0f ) );
            const uint16 ib = static_cast<uint16>( Math::Clamp( rgbaPtr[2], 0.0f, 1023.0f ) );
            const uint16 ia = static_cast<uint16>( Math::Clamp( rgbaPtr[3], 0.0f, 3.0f ) );

            ((uint32*)dstPtr)[0] = (ia << 30u) | (ib << 20u) | (ig << 10u) | (ir);
            break;
        }
        case PFG_R11G11B10_FLOAT:
            OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED, "PFG_R11G11B10_FLOAT",
                         "PixelFormatGpuUtils::packColour" );
            break;
        case PFG_RGBA8_UNORM:
            convertFromFloat<uint8>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGBA8_UNORM_SRGB:
            convertFromFloat<uint8>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGBA8_UINT:
            convertFromFloat<uint8>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGBA8_SNORM:
            convertFromFloat<int8>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RGBA8_SINT:
            convertFromFloat<int8>( rgbaPtr, dstPtr, 4u, flags );
            break;
        case PFG_RG16_FLOAT:
            convertFromFloat<uint16>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_RG16_UNORM:
            convertFromFloat<uint16>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_RG16_UINT:
            convertFromFloat<uint16>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_RG16_SNORM:
            convertFromFloat<int16>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_RG16_SINT:
            convertFromFloat<int16>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_D32_FLOAT:
            convertFromFloat<float>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R32_FLOAT:
            convertFromFloat<float>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R32_UINT:
            convertFromFloat<uint32>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R32_SINT:
            convertFromFloat<int32>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_D24_UNORM:
            ((uint32*)dstPtr)[0] = static_cast<uint32>( roundf( rgbaPtr[0] * 16777215.0f ) );
            break;
        case PFG_D24_UNORM_S8_UINT:
            ((uint32*)dstPtr)[0] = (static_cast<uint32>( rgbaPtr[1] ) << 24u) |
                    static_cast<uint32>( roundf( rgbaPtr[0] * 16777215.0f ) );
            break;
        case PFG_RG8_UNORM:
            convertFromFloat<uint8>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_RG8_UINT:
            convertFromFloat<uint8>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_RG8_SNORM:
            convertFromFloat<int8>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_RG8_SINT:
            convertFromFloat<int8>( rgbaPtr, dstPtr, 2u, flags );
            break;
        case PFG_R16_FLOAT:
            convertFromFloat<uint16>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_D16_UNORM:
            convertFromFloat<uint16>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R16_UNORM:
            convertFromFloat<uint16>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R16_UINT:
            convertFromFloat<uint16>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R16_SNORM:
            convertFromFloat<int16>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R16_SINT:
            convertFromFloat<int16>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R8_UNORM:
            convertFromFloat<uint8>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R8_UINT:
            convertFromFloat<uint8>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R8_SNORM:
            convertFromFloat<int8>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R8_SINT:
            convertFromFloat<int8>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_A8_UNORM:
            convertFromFloat<uint8>( rgbaPtr, dstPtr, 1u, flags );
            break;
        case PFG_R1_UNORM:
        case PFG_R9G9B9E5_SHAREDEXP:
        case PFG_R8G8_B8G8_UNORM:
        case PFG_G8R8_G8B8_UNORM:
            OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED, "PFG_R9G9B9E5_SHAREDEXP",
                         "PixelFormatGpuUtils::packColour" );
            break;
        case PFG_B5G6R5_UNORM:
        {
            const uint8 ir = static_cast<uint8>( Math::saturate( rgbaPtr[0] ) * 31.0f + 0.5f );
            const uint8 ig = static_cast<uint8>( Math::saturate( rgbaPtr[1] ) * 63.0f + 0.5f );
            const uint8 ib = static_cast<uint8>( Math::saturate( rgbaPtr[2] ) * 31.0f + 0.5f );

            ((uint16*)dstPtr)[0] = (ir << 11u) | (ig << 5u) | (ib);
            break;
        }
        case PFG_B5G5R5A1_UNORM:
        {
            const uint8 ir = static_cast<uint8>( Math::saturate( rgbaPtr[0] ) * 31.0f + 0.5f );
            const uint8 ig = static_cast<uint8>( Math::saturate( rgbaPtr[1] ) * 31.0f + 0.5f );
            const uint8 ib = static_cast<uint8>( Math::saturate( rgbaPtr[2] ) * 31.0f + 0.5f );
            const uint8 ia = rgbaPtr[3] == 0.0f ? 0u : 1u;

            ((uint16*)dstPtr)[0] = (ia << 15u) | (ir << 10u) | (ig << 5u) | (ib);
            break;
        }
        case PFG_BGRA8_UNORM:
            ((uint8*)dstPtr)[0] = static_cast<uint8>( Math::saturate( rgbaPtr[2] ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[1] = static_cast<uint8>( Math::saturate( rgbaPtr[1] ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[2] = static_cast<uint8>( Math::saturate( rgbaPtr[0] ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[3] = static_cast<uint8>( Math::saturate( rgbaPtr[3] ) * 255.0f + 0.5f );
            break;
        case PFG_BGRX8_UNORM:
            ((uint8*)dstPtr)[0] = static_cast<uint8>( Math::saturate( rgbaPtr[2] ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[1] = static_cast<uint8>( Math::saturate( rgbaPtr[1] ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[2] = static_cast<uint8>( Math::saturate( rgbaPtr[0] ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[3] = 255u;
            break;
        case PFG_R10G10B10_XR_BIAS_A2_UNORM:
            OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED, "PFG_R10G10B10_XR_BIAS_A2_UNORM",
                         "PixelFormatGpuUtils::packColour" );
            break;
        case PFG_BGRA8_UNORM_SRGB:
            ((uint8*)dstPtr)[0] =
                    static_cast<uint8>( Math::saturate( toSRGB( rgbaPtr[2] ) ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[1] =
                    static_cast<uint8>( Math::saturate( toSRGB( rgbaPtr[1] ) ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[2] =
                    static_cast<uint8>( Math::saturate( toSRGB( rgbaPtr[0] ) ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[3] = static_cast<uint8>( Math::saturate( rgbaPtr[3] ) * 255.0f + 0.5f );
            break;
        case PFG_BGRX8_UNORM_SRGB:
            ((uint8*)dstPtr)[0] =
                    static_cast<uint8>( Math::saturate( toSRGB( rgbaPtr[2] ) ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[1] =
                    static_cast<uint8>( Math::saturate( toSRGB( rgbaPtr[1] ) ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[2] =
                    static_cast<uint8>( Math::saturate( toSRGB( rgbaPtr[0] ) ) * 255.0f + 0.5f );
            ((uint8*)dstPtr)[3] = 255u;
            break;
        case PFG_B4G4R4A4_UNORM:
        {
            const uint8 ir = static_cast<uint8>( Math::saturate( rgbaPtr[0] ) * 15.0f + 0.5f );
            const uint8 ig = static_cast<uint8>( Math::saturate( rgbaPtr[1] ) * 15.0f + 0.5f );
            const uint8 ib = static_cast<uint8>( Math::saturate( rgbaPtr[2] ) * 15.0f + 0.5f );
            const uint8 ia = static_cast<uint8>( Math::saturate( rgbaPtr[2] ) * 15.0f + 0.5f );

            ((uint16*)dstPtr)[0] = (ia << 12u) | (ir << 8u) | (ig << 4u) | (ib);
            break;
        }

        case PFG_AYUV: case PFG_Y410: case PFG_Y416:
        case PFG_NV12: case PFG_P010: case PFG_P016:
        case PFG_420_OPAQUE:
        case PFG_YUY2: case PFG_Y210: case PFG_Y216:
        case PFG_NV11: case PFG_AI44: case PFG_IA44:
        case PFG_P8:   case PFG_A8P8:
        case PFG_P208: case PFG_V208: case PFG_V408:
        case PFG_UNKNOWN: case PFG_COUNT:
            OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED, "",
                         "PixelFormatGpuUtils::packColour" );
            break;

        case PFG_BC1_UNORM: case PFG_BC1_UNORM_SRGB:
        case PFG_BC2_UNORM: case PFG_BC2_UNORM_SRGB:
        case PFG_BC3_UNORM: case PFG_BC3_UNORM_SRGB:
        case PFG_BC4_UNORM: case PFG_BC4_SNORM:
        case PFG_BC5_UNORM: case PFG_BC5_SNORM:
        case PFG_BC6H_UF16: case PFG_BC6H_SF16:
        case PFG_BC7_UNORM: case PFG_BC7_UNORM_SRGB:
        case PFG_PVRTC_RGB2: case PFG_PVRTC_RGBA2:
        case PFG_PVRTC_RGB4: case PFG_PVRTC_RGBA4:
        case PFG_PVRTC2_2BPP: case PFG_PVRTC2_4BPP:
        case PFG_ETC1_RGB8_UNORM:
        case PFG_ETC2_RGB8_UNORM: case PFG_ETC2_RGB8_UNORM_SRGB:
        case PFG_ETC2_RGBA8_UNORM: case PFG_ETC2_RGBA8_UNORM_SRGB:
        case PFG_ETC2_RGB8A1_UNORM: case PFG_ETC2_RGB8A1_UNORM_SRGB:
        case PFG_EAC_R11_UNORM: case PFG_EAC_R11_SNORM:
        case PFG_EAC_R11G11_UNORM: case PFG_EAC_R11G11_SNORM:
        case PFG_ATC_RGB:
        case PFG_ATC_RGBA_EXPLICIT_ALPHA:
        case PFG_ATC_RGBA_INTERPOLATED_ALPHA:
            OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS, "Compressed formats not supported!",
                         "PixelFormatGpuUtils::packColour" );
            break;
        }
    }
    //-----------------------------------------------------------------------------------
    void PixelFormatGpuUtils::unpackColour( float *rgbaPtr, PixelFormatGpu pf, const void* srcPtr )
    {
        const uint32 flags = getFlags( pf );
        switch( pf )
        {
        case PFG_RGBA32_FLOAT:
            convertToFloat<float>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGBA32_UINT:
            convertToFloat<uint32>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGBA32_SINT:
            convertToFloat<int32>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGB32_FLOAT:
            convertToFloat<float>( rgbaPtr, srcPtr, 3u, flags );
            break;
        case PFG_RGB32_UINT:
            convertToFloat<uint32>( rgbaPtr, srcPtr, 3u, flags );
            break;
        case PFG_RGB32_SINT:
            convertToFloat<int32>( rgbaPtr, srcPtr, 3u, flags );
            break;
        case PFG_RGBA16_FLOAT:
            convertToFloat<uint16>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGBA16_UNORM:
            convertToFloat<uint16>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGBA16_UINT:
            convertToFloat<uint16>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGBA16_SNORM:
            convertToFloat<int16>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGBA16_SINT:
            convertToFloat<int16>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RG32_FLOAT:
            convertToFloat<float>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_RG32_UINT:
            convertToFloat<uint32>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_RG32_SINT:
            convertToFloat<int32>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_D32_FLOAT_S8X24_UINT:
            rgbaPtr[0] = ((const float*)srcPtr)[0];
            rgbaPtr[1] = static_cast<float>( ((const uint32*)srcPtr)[1] >> 24u );
            rgbaPtr[2] = 0.0f;
            rgbaPtr[3] = 1.0f;
            break;
        case PFG_R10G10B10A2_UNORM:
        {
            const uint32 val = ((const uint32*)srcPtr)[0];
            rgbaPtr[0] = static_cast<float>( val & 0x3FF ) / 1023.0f;
            rgbaPtr[1] = static_cast<float>( (val >> 10u) & 0x3FF ) / 1023.0f;
            rgbaPtr[2] = static_cast<float>( (val >> 20u) & 0x3FF ) / 1023.0f;
            rgbaPtr[3] = static_cast<float>( val >> 30u ) / 3.0f;
            break;
        }
        case PFG_R10G10B10A2_UINT:
        {
            const uint32 val = ((const uint32*)srcPtr)[0];
            rgbaPtr[0] = static_cast<float>( val & 0x3FF );
            rgbaPtr[1] = static_cast<float>( (val >> 10u) & 0x3FF );
            rgbaPtr[2] = static_cast<float>( (val >> 20u) & 0x3FF );
            rgbaPtr[3] = static_cast<float>( val >> 30u );
            break;
        }
        case PFG_R11G11B10_FLOAT:
            OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED, "PFG_R11G11B10_FLOAT",
                         "PixelFormatGpuUtils::unpackColour" );
            break;
        case PFG_RGBA8_UNORM:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGBA8_UNORM_SRGB:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGBA8_UINT:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGBA8_SNORM:
            convertToFloat<int8>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RGBA8_SINT:
            convertToFloat<int8>( rgbaPtr, srcPtr, 4u, flags );
            break;
        case PFG_RG16_FLOAT:
            convertToFloat<uint16>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_RG16_UNORM:
            convertToFloat<uint16>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_RG16_UINT:
            convertToFloat<uint16>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_RG16_SNORM:
            convertToFloat<int16>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_RG16_SINT:
            convertToFloat<int16>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_D32_FLOAT:
            convertToFloat<float>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R32_FLOAT:
            convertToFloat<float>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R32_UINT:
            convertToFloat<uint32>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R32_SINT:
            convertToFloat<int32>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_D24_UNORM:
            rgbaPtr[0] = static_cast<float>( ((const uint32*)srcPtr)[0] ) / 16777215.0f;
            rgbaPtr[1] = 0.0f;
            rgbaPtr[2] = 0.0f;
            rgbaPtr[3] = 1.0f;
            break;
        case PFG_D24_UNORM_S8_UINT:
            rgbaPtr[0] = static_cast<float>( ((const uint32*)srcPtr)[0] & 0x00FFFFFF ) / 16777215.0f;
            rgbaPtr[1] = static_cast<float>( ((const uint32*)srcPtr)[0] >> 24u );
            rgbaPtr[2] = 0.0f;
            rgbaPtr[3] = 1.0f;
            break;
        case PFG_RG8_UNORM:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_RG8_UINT:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_RG8_SNORM:
            convertToFloat<int8>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_RG8_SINT:
            convertToFloat<int8>( rgbaPtr, srcPtr, 2u, flags );
            break;
        case PFG_R16_FLOAT:
            convertToFloat<uint16>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_D16_UNORM:
            convertToFloat<uint16>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R16_UNORM:
            convertToFloat<uint16>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R16_UINT:
            convertToFloat<uint16>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R16_SNORM:
            convertToFloat<int16>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R16_SINT:
            convertToFloat<int16>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R8_UNORM:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R8_UINT:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R8_SNORM:
            convertToFloat<int8>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_R8_SINT:
            convertToFloat<int8>( rgbaPtr, srcPtr, 1u, flags );
            break;
        case PFG_A8_UNORM:
            rgbaPtr[0] = 0;
            rgbaPtr[1] = 0;
            rgbaPtr[2] = 0;
            rgbaPtr[3] = static_cast<float>( ((const uint32*)srcPtr)[0] );
            break;
        case PFG_R1_UNORM:
        case PFG_R9G9B9E5_SHAREDEXP:
        case PFG_R8G8_B8G8_UNORM:
        case PFG_G8R8_G8B8_UNORM:
            OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED, "PFG_R9G9B9E5_SHAREDEXP",
                         "PixelFormatGpuUtils::unpackColour" );
            break;
        case PFG_B5G6R5_UNORM:
        {
            const uint16 val = ((const uint16*)srcPtr)[0];
            rgbaPtr[0] = static_cast<float>( (val >> 11u) & 0x1F ) / 31.0f;
            rgbaPtr[1] = static_cast<float>( (val >>  5u) & 0x3F ) / 63.0f;
            rgbaPtr[2] = static_cast<float>( val & 0x1F ) / 31.0f;
            rgbaPtr[3] = 1.0f;
            break;
        }
        case PFG_B5G5R5A1_UNORM:
        {
            const uint16 val = ((const uint16*)srcPtr)[0];
            rgbaPtr[0] = static_cast<float>( (val >> 10u) & 0x1F ) / 31.0f;
            rgbaPtr[1] = static_cast<float>( (val >>  5u) & 0x1F ) / 31.0f;
            rgbaPtr[2] = static_cast<float>( val & 0x1F ) / 31.0f;
            rgbaPtr[3] = (val >> 15u) == 0 ? 0.0f : 1.0f;
            break;
        }
        case PFG_BGRA8_UNORM:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 4u, flags );
            std::swap( rgbaPtr[0], rgbaPtr[2] );
            break;
        case PFG_BGRX8_UNORM:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 4u, flags );
            std::swap( rgbaPtr[0], rgbaPtr[2] );
            break;
        case PFG_R10G10B10_XR_BIAS_A2_UNORM:
            OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED, "PFG_R10G10B10_XR_BIAS_A2_UNORM",
                         "PixelFormatGpuUtils::unpackColour" );
            break;
        case PFG_BGRA8_UNORM_SRGB:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 4u, flags );
            std::swap( rgbaPtr[0], rgbaPtr[2] );
            break;
        case PFG_BGRX8_UNORM_SRGB:
            convertToFloat<uint8>( rgbaPtr, srcPtr, 3u, flags );
            std::swap( rgbaPtr[0], rgbaPtr[2] );
            break;
        case PFG_B4G4R4A4_UNORM:
        {
            const uint16 val = ((const uint16*)srcPtr)[0];
            rgbaPtr[0] = static_cast<float>( (val >>  8u) & 0xF ) / 15.0f;
            rgbaPtr[1] = static_cast<float>( (val >>  4u) & 0xF ) / 15.0f;
            rgbaPtr[2] = static_cast<float>( val & 0xF ) / 15.0f;
            rgbaPtr[3] = static_cast<float>( (val >> 12u) & 0xF ) / 15.0f;
            break;
        }

        case PFG_AYUV: case PFG_Y410: case PFG_Y416:
        case PFG_NV12: case PFG_P010: case PFG_P016:
        case PFG_420_OPAQUE:
        case PFG_YUY2: case PFG_Y210: case PFG_Y216:
        case PFG_NV11: case PFG_AI44: case PFG_IA44:
        case PFG_P8:   case PFG_A8P8:
        case PFG_P208: case PFG_V208: case PFG_V408:
        case PFG_UNKNOWN: case PFG_COUNT:
            OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED, "",
                         "PixelFormatGpuUtils::unpackColour" );
            break;

        case PFG_BC1_UNORM: case PFG_BC1_UNORM_SRGB:
        case PFG_BC2_UNORM: case PFG_BC2_UNORM_SRGB:
        case PFG_BC3_UNORM: case PFG_BC3_UNORM_SRGB:
        case PFG_BC4_UNORM: case PFG_BC4_SNORM:
        case PFG_BC5_UNORM: case PFG_BC5_SNORM:
        case PFG_BC6H_UF16: case PFG_BC6H_SF16:
        case PFG_BC7_UNORM: case PFG_BC7_UNORM_SRGB:
        case PFG_PVRTC_RGB2: case PFG_PVRTC_RGBA2:
        case PFG_PVRTC_RGB4: case PFG_PVRTC_RGBA4:
        case PFG_PVRTC2_2BPP: case PFG_PVRTC2_4BPP:
        case PFG_ETC1_RGB8_UNORM:
        case PFG_ETC2_RGB8_UNORM: case PFG_ETC2_RGB8_UNORM_SRGB:
        case PFG_ETC2_RGBA8_UNORM: case PFG_ETC2_RGBA8_UNORM_SRGB:
        case PFG_ETC2_RGB8A1_UNORM: case PFG_ETC2_RGB8A1_UNORM_SRGB:
        case PFG_EAC_R11_UNORM: case PFG_EAC_R11_SNORM:
        case PFG_EAC_R11G11_UNORM: case PFG_EAC_R11G11_SNORM:
        case PFG_ATC_RGB:
        case PFG_ATC_RGBA_EXPLICIT_ALPHA:
        case PFG_ATC_RGBA_INTERPOLATED_ALPHA:
            OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS, "Compressed formats not supported!",
                         "PixelFormatGpuUtils::unpackColour" );
            break;
        }
    }
    //-----------------------------------------------------------------------------------
    void PixelFormatGpuUtils::bulkPixelConversion( const TextureBox &src, PixelFormatGpu srcFormat,
                                                   const TextureBox &dst, PixelFormatGpu dstFormat )
    {
        assert( src.equalSize( dst ) );

        if( src.bytesPerImage == dst.bytesPerImage &&
            srcFormat == dstFormat &&
            src.x == 0 && dst.x == 0 &&
            src.y == 0 && dst.y == 0 &&
            src.z == 0 && dst.z == 0 )
        {
            //Easy case. Raw copy.
            memcpy( dst.at( 0u, 0u, dst.sliceStart ),
                    src.at( 0u, 0u, src.sliceStart ),
                    src.bytesPerImage * src.numSlices );
            return;
        }

        // Check for compressed formats, we don't support decompression, compression or recoding
        if( isCompressed( srcFormat ) || isCompressed( dstFormat ) )
        {
            if( srcFormat == dstFormat )
            {
                const uint32 blockWidth  = getCompressedBlockWidth( dstFormat, false );
                const uint32 blockHeight = getCompressedBlockHeight( dstFormat, false );

                if( blockWidth == 0 || blockHeight == 0 )
                {
                    OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED,
                                 "This format should be consecutive!",
                                 "PixelFormatGpuUtils::bulkPixelConversion" );
                }

                uint8 *srcptr = static_cast<uint8*>(src.data) +
                                (src.x + blockWidth - 1u) / blockWidth +
                                (src.y + blockHeight - 1u) / blockHeight * src.bytesPerRow +
                                src.getZOrSlice() * src.bytesPerImage;
                uint8 *dstptr = static_cast<uint8*>(dst.data) +
                                (dst.x + blockWidth - 1u) / blockWidth +
                                (dst.y + blockHeight - 1u) / blockHeight * dst.bytesPerRow +
                                dst.getZOrSlice() * dst.bytesPerImage;

                // Calculate pitches+skips in bytes
                const size_t srcBytesPerRow     = src.bytesPerRow;
                const size_t srcSliceSkipBytes  = src.bytesPerImage;

                const size_t dstBytesPerRow     = dst.bytesPerRow;
                const size_t dstSliceSkipBytes  = dst.bytesPerImage;

                const size_t compressedSrcY = (src.y + blockHeight - 1u) / blockHeight;
                const size_t compressedSrcMaxY = (src.y + src.height + blockHeight - 1u) / blockHeight;

                for( size_t z=src.getZOrSlice(); z<src.getZOrSlice() + src.getMaxSlice(); ++z )
                {
                    uint8 *srcData = srcptr;
                    uint8 *dstData = dstptr;

                    for( size_t y=compressedSrcY; y<compressedSrcMaxY; ++y )
                    {
                        memcpy( dstData, srcData, srcBytesPerRow );
                        srcData += srcBytesPerRow;
                        dstData += dstBytesPerRow;
                    }

                    srcptr += srcSliceSkipBytes;
                    dstptr += dstSliceSkipBytes;
                }

                return;
            }
            else
            {
                OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED,
                             "This method can not be used to compress or decompress images",
                             "PixelFormatGpuUtils::bulkPixelConversion" );
            }
        }

        // The easy case
        if( srcFormat == dstFormat )
        {
            uint8 *srcData = reinterpret_cast<uint8*>( src.at( src.x, src.y, src.getZOrSlice() ) );
            uint8 *dstData = reinterpret_cast<uint8*>( dst.at( dst.x, dst.y, dst.getZOrSlice() ) );

            for( size_t z=0; z<src.getDepthOrSlices(); ++z )
            {
                uint8 *srcPtr = srcData + src.bytesPerImage * z;
                uint8 *dstPtr = dstData + dst.bytesPerImage * z;

                for( size_t y=0; y<src.height; ++y )
                {
                    memcpy( dstPtr, srcPtr, src.width * src.bytesPerPixel );
                    srcPtr += src.bytesPerRow;
                    dstPtr += dst.bytesPerRow;
                }
            }

            return;
        }

        // Is there a specialized, inlined, conversion?
        /*if(doOptimizedConversion(src, dst))
        {
            // If so, good
            return;
        }*/

        const size_t srcBytesPerPixel = src.bytesPerPixel;
        const size_t dstBytesPerPixel = dst.bytesPerPixel;

        uint8 *srcData = reinterpret_cast<uint8*>( src.at( src.x, src.y, src.getZOrSlice() ) );
        uint8 *dstData = reinterpret_cast<uint8*>( dst.at( dst.x, dst.y, dst.getZOrSlice() ) );

        const size_t width = src.width;
        const size_t height = src.height;
        const size_t depthOrSlices = src.getDepthOrSlices();

        // The brute force fallback
        float rgba[4];
        for( size_t z=0; z<depthOrSlices; ++z )
        {
            for( size_t y=0; y<height; ++y )
            {
                uint8 *srcPtr = srcData + src.bytesPerImage * z + src.bytesPerRow * y;
                uint8 *dstPtr = dstData + dst.bytesPerImage * z + dst.bytesPerRow * y;

                for( size_t x=0; x<width; ++x )
                {
                    unpackColour( rgba, srcFormat, srcPtr );
                    packColour( rgba, dstFormat, dstPtr );
                    srcPtr += srcBytesPerPixel;
                    dstPtr += dstBytesPerPixel;
                }
            }
        }
    }
    //-----------------------------------------------------------------------------------
    uint32 PixelFormatGpuUtils::getFlags( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return desc.flags;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isFloat( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_FLOAT) != 0;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isHalf( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_HALF) != 0;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isFloatRare( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_FLOAT_RARE) != 0;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isInteger( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_INTEGER) != 0;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isNormalized( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_NORMALIZED) != 0;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isSigned( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_SIGNED) != 0;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isDepth( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_DEPTH) != 0;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isStencil( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_STENCIL) != 0;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isSRgb( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_SRGB) != 0;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isCompressed( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_COMPRESSED) != 0;
    }
    //-----------------------------------------------------------------------------------
    bool PixelFormatGpuUtils::isPallete( PixelFormatGpu format )
    {
        const PixelFormatDesc &desc = getDescriptionFor( format );
        return (desc.flags & PFF_PALLETE) != 0;
    }
    //-----------------------------------------------------------------------------------
    PixelFormatGpu PixelFormatGpuUtils::getFamily( PixelFormatGpu format )
    {
        switch( format )
        {
        case PFG_RGBA32_FLOAT:
        case PFG_RGBA32_UINT:
        case PFG_RGBA32_SINT:
            return PFG_RGBA32_UINT;

        case PFG_RGB32_FLOAT:
        case PFG_RGB32_UINT:
        case PFG_RGB32_SINT:
            return PFG_RGB32_UINT;

        case PFG_RGBA16_FLOAT:
        case PFG_RGBA16_UNORM:
        case PFG_RGBA16_UINT:
        case PFG_RGBA16_SNORM:
        case PFG_RGBA16_SINT:
            return PFG_RGBA16_UINT;

        case PFG_RG32_FLOAT:
        case PFG_RG32_UINT:
        case PFG_RG32_SINT:
            return PFG_RG32_UINT;

        case PFG_R10G10B10A2_UNORM:
        case PFG_R10G10B10A2_UINT:
            return PFG_R10G10B10A2_UINT;

        case PFG_R11G11B10_FLOAT:
            return PFG_R11G11B10_FLOAT;

        case PFG_RGBA8_UNORM:
        case PFG_RGBA8_UNORM_SRGB:
        case PFG_RGBA8_UINT:
        case PFG_RGBA8_SNORM:
        case PFG_RGBA8_SINT:
            return PFG_RGBA8_UNORM;

        case PFG_RG16_FLOAT:
        case PFG_RG16_UNORM:
        case PFG_RG16_UINT:
        case PFG_RG16_SNORM:
        case PFG_RG16_SINT:
            return PFG_RG16_UINT;

        case PFG_D32_FLOAT:
        case PFG_R32_FLOAT:
        case PFG_R32_UINT:
        case PFG_R32_SINT:
            return PFG_R32_UINT;

        case PFG_D24_UNORM:
        case PFG_D24_UNORM_S8_UINT:
            return PFG_D24_UNORM_S8_UINT;

        case PFG_RG8_UNORM:
        case PFG_RG8_UINT:
        case PFG_RG8_SNORM:
        case PFG_RG8_SINT:
            return PFG_RG8_UINT;

        case PFG_R16_FLOAT:
        case PFG_D16_UNORM:
        case PFG_R16_UNORM:
        case PFG_R16_UINT:
        case PFG_R16_SNORM:
        case PFG_R16_SINT:
            return PFG_R16_UINT;

        case PFG_R8_UNORM:
        case PFG_R8_UINT:
        case PFG_R8_SNORM:
        case PFG_R8_SINT:
        case PFG_A8_UNORM:
            return PFG_R8_UINT;

        case PFG_R8G8_B8G8_UNORM:
        case PFG_G8R8_G8B8_UNORM:
            return PFG_R8G8_B8G8_UNORM;

        case PFG_BC1_UNORM:
        case PFG_BC1_UNORM_SRGB:
            return PFG_BC1_UNORM;
        case PFG_BC2_UNORM:
        case PFG_BC2_UNORM_SRGB:
            return PFG_BC2_UNORM;
        case PFG_BC3_UNORM:
        case PFG_BC3_UNORM_SRGB:
            return PFG_BC3_UNORM;
        case PFG_BC4_UNORM:
        case PFG_BC4_SNORM:
            return PFG_BC4_UNORM;
        case PFG_BC5_UNORM:
        case PFG_BC5_SNORM:
            return PFG_BC5_UNORM;

        case PFG_BGRA8_UNORM:
        case PFG_BGRA8_UNORM_SRGB:
            return PFG_BGRA8_UNORM;

        case PFG_BGRX8_UNORM:
        case PFG_BGRX8_UNORM_SRGB:
            return PFG_BGRX8_UNORM;

        case PFG_BC6H_UF16:
        case PFG_BC6H_SF16:
            return PFG_BC6H_UF16;

        case PFG_BC7_UNORM:
        case PFG_BC7_UNORM_SRGB:
            return PFG_BC7_UNORM;

        default:
            return format;
        }

        return format;
    }

    static const uint32 PFF_COMPRESSED_COMMON = PixelFormatGpuUtils::PFF_COMPRESSED|
                                                PixelFormatGpuUtils::PFF_INTEGER|
                                                PixelFormatGpuUtils::PFF_NORMALIZED;

    PixelFormatGpuUtils::PixelFormatDesc PixelFormatGpuUtils::msPixelFormatDesc[PFG_COUNT + 1u] =
    {
        {"PFG_UNKNOWN", 1u, 0, 0 },
        {"PFG_RGBA32_FLOAT",		4u, 4u * sizeof(uint32),    PFF_FLOAT },
        {"PFG_RGBA32_UINT",			4u, 4u * sizeof(uint32),	PFF_INTEGER },
        {"PFG_RGBA32_INT",			4u, 4u * sizeof(uint32),	PFF_INTEGER|PFF_SIGNED },

        {"PFG_RGB32_FLOAT",			3u, 3u * sizeof(uint32),	PFF_FLOAT },
        {"PFG_RGB32_UINT",			3u, 3u * sizeof(uint32),	PFF_INTEGER },
        {"PFG_RGB32_INT",			3u, 3u * sizeof(uint32),	PFF_INTEGER|PFF_SIGNED },

        {"PFG_RGBA16_FLOAT",		4u, 4u * sizeof(uint16),	PFF_HALF },
        {"PFG_RGBA16_UNORM",		4u, 4u * sizeof(uint16),	PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_RGBA16_UINT",			4u, 4u * sizeof(uint16),	PFF_INTEGER },
        {"PFG_RGBA16_SNORM",		4u, 4u * sizeof(uint16),	PFF_INTEGER|PFF_SIGNED|PFF_NORMALIZED },
        {"PFG_RGBA16_SINT",			4u, 4u * sizeof(uint16),	PFF_INTEGER|PFF_SIGNED },

        {"PFG_RG32_FLOAT",			2u, 2u * sizeof(uint32),	PFF_FLOAT },
        {"PFG_RG32_UINT",			2u, 2u * sizeof(uint32),	PFF_INTEGER },
        {"PFG_RG32_SINT",			2u, 2u * sizeof(uint32),	PFF_INTEGER|PFF_SIGNED },

        {"PFG_D32_FLOAT_S8X24_UINT",2u, 2u * sizeof(uint32),	PFF_FLOAT|PFF_DEPTH|PFF_STENCIL },

        {"PFG_R10G10B10A2_UNORM",	4u, 1u * sizeof(uint32),	PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_R10G10B10A2_UINT",	4u, 1u * sizeof(uint32),	PFF_INTEGER },
        {"PFG_R11G11B10_FLOAT",		3u, 1u * sizeof(uint32),	PFF_FLOAT_RARE },

        {"PFG_RGBA8_UNORM",			4u, 4u * sizeof(uint8),		PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_RGBA8_UNORM_SRGB",	4u, 4u * sizeof(uint8),		PFF_INTEGER|PFF_NORMALIZED|PFF_SRGB },
        {"PFG_RGBA8_UINT",			4u, 4u * sizeof(uint8),		PFF_INTEGER },
        {"PFG_RGBA8_SNORM",			4u, 4u * sizeof(uint8),		PFF_INTEGER|PFF_SIGNED|PFF_NORMALIZED },
        {"PFG_RGBA8_SINT",			4u, 4u * sizeof(uint8),		PFF_INTEGER|PFF_SIGNED },

        {"PFG_RG16_FLOAT",			2u, 2u * sizeof(uint16),	PFF_HALF },
        {"PFG_RG16_UNORM",			2u, 2u * sizeof(uint16),	PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_RG16_UINT",			2u, 2u * sizeof(uint16),	PFF_INTEGER },
        {"PFG_RG16_SNORM",			2u, 2u * sizeof(uint16),	PFF_INTEGER|PFF_SIGNED|PFF_NORMALIZED },
        {"PFG_RG16_SINT",			2u, 2u * sizeof(uint16),	PFF_INTEGER|PFF_SIGNED },

        {"PFG_D32_FLOAT",			1u, 1u * sizeof(uint32),	PFF_FLOAT|PFF_DEPTH },
        {"PFG_R32_FLOAT",			1u, 1u * sizeof(uint32),	PFF_FLOAT },
        {"PFG_R32_UINT",			1u, 1u * sizeof(uint32),	PFF_INTEGER },
        {"PFG_R32_SINT",			1u, 1u * sizeof(uint32),	PFF_INTEGER|PFF_SIGNED },

        {"PFG_D24_UNORM",			1u, 1u * sizeof(uint32),	PFF_INTEGER|PFF_NORMALIZED|PFF_DEPTH },
        {"PFG_D24_UNORM_S8_UINT",	1u, 1u * sizeof(uint32),	PFF_INTEGER|PFF_NORMALIZED|
                                                                PFF_DEPTH|PFF_STENCIL },

        {"PFG_RG8_UNORM",			2u, 2u * sizeof(uint8),		PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_RG8_UINT",			2u, 2u * sizeof(uint8),		PFF_INTEGER },
        {"PFG_RG8_SNORM",			2u, 2u * sizeof(uint8),		PFF_INTEGER|PFF_SIGNED|PFF_NORMALIZED },
        {"PFG_RG8_SINT",			2u, 2u * sizeof(uint8),		PFF_INTEGER|PFF_SIGNED },

        {"PFG_R16_FLOAT",			1u, 1u * sizeof(uint16),	PFF_HALF },
        {"PFG_D16_UNORM",			1u, 1u * sizeof(uint16),	PFF_INTEGER|PFF_NORMALIZED|PFF_DEPTH },
        {"PFG_R16_UNORM",			1u, 1u * sizeof(uint16),	PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_R16_UINT",			1u, 1u * sizeof(uint16),	PFF_INTEGER },
        {"PFG_R16_SNORM",			1u, 1u * sizeof(uint16),	PFF_INTEGER|PFF_SIGNED|PFF_NORMALIZED },
        {"PFG_R16_SINT",			1u, 1u * sizeof(uint16),	PFF_INTEGER|PFF_SIGNED },

        {"PFG_R8_UNORM",			1u, 1u * sizeof(uint8),		PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_R8_UINT",				1u, 1u * sizeof(uint8),		PFF_INTEGER },
        {"PFG_R8_SNORM",			1u, 1u * sizeof(uint8),		PFF_INTEGER|PFF_SIGNED|PFF_NORMALIZED },
        {"PFG_R8_SINT",				1u, 1u * sizeof(uint8),		PFF_INTEGER|PFF_SIGNED },
        {"PFG_A8_UNORM",			1u, 1u * sizeof(uint8),		PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_R1_UNORM",			1u, 0,						0 }, // ???

        {"PFG_R9G9B9E5_SHAREDEXP",	1u, 1u * sizeof(uint32),	PFF_FLOAT_RARE },

        {"PFG_R8G8_B8G8_UNORM",		4u, 4u * sizeof(uint8),		PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_G8R8_G8B8_UNORM",		4u, 4u * sizeof(uint8),		PFF_INTEGER|PFF_SIGNED|PFF_NORMALIZED },

        {"PFG_BC1_UNORM",			4u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_BC1_UNORM_SRGB",		4u, 0,						PFF_COMPRESSED_COMMON|PFF_SRGB },

        {"PFG_BC2_UNORM",			4u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_BC2_UNORM_SRGB",		4u, 0,						PFF_COMPRESSED_COMMON|PFF_SRGB },

        {"PFG_BC3_UNORM",			4u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_BC3_UNORM_SRGB",		4u, 0,						PFF_COMPRESSED_COMMON|PFF_SRGB },

        {"PFG_BC4_UNORM",			1u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_BC4_SNORM",			1u, 0,						PFF_COMPRESSED_COMMON|PFF_SIGNED },

        {"PFG_BC5_UNORM",			2u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_BC5_SNORM",			2u, 0,						PFF_COMPRESSED_COMMON|PFF_SIGNED },

        {"PFG_B5G6R5_UNORM",		3u, 1u * sizeof(uint16),	PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_B5G5R5A1_UNORM",		3u, 1u * sizeof(uint16),	PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_BGRA8_UNORM",			4u, 4u * sizeof(uint8),		PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_BGRX8_UNORM",			4u, 4u * sizeof(uint8),		PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_R10G10B10_XR_BIAS_A2_UNORM",4u, 1u * sizeof(uint32),PFF_FLOAT_RARE },

        {"PFG_BGRA8_UNORM_SRGB",	4u, 4u * sizeof(uint8),		PFF_INTEGER|PFF_NORMALIZED|PFF_SRGB },
        {"PFG_BGRX8_UNORM_SRGB",	3u, 4u * sizeof(uint8),		PFF_INTEGER|PFF_NORMALIZED|PFF_SRGB },

        {"PFG_BC6H_UF16",			3u, 0,						PFF_COMPRESSED|PFF_FLOAT_RARE },
        {"PFG_BC6H_SF16",			3u, 0,						PFF_COMPRESSED|PFF_FLOAT_RARE|PFF_SIGNED },

        {"PFG_BC7_UNORM",			4u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_BC7_UNORM_SRGB",		4u, 0,						PFF_COMPRESSED_COMMON|PFF_SRGB },

        {"PFG_AYUV",				3u, 0,						0 },
        {"PFG_Y410",				3u, 0,						0 },
        {"PFG_Y416",				3u, 0,						0 },
        {"PFG_NV12",				3u, 0,						0 },
        {"PFG_P010",				3u, 0,						0 },
        {"PFG_P016",				3u, 0,						0 },
        {"PFG_420_OPAQUE",			3u, 0,						0 },
        {"PFG_YUY2",				3u, 0,						0 },
        {"PFG_Y210",				3u, 0,						0 },
        {"PFG_Y216",				3u, 0,						0 },
        {"PFG_NV11",				3u, 0,						0 },
        {"PFG_AI44",				3u, 0,						0 },
        {"PFG_IA44",				3u, 0,						0 },
        {"PFG_P8",					1u, 1u * sizeof(uint8),		PFF_PALLETE },
        {"PFG_A8P8",				1u, 2u * sizeof(uint8),		PFF_PALLETE },
        {"PFG_B4G4R4A4_UNORM",		4u, 1u * sizeof(uint16),	PFF_INTEGER|PFF_NORMALIZED },
        {"PFG_P208",				3u, 0,						0 },
        {"PFG_V208",				3u, 0,						0 },
        {"PFG_V408",				3u, 0,						0 },

        {"PFG_PVRTC_RGB2",			3u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_PVRTC_RGBA2",			4u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_PVRTC_RGB4",			3u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_PVRTC_RGBA4",			4u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_PVRTC2_2BPP",			3u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_PVRTC2_4BPP",			3u, 0,						PFF_COMPRESSED_COMMON },

        {"PFG_ETC1_RGB8_UNORM",		3u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_ETC2_RGB8_UNORM",		3u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_ETC2_RGB8_UNORM_SRGB",3u, 0,						PFF_COMPRESSED_COMMON|PFF_SRGB },
        {"PFG_ETC2_RGBA8_UNORM",	4u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_ETC2_RGBA8_UNORM_SRGB",4u,0,						PFF_COMPRESSED_COMMON|PFF_SRGB },
        {"PFG_ETC2_RGB8A1_UNORM",	4u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_ETC2_RGB8A1_UNORM_SRGB",4u, 0,					PFF_COMPRESSED_COMMON|PFF_SRGB },
        {"PFG_EAC_R11_UNORM",		1u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_EAC_R11_SNORM",		1u, 0,						PFF_COMPRESSED_COMMON|PFF_SIGNED },
        {"PFG_EAC_R11G11_UNORM",	2u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_EAC_R11G11_SNORM",	2u, 0,						PFF_COMPRESSED_COMMON|PFF_SIGNED },

        {"PFG_ATC_RGB",				3u, 0,						PFF_COMPRESSED_COMMON },
        {"PFG_ATC_RGBA_EXPLICIT_ALPHA",			4u, 0,			PFF_COMPRESSED_COMMON },
        {"PFG_ATC_RGBA_INTERPOLATED_ALPHA",		4u, 0,			PFF_COMPRESSED_COMMON },

        {"PFG_COUNT", 1u, 0, 0 },
    };
}
