// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ImpliedTyping.h"
#include "Conversion.h"
// #include "../Math/Vector.h"
// #include "../Math/Matrix.h"
#include <regex>

namespace Utility
{
    namespace ImpliedTyping
    {
        uint32_t TypeDesc::GetSize() const
        {
            switch (_type) {
            case TypeCat::Bool: return sizeof(bool)*unsigned(_arrayCount);

            case TypeCat::Int8:
            case TypeCat::UInt8: return sizeof(uint8_t)*unsigned(_arrayCount);

            case TypeCat::Int16:
            case TypeCat::UInt16: return sizeof(uint16_t)*unsigned(_arrayCount);

            case TypeCat::Int32:
            case TypeCat::UInt32:
            case TypeCat::Float: return sizeof(uint32_t)*unsigned(_arrayCount);

            case TypeCat::Int64:
            case TypeCat::UInt64:
            case TypeCat::Double: return sizeof(uint64_t)*unsigned(_arrayCount);

            case TypeCat::Void:
            default: return 0;
            }
        }

        bool operator==(const TypeDesc& lhs, const TypeDesc& rhs)
        {
                // (note -- ignoring type hint for this comparison (because the hint isn't actually related to the structure of the data)
            return lhs._type == rhs._type
                && lhs._arrayCount == rhs._arrayCount;
        }

        TypeDesc TypeOf(const char expression[]) 
        {
                // not implemented
            assert(0);
            return TypeDesc();
        }

        bool Cast(
            IteratorRange<void*> dest, TypeDesc destType,
            IteratorRange<const void*> rawSrc, TypeDesc srcType)
        {
            assert(rawSrc.size() >= srcType.GetSize());
            assert(dest.size() >= destType.GetSize());
            IteratorRange<const void*> src = rawSrc;
            if (destType._arrayCount <= 1) {
#if defined(__arm__) && !defined(__aarch64__)
                // Only 32-bit ARM, we may get unaligned access reading directly from rawSrc
                // Therefore, we check if it is 4-byte unaligned and copy if so

                // Setup the stack buffer if we need it
                // Right now maximum size of a type is 8 bytes, adjust if necessary
                const size_t MAXIMUM_SRC_SIZE = 8;
                const size_t srcSize = srcType.GetSize();
                assert(srcSize > 0);
                assert(srcSize <= MAXIMUM_SRC_SIZE);
                uint8_t srcBuffer[MAXIMUM_SRC_SIZE];

                // Check if unaligned
                if (uintptr_t(rawSrc.begin()) & 3u) {
                    // If unaligned, copy to the srcBuffer (memcpy is safe to do unaligned access)
                    memcpy(&srcBuffer[0], src.begin(), srcSize);
                    // Set src to the srcBuffer
                    src = { &srcBuffer[0], &srcBuffer[srcSize] };
                }
#endif
                    // casting single element. Will we read the first element
                    // of the 
                switch (destType._type) {
                case TypeCat::Bool:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(bool*)dest.begin() = *(bool*)src.begin(); return true;
                        case TypeCat::Int8: *(bool*)dest.begin() = !!*(int8_t*)src.begin(); return true;
                        case TypeCat::UInt8: *(bool*)dest.begin() = !!*(uint8_t*)src.begin(); return true;
                        case TypeCat::Int16: *(bool*)dest.begin() = !!*(int16_t*)src.begin(); return true;
                        case TypeCat::UInt16: *(bool*)dest.begin() = !!*(uint16_t*)src.begin(); return true;
                        case TypeCat::Int32: *(bool*)dest.begin() = !!*(int32_t*)src.begin(); return true;
                        case TypeCat::UInt32: *(bool*)dest.begin() = !!*(uint32_t*)src.begin(); return true;
                        case TypeCat::Int64: *(bool*)dest.begin() = !!*(int64_t*)src.begin(); return true;
                        case TypeCat::UInt64: *(bool*)dest.begin() = !!*(uint64_t*)src.begin(); return true;
                        case TypeCat::Float: *(bool*)dest.begin() = !!*(float*)src.begin(); return true;
                        case TypeCat::Double: *(bool*)dest.begin() = !!*(double*)src.begin(); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::Int8:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int8_t*)dest.begin() = int8_t(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(int8_t*)dest.begin() = int8_t(*(int8_t*)src.begin()); return true;
                        case TypeCat::UInt8: *(int8_t*)dest.begin() = int8_t(*(uint8_t*)src.begin()); return true;
                        case TypeCat::Int16: *(int8_t*)dest.begin() = int8_t(*(int16_t*)src.begin()); return true;
                        case TypeCat::UInt16: *(int8_t*)dest.begin() = int8_t(*(uint16_t*)src.begin()); return true;
                        case TypeCat::Int32: *(int8_t*)dest.begin() = int8_t(*(int32_t*)src.begin()); return true;
                        case TypeCat::UInt32: *(int8_t*)dest.begin() = int8_t(*(uint32_t*)src.begin()); return true;
                        case TypeCat::Int64: *(int8_t*)dest.begin() = int8_t(*(int64_t*)src.begin()); return true;
                        case TypeCat::UInt64: *(int8_t*)dest.begin() = int8_t(*(uint64_t*)src.begin()); return true;
                        case TypeCat::Float: *(int8_t*)dest.begin() = int8_t(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(int8_t*)dest.begin() = int8_t(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::UInt8:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint8_t*)dest.begin() = uint8_t(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(uint8_t*)dest.begin() = uint8_t(*(int8_t*)src.begin()); return true;
                        case TypeCat::UInt8: *(uint8_t*)dest.begin() = uint8_t(*(uint8_t*)src.begin()); return true;
                        case TypeCat::Int16: *(uint8_t*)dest.begin() = uint8_t(*(int16_t*)src.begin()); return true;
                        case TypeCat::UInt16: *(uint8_t*)dest.begin() = uint8_t(*(uint16_t*)src.begin()); return true;
                        case TypeCat::Int32: *(uint8_t*)dest.begin() = uint8_t(*(int32_t*)src.begin()); return true;
                        case TypeCat::UInt32: *(uint8_t*)dest.begin() = uint8_t(*(uint32_t*)src.begin()); return true;
                        case TypeCat::Int64: *(uint8_t*)dest.begin() = uint8_t(*(int64_t*)src.begin()); return true;
                        case TypeCat::UInt64: *(uint8_t*)dest.begin() = uint8_t(*(uint64_t*)src.begin()); return true;
                        case TypeCat::Float: *(uint8_t*)dest.begin() = uint8_t(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(uint8_t*)dest.begin() = uint8_t(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;
                
                case TypeCat::Int16:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int16_t*)dest.begin() = int16_t(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(int16_t*)dest.begin() = int16_t(*(int8_t*)src.begin()); return true;
                        case TypeCat::UInt8: *(int16_t*)dest.begin() = int16_t(*(uint8_t*)src.begin()); return true;
                        case TypeCat::Int16: *(int16_t*)dest.begin() = int16_t(*(int16_t*)src.begin()); return true;
                        case TypeCat::UInt16: *(int16_t*)dest.begin() = int16_t(*(uint16_t*)src.begin()); return true;
                        case TypeCat::Int32: *(int16_t*)dest.begin() = int16_t(*(int32_t*)src.begin()); return true;
                        case TypeCat::UInt32: *(int16_t*)dest.begin() = int16_t(*(uint32_t*)src.begin()); return true;
                        case TypeCat::Int64: *(int16_t*)dest.begin() = int16_t(*(int64_t*)src.begin()); return true;
                        case TypeCat::UInt64: *(int16_t*)dest.begin() = int16_t(*(uint64_t*)src.begin()); return true;
                        case TypeCat::Float: *(int16_t*)dest.begin() = int16_t(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(int16_t*)dest.begin() = int16_t(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::UInt16:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint16_t*)dest.begin() = uint16_t(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(uint16_t*)dest.begin() = uint16_t(*(int8_t*)src.begin()); return true;
                        case TypeCat::UInt8: *(uint16_t*)dest.begin() = uint16_t(*(uint8_t*)src.begin()); return true;
                        case TypeCat::Int16: *(uint16_t*)dest.begin() = uint16_t(*(int16_t*)src.begin()); return true;
                        case TypeCat::UInt16: *(uint16_t*)dest.begin() = uint16_t(*(uint16_t*)src.begin()); return true;
                        case TypeCat::Int32: *(uint16_t*)dest.begin() = uint16_t(*(int32_t*)src.begin()); return true;
                        case TypeCat::UInt32: *(uint16_t*)dest.begin() = uint16_t(*(uint32_t*)src.begin()); return true;
                        case TypeCat::Int64: *(uint16_t*)dest.begin() = uint16_t(*(int64_t*)src.begin()); return true;
                        case TypeCat::UInt64: *(uint16_t*)dest.begin() = uint16_t(*(uint64_t*)src.begin()); return true;
                        case TypeCat::Float: *(uint16_t*)dest.begin() = uint16_t(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(uint16_t*)dest.begin() = uint16_t(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;
                
                case TypeCat::Int32:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int32_t*)dest.begin() = int32_t(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(int32_t*)dest.begin() = int32_t(*(int8_t*)src.begin()); return true;
                        case TypeCat::UInt8: *(int32_t*)dest.begin() = int32_t(*(uint8_t*)src.begin()); return true;
                        case TypeCat::Int16: *(int32_t*)dest.begin() = int32_t(*(int16_t*)src.begin()); return true;
                        case TypeCat::UInt16: *(int32_t*)dest.begin() = int32_t(*(uint16_t*)src.begin()); return true;
                        case TypeCat::Int32: *(int32_t*)dest.begin() = *(int32_t*)src.begin(); return true;
                        case TypeCat::UInt32: *(int32_t*)dest.begin() = int32_t(*(uint32_t*)src.begin()); return true;
                        case TypeCat::Int64: *(int32_t*)dest.begin() = int32_t(*(int64_t*)src.begin()); return true;
                        case TypeCat::UInt64: *(int32_t*)dest.begin() = int32_t(*(uint64_t*)src.begin()); return true;
                        case TypeCat::Float: *(int32_t*)dest.begin() = int32_t(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(int32_t*)dest.begin() = int32_t(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::UInt32:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint32_t*)dest.begin() = uint32_t(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(uint32_t*)dest.begin() = uint32_t(*(int8_t*)src.begin()); return true;
                        case TypeCat::UInt8: *(uint32_t*)dest.begin() = uint32_t(*(uint8_t*)src.begin()); return true;
                        case TypeCat::Int16: *(uint32_t*)dest.begin() = uint32_t(*(int16_t*)src.begin()); return true;
                        case TypeCat::UInt16: *(uint32_t*)dest.begin() = uint32_t(*(uint16_t*)src.begin()); return true;
                        case TypeCat::Int32: *(uint32_t*)dest.begin() = uint32_t(*(int32_t*)src.begin()); return true;
                        case TypeCat::UInt32: *(uint32_t*)dest.begin() = *(uint32_t*)src.begin(); return true;
                        case TypeCat::Int64: *(uint32_t*)dest.begin() = uint32_t(*(int64_t*)src.begin()); return true;
                        case TypeCat::UInt64: *(uint32_t*)dest.begin() = uint32_t(*(uint64_t*)src.begin()); return true;
                        case TypeCat::Float: *(uint32_t*)dest.begin() = uint32_t(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(uint32_t*)dest.begin() = uint32_t(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::Int64:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int64_t*)dest.begin() = int64_t(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(int64_t*)dest.begin() = int64_t(*(int8_t*)src.begin()); return true;
                        case TypeCat::UInt8: *(int64_t*)dest.begin() = int64_t(*(uint8_t*)src.begin()); return true;
                        case TypeCat::Int16: *(int64_t*)dest.begin() = int64_t(*(int16_t*)src.begin()); return true;
                        case TypeCat::UInt16: *(int64_t*)dest.begin() = int64_t(*(uint16_t*)src.begin()); return true;
                        case TypeCat::Int32: *(int64_t*)dest.begin() = int64_t(*(int32_t*)src.begin()); return true;
                        case TypeCat::UInt32: *(int64_t*)dest.begin() = int64_t(*(uint32_t*)src.begin()); return true;
                        case TypeCat::Int64: *(int64_t*)dest.begin() = int64_t(*(int64_t*)src.begin()); return true;
                        case TypeCat::UInt64: *(int64_t*)dest.begin() = int64_t(*(uint64_t*)src.begin()); return true;
                        case TypeCat::Float: *(int64_t*)dest.begin() = int64_t(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(int64_t*)dest.begin() = int64_t(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::UInt64:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint64_t*)dest.begin() = uint64_t(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(uint64_t*)dest.begin() = uint64_t(*(int8_t*)src.begin()); return true;
                        case TypeCat::UInt8: *(uint64_t*)dest.begin() = uint64_t(*(uint8_t*)src.begin()); return true;
                        case TypeCat::Int16: *(uint64_t*)dest.begin() = uint64_t(*(int16_t*)src.begin()); return true;
                        case TypeCat::UInt16: *(uint64_t*)dest.begin() = uint64_t(*(uint16_t*)src.begin()); return true;
                        case TypeCat::Int32: *(uint64_t*)dest.begin() = uint64_t(*(int32_t*)src.begin()); return true;
                        case TypeCat::UInt32: *(uint64_t*)dest.begin() = uint64_t(*(uint32_t*)src.begin()); return true;
                        case TypeCat::Int64: *(uint64_t*)dest.begin() = uint64_t(*(int64_t*)src.begin()); return true;
                        case TypeCat::UInt64: *(uint64_t*)dest.begin() = uint64_t(*(uint64_t*)src.begin()); return true;
                        case TypeCat::Float: *(uint64_t*)dest.begin() = uint64_t(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(uint64_t*)dest.begin() = uint64_t(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::Float:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(float*)dest.begin() = float(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(float*)dest.begin() = float(*(int8_t*)src.begin()); return true;
                        case TypeCat::UInt8: *(float*)dest.begin() = float(*(uint8_t*)src.begin()); return true;
                        case TypeCat::Int16: *(float*)dest.begin() = float(*(int16_t*)src.begin()); return true;
                        case TypeCat::UInt16: *(float*)dest.begin() = float(*(uint16_t*)src.begin()); return true;
                        case TypeCat::Int32: *(float*)dest.begin() = float(*(int32_t*)src.begin()); return true;
                        case TypeCat::UInt32: *(float*)dest.begin() = float(*(uint32_t*)src.begin()); return true;
                        case TypeCat::Int64: *(float*)dest.begin() = float(*(int64_t*)src.begin()); return true;
                        case TypeCat::UInt64: *(float*)dest.begin() = float(*(uint64_t*)src.begin()); return true;
                        case TypeCat::Float: *(float*)dest.begin() = float(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(float*)dest.begin() = float(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::Double:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(double*)dest.begin() = double(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(double*)dest.begin() = double(*(int8_t*)src.begin()); return true;
                        case TypeCat::UInt8: *(double*)dest.begin() = double(*(uint8_t*)src.begin()); return true;
                        case TypeCat::Int16: *(double*)dest.begin() = double(*(int16_t*)src.begin()); return true;
                        case TypeCat::UInt16: *(double*)dest.begin() = double(*(uint16_t*)src.begin()); return true;
                        case TypeCat::Int32: *(double*)dest.begin() = double(*(int32_t*)src.begin()); return true;
                        case TypeCat::UInt32: *(double*)dest.begin() = double(*(uint32_t*)src.begin()); return true;
                        case TypeCat::Int64: *(double*)dest.begin() = double(*(int64_t*)src.begin()); return true;
                        case TypeCat::UInt64: *(double*)dest.begin() = double(*(uint64_t*)src.begin()); return true;
                        case TypeCat::Float: *(double*)dest.begin() = double(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(double*)dest.begin() = double(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;
                        
                case TypeCat::Void: break;
                }
            } else {

                    // multiple array elements. We might need to remap elements
                    // First -- trival cases can be completed with a memcpy
                if (    srcType._arrayCount == destType._arrayCount
                    &&  srcType._type == destType._type) {
                    std::memcpy(dest.begin(), src.begin(), std::min(dest.size(), size_t(srcType.GetSize())));
                    return true;
                }
                    
                auto destIterator = dest;
                auto srcIterator = src;
                for (unsigned c=0; c<destType._arrayCount; ++c) {
                    if (destIterator.size() < TypeDesc{destType._type}.GetSize()) {
                        return false;
                    }
                    if (c < srcType._arrayCount) {
                        if (!Cast(destIterator, TypeDesc{destType._type},
                            srcIterator, TypeDesc{srcType._type})) {
                            return false;
                        }

                        destIterator.first = PtrAdd(destIterator.first, TypeDesc{destType._type}.GetSize());
                        srcIterator.first = PtrAdd(srcIterator.first, TypeDesc{srcType._type}.GetSize());
                    } else {
                            // using HLSL rules for filling in blanks:
                            //  element 3 is 1, but others are 0
                        unsigned value = (c==3)?1:0;
                        if (!Cast(destIterator, TypeDesc{destType._type},
                            AsOpaqueIteratorRange(value), TypeDesc{TypeCat::UInt32})) {
                            return false;
                        }
                        destIterator.first = PtrAdd(destIterator.first, TypeDesc{destType._type}.GetSize());
                    }
                }
                return true;
            }

            return false;
        }
        
        
        CastType CalculateCastType(TypeCat testType, TypeCat againstType) 
        {
            if (testType == againstType)
                return CastType::Equal;
            
            bool isWidening = false;
            switch (againstType) {
                case TypeCat::Bool:
                case TypeCat::UInt8:
                case TypeCat::UInt16:
                case TypeCat::UInt32:
                case TypeCat::UInt64:
                    isWidening = (testType == TypeCat::Bool ||
                            testType == TypeCat::UInt8 ||
                            testType == TypeCat::UInt16 ||
                            testType == TypeCat::UInt32 ||
                            testType == TypeCat::UInt64) && (testType < againstType);
                    break;
                case TypeCat::Float:
                case TypeCat::Double:
                    isWidening = testType < againstType;
                    break;
                case TypeCat::Int8:
                    isWidening = testType <= TypeCat::UInt8;
                    break;
                case TypeCat::Int16:
                    isWidening = testType <= TypeCat::UInt16;
                    break;
                case TypeCat::Int32:
                    isWidening = testType <= TypeCat::UInt32;
                    break;
                case TypeCat::Int64:
                    isWidening = testType <= TypeCat::UInt64;
                    break;
                default:
                    assert(false); // Unknown type
                    isWidening = false;
                    break;
            }
            
            return isWidening ? CastType::Widening : CastType::Narrowing;
        }
        
        template<typename CharType>
            class ParsingRegex
        {
        public:
            std::basic_regex<CharType> s_booleanTrue;
            std::basic_regex<CharType> s_booleanFalse;
            std::basic_regex<CharType> s_unsignedPattern;
            std::basic_regex<CharType> s_signedPattern;
            std::basic_regex<CharType> s_floatPattern;
            std::basic_regex<CharType> s_arrayPattern;
            std::basic_regex<CharType> s_arrayElementPattern;
        
            ParsingRegex()
            : s_booleanTrue((const CharType*)R"(^(true)|(True)|(y)|(Y)|(yes)|(Yes)|(TRUE)|(Y)|(YES)$)")
            , s_booleanFalse((const CharType*)R"(^(false)|(False)|(n)|(N)|(no)|(No)|(FALSE)|(N)|(NO)$)")
            , s_unsignedPattern((const CharType*)R"(^\+?(([\d]+)|(0x[\da-fA-F]+))(u|U|(u8)|(u16)|(u32)|(u64)|(U8)|(U16)|(U32)|(U64))?$)")
            , s_signedPattern((const CharType*)R"(^[-\+]?(([\d]+)|(0x[\da-fA-F]+))(i|I|(i8)|(i16)|(i32)|(i64)|(I8)|(I16)|(I32)|(I64))?$)")
            , s_floatPattern((const CharType*)R"(^[-\+]?(([\d]*\.?[\d]+)|([\d]+\.))([eE][-\+]?[\d]+)?(f|F|(f32)|(F32)|(f64)|(F64))?$)")
            , s_arrayPattern((const CharType*)R"(\{\s*([^,\s]+(?:\s*,\s*[^,\s]+)*)\s*\}([vcVC]?))")
            , s_arrayElementPattern((const CharType*)R"(\s*([^,\s]+)\s*(?:,|$))")
            {}
            ~ParsingRegex() {}
        };
        
        static std::unique_ptr<ParsingRegex<char>> s_parsingChar;
        
        template<typename CharType>
            TypeDesc Parse(
                StringSection<CharType> expression,
                void* dest, size_t destSize)
        {
            if (!s_parsingChar) {
                s_parsingChar = std::make_unique<ParsingRegex<char>>();
            }
                // parse string expression into _tnative types.
                // We'll write the native object into _tthe buffer and return a type desc
            if (std::regex_match(expression.begin(), expression.end(), s_parsingChar->s_booleanTrue)) {
                assert(destSize >= sizeof(bool));
                *(bool*)dest = true;
                return TypeDesc{TypeCat::Bool};
            } else if (std::regex_match(expression.begin(), expression.end(), s_parsingChar->s_booleanFalse)) {
                assert(destSize >= sizeof(bool));
                *(bool*)dest = false;
                return TypeDesc{TypeCat::Bool};
            }

            std::match_results<const CharType*> cm; 
            if (std::regex_match(expression.begin(), expression.end(), s_parsingChar->s_unsignedPattern)) {
                unsigned precision = 32;
                if (cm.size() >= 4 && cm[4].length() > 1)
                    precision = XlAtoUI32(&cm[2].str()[1]);

                uint64_t value;
                auto len = expression.size();
                if (len > 2 && (expression.begin()[0] == '0' && expression.begin()[1] == 'x')) {
                        // hex form
                    value = XlAtoUI64(&expression.begin()[2], nullptr, 16);
                } else {
                    value = XlAtoUI64(expression.begin());
                }

                if (precision == 8) {
                    assert(destSize >= sizeof(uint8_t));
                    *(uint8_t*)dest = (uint8_t)value;
                    return TypeDesc{TypeCat::UInt8};
                } else if (precision == 16) {
                    assert(destSize >= sizeof(uint16_t));
                    *(uint16_t*)dest = (uint16_t)value;
                    return TypeDesc{TypeCat::UInt16};
                } else if (precision == 32) {
                    assert(destSize >= sizeof(uint32_t));
                    *(uint32_t*)dest = (uint32_t)value;
                    return TypeDesc{TypeCat::UInt32};
                } else if (precision == 64) {
                    assert(destSize >= sizeof(uint64_t));
                    *(uint64_t*)dest = (uint64_t)value;
                    return TypeDesc{TypeCat::UInt64};
                }

                assert(0);
            }

            if (std::regex_match(expression.begin(), expression.end(), cm, s_parsingChar->s_signedPattern)) {
                unsigned precision = 32;
                if (cm.size() >= 4 && cm[4].length() > 1)
                    precision = XlAtoUI32(&cm[2].str()[1]);

                int64_t value;
                auto len = expression.end() - expression.begin();
                if (len > 2 && (expression.begin()[0] == '0' && expression.begin()[1] == 'x')) {
                        // hex form
                    value = XlAtoI64(&expression.begin()[2], nullptr, 16);
                } else {
                    value = XlAtoI64(expression.begin());
                }

                if (precision == 8) {
                    assert(destSize >= sizeof(int8_t));
                    *(int8_t*)dest = (int8_t)value;
                    return TypeDesc{TypeCat::Int8};
                } else if (precision == 16) {
                    assert(destSize >= sizeof(int16_t));
                    *(int16_t*)dest = (int16_t)value;
                    return TypeDesc{TypeCat::Int16};
                } else if (precision == 32) {
                    assert(destSize >= sizeof(int32_t));
                    *(int32_t*)dest = (int32_t)value;
                    return TypeDesc{TypeCat::Int32};
                } else if (precision == 64) {
                    assert(destSize >= sizeof(int64_t));
                    *(int64_t*)dest = (int64_t)value;
                    return TypeDesc{TypeCat::Int64};
                }

                assert(0);
            }

            if (std::regex_match(expression.begin(), expression.end(), s_parsingChar->s_floatPattern)) {
                bool doublePrecision = false;
                if (cm.size() >= 4 && cm[4].length() > 1) {
                    auto precision = XlAtoUI32(&cm[2].str()[1]);
                    doublePrecision = precision == 64;
                }

                if (doublePrecision) {
                    assert(destSize >= sizeof(double));
                    *(double*)dest = Conversion::Convert<double>(expression);
                    return TypeDesc{TypeCat::Double};
                } else {
                    assert(destSize >= sizeof(float));
                    *(float*)dest = Conversion::Convert<float>(expression);
                    return TypeDesc{TypeCat::Float};
                }
            }

            {
                    // match for float array:
                // R"(^\{\s*[-\+]?(([\d]*\.?[\d]+)|([\d]+\.))([eE][-\+]?[\d]+)?[fF]\s*(\s*,\s*([-\+]?(([\d]*\.?[\d]+)|([\d]+\.))([eE][-\+]?[\d]+)?[fF]))*\s*\}[vc]?$)"

                // std::match_results<typename std::basic_string<CharType>::const_iterator> cm;
                if (std::regex_match(expression.begin(), expression.end(), cm, s_parsingChar->s_arrayPattern)) {
                    const auto& subMatch = cm[1];
                    std::regex_iterator<const CharType*> rit(
                        subMatch.first, subMatch.second,
                        s_parsingChar->s_arrayElementPattern);
                    std::regex_iterator<const CharType*> rend;
                    
                    auto startIt = rit;

                    auto dstIterator = dest;
                    auto dstIteratorSize = ptrdiff_t(destSize);

                    TypeCat cat = TypeCat::Void;
                    unsigned count = 0;
                    if (rit != rend) {
                        assert(rit->size() >= 1);
                        const auto& eleMatch = (*rit)[1];

                        auto subType = Parse(
                            MakeStringSection(eleMatch.first, eleMatch.second),
                            dstIterator, dstIteratorSize);

                        assert(subType._arrayCount <= 1);
                        assert(subType._type != TypeCat::Void);
                        cat = subType._type;

                        auto size = subType.GetSize();
                        dstIterator = PtrAdd(dstIterator, size);
                        dstIteratorSize -= size;
                        ++rit;
                        ++count;
                    }

                    assert(cat != TypeCat::Void);
                    for (;rit != rend; ++rit) {
                        assert(rit->size() >= 1);
                        const auto& eleMatch = (*rit)[1];

                        auto subType = Parse(
                            MakeStringSection(eleMatch.first, eleMatch.second),
                            dstIterator, dstIteratorSize);

                        if (CalculateCastType(subType._type, cat) != CastType::Narrowing) {
                            bool castSuccess = Cast(   
                                { dstIterator, PtrAdd(dstIterator, TypeDesc{cat}.GetSize()) }, TypeDesc{cat},
                                { dstIterator, PtrAdd(dstIterator, subType.GetSize()) }, subType);
                            (void)castSuccess;
                            subType._type = cat;
                        } else {
                            // If the cast would narrow the type, we would corrupt the input
                            // Therefore, instead we modify the type we are reading and cast
                            // the previously read values to the new type
                            assert(CalculateCastType(cat, subType._type) == CastType::Widening);
                            const auto catType = TypeDesc{cat};
                            const size_t cpySize = catType.GetSize() * count;
                            std::unique_ptr<uint8_t[]> tempCpy = std::make_unique<uint8_t[]>(cpySize);
                            auto tempCpyIterator = tempCpy.get();
                            std::memcpy(tempCpy.get(), dest, cpySize);
                            
                            dstIterator = dest;
                            assert(ptrdiff_t(destSize) - dstIteratorSize == ptrdiff_t(cpySize));
                            dstIteratorSize = ptrdiff_t(destSize);
                            for (auto redoIt = startIt; redoIt != rit; ++redoIt) {
                                bool castSuccess = Cast(
                                    { dstIterator, PtrAdd(dstIterator, subType.GetSize()) }, subType, 
                                    { tempCpyIterator, PtrAdd(tempCpyIterator, catType.GetSize()) }, catType);
                                assert(castSuccess);
                                (void)castSuccess;
                                
                                dstIterator = PtrAdd(dstIterator, subType.GetSize());
                                dstIteratorSize -= subType.GetSize();
                                
                                tempCpyIterator = tempCpyIterator + catType.GetSize();
                            }
                            
                            cat = subType._type;
                        }

                        assert(subType._arrayCount <= 1);
                        dstIterator = PtrAdd(dstIterator, subType.GetSize());
                        dstIteratorSize -= subType.GetSize();
                        ++count;
                    }

                    TypeHint hint = TypeHint::None;
                    if (cm.size() >= 2 && cm[2].length() >= 1) {
                        if (tolower(cm[2].str()[0]) == 'v') hint = TypeHint::Vector;
                        if (tolower(cm[2].str()[0]) == 'c') hint = TypeHint::Color;
                    }

                    return TypeDesc{cat, uint16_t(count), hint};
                }
            }

            return TypeDesc{TypeCat::Void};
        }

        std::string AsString(const void* data, size_t dataSize, const TypeDesc& desc, bool strongTyping)
        {
            if (desc._typeHint == TypeHint::String) {
                if (desc._type == TypeCat::UInt8 || desc._type == TypeCat::Int8) {
                    return std::string((const char*)data, (const char*)PtrAdd(data, desc._arrayCount * sizeof(char)));
                }
                if (desc._type == TypeCat::UInt16 || desc._type == TypeCat::Int16) {
                    return Conversion::Convert<std::string>(std::basic_string<utf16>((const utf16*)data, (const utf16*)PtrAdd(data, desc._arrayCount * sizeof(utf16))));
                }
            }

            std::stringstream result;
            assert(dataSize >= desc.GetSize());
            auto arrayCount = unsigned(desc._arrayCount);
            if (arrayCount > 1) result << "{";

            for (auto i=0u; i<arrayCount; ++i) {
                if (i!=0) result << ", ";

                if (strongTyping) {
                    switch (desc._type) {
                    case TypeCat::Bool:     if (*(bool*)data) { result << "true"; } else { result << "false"; }; break;
                    case TypeCat::Int8:     result << (int32_t)*(int8_t*)data << "i8"; break;
                    case TypeCat::UInt8:    result << (uint32_t)*(uint8_t*)data << "u8"; break;
                    case TypeCat::Int16:    result << *(int16_t*)data << "i16"; break;
                    case TypeCat::UInt16:   result << *(uint16_t*)data << "u16"; break;
                    case TypeCat::Int32:    result << *(int32_t*)data << "i"; break;
                    case TypeCat::UInt32:   result << *(uint32_t*)data << "u"; break;
                    case TypeCat::Int64:    result << *(int64_t*)data << "i64"; break;
                    case TypeCat::UInt64:   result << *(uint64_t*)data << "u64"; break;
                    case TypeCat::Float:    result << *(float*)data << "f"; break;
                    case TypeCat::Double:   result << *(double*)data << "f64"; break;
                    case TypeCat::Void:     result << ""; break;
                    default:                result << "<<error>>"; break;
                    }
                } else {
                    switch (desc._type) {
                    case TypeCat::Bool:     result << *(bool*)data; break;
                    case TypeCat::Int8:     result << (int32_t)*(int8_t*)data; break;
                    case TypeCat::UInt8:    result << (uint32_t)*(uint8_t*)data; break;
                    case TypeCat::Int16:    result << *(int16_t*)data; break;
                    case TypeCat::UInt16:   result << *(uint16_t*)data; break;
                    case TypeCat::Int32:    result << *(int32_t*)data; break;
                    case TypeCat::UInt32:   result << *(uint32_t*)data; break;
                    case TypeCat::Int64:    result << *(int64_t*)data; break;
                    case TypeCat::UInt64:   result << *(uint64_t*)data; break;
                    case TypeCat::Float:    result << *(float*)data; break;
                    case TypeCat::Double:   result << *(double*)data; break;
                    case TypeCat::Void:     result << ""; break;
                    default:                result << "<<error>>"; break;
                    }
                }

                    // skip forward one element
                data = PtrAdd(data, TypeDesc{desc._type}.GetSize());
            }

            if (arrayCount > 1) {
                result << "}";
                switch (desc._typeHint) {
                case TypeHint::Color: result << "c"; break;
                case TypeHint::Vector: result << "v"; break;
                default: break;
                }
            }

            return result.str();
        }

        std::string AsString(IteratorRange<const void*> data, const TypeDesc& type, bool strongTyping)
        {
            return AsString(data.begin(), data.size(), type, strongTyping);
		}

		void Cleanup()
		{
            s_parsingChar.reset();
        }

        template TypeDesc Parse(StringSection<utf8> expression, void* dest, size_t destSize);

    }
}
