#include "olv_xml.hpp"
#include <whb/log.h>

namespace nn::olv::xml
{
    const char *XML_ERRORS[] = {
        "Error_None",
        "Error_NodeFull",
        "Error_OverMaxRecursive",
        "Error_CharEncoding",
        "Error_CalcElementNum",
        "Error_NotExistRootElement",
        "Error_CharBeforeRootElement",
        "Error_CharAfterRootElement",
        "Error_IllegalEndText",
        "Error_StartTag",
        "Error_EndTag",
        "Error_MismatchStartEndTag",
        "Error_NotName",
        "Error_NotCharData",
        "Error_Reference",
        "Error_Content",
        "Error_UnsupportContentFormat",
        "Error_EntityRef",
        "Error_CharRef",
        "Error_CharRefDec",
        "Error_CharRefHex",
    };

    const char *XMLError_ToString(XMLError err)
    {
        if (err >= XMLError::Error_MAX)
            return "";
        else
            return XML_ERRORS[(int)err];
    }

    XMLParsingStruct sParsingStruct = {};

    inline Parser *GetParserInstance()
    {
        return sParsingStruct.instance;
    }

    bool Internal_InRange(uint32_t minR, uint32_t maxR, uint32_t value)
    {
        return minR <= value && value <= maxR;
    }

    bool IsBlankCharacter(uint32_t param_1)
    {
        return !((param_1 < 9) || (((10 < param_1 && (param_1 != 0xd)) && (param_1 != 0x20))));
    }

    uint8_t CountSpecialCharacters(uint8_t *chars, uint32_t length)
    {

        static uint8_t SPECIAL_CHARS[] = {
            0,
            0x7f,
            0xc2,
            0xdf,
            0xe0,
            0xef,
            0xf0,
            0xf7,
            0xf8,
            0xfb,
            0xfc,
            0xfd,
            0,  // Useless
            0}; // Useless too

        uint32_t count = 0;
        while (1)
        {
            if (count >= length)
                return 0;

            if (Internal_InRange(SPECIAL_CHARS[2 * count], SPECIAL_CHARS[2 * count + 1], *chars))
                break;

            if (++count >= 6)
                return 0;
        }

        for (uint32_t i = 1; i < count + 1 && i < length; ++i)
        {
            if (!Internal_InRange(0x80u, 0xBFu, chars[i]))
                return 0;
        }

        return count + 1;
    }

    bool ValidateCharacters(uint8_t *chars, uint32_t length)
    {
        return CountSpecialCharacters(chars, length) != 0;
    }

    bool Internal_ValidateCharacterAgainstList(CharacterRange *validateRangeList, uint32_t validateRangeListLen, uint32_t *specialList, uint32_t specialListLen, uint32_t character)
    {
        bool result;
        uint32_t idx = 0;
        if (validateRangeListLen)
        {
            while (!Internal_InRange(validateRangeList[idx].minVal, validateRangeList[idx].maxVal, character))
            {
                if (++idx >= validateRangeListLen)
                    goto LABEL_4;
            }
        LABEL_6:
            result = 1;
        }
        else
        {
        LABEL_4:
            for (uint32_t i = 0; i < specialListLen; ++i)
            {
                if (specialList[i] == character)
                    goto LABEL_6;
            }
            result = 0;
        }
        return result;
    }

    bool Internal_ValidateCharacter_List1(uint32_t character)
    {
        return Internal_ValidateCharacterAgainstList(VALIDATE_LIST_RANGE1, 150, SPECIAL_LIST1, 52, character);
    }

    bool Internal_ValidateCharacter_List2(uint32_t character)
    {
        return Internal_ValidateCharacterAgainstList(VALIDATE_LIST_RANGE2, 2, SPECIAL_LIST2, 1, character);
    }

    bool Internal_ValidateCharacter_List3(uint32_t character)
    {
        return Internal_ValidateCharacterAgainstList(VALIDATE_LIST_RANGE3, 15, SPECIAL_LIST3, 0, character);
    }

    bool Internal_ValidateCharacter_List4(uint32_t character)
    {
        return Internal_ValidateCharacterAgainstList(VALIDATE_LIST_RANGE4, 66, SPECIAL_LIST4, 29, character);
    }

    bool Internal_ValidateCharacter_List5(uint32_t character)
    {
        return Internal_ValidateCharacterAgainstList(VALIDATE_LIST_RANGE5, 3, SPECIAL_LIST5, 8, character);
    }

    bool Internal_ValidateCharacter_List1_2(uint32_t character)
    {
        return !(!Internal_ValidateCharacter_List1(character) && !Internal_ValidateCharacter_List2(character));
    }

    bool Internal_ValidateCharacterForValidName(unsigned int a1)
    {
        bool v2; // r30

        v2 = 1;
        if (!Internal_ValidateCharacter_List1_2(a1) && !Internal_ValidateCharacter_List3(a1) && ((a1 < '-' || ((('.' < a1 && (a1 != ':')) && (a1 != '_'))))) && !Internal_ValidateCharacter_List4(a1) && !Internal_ValidateCharacter_List5(a1))
            v2 = 0;

        return v2;
    }

    void ReadUTFCharacter(uint32_t *outVal, uint8_t *chars, uint32_t length)
    {

        static uint8_t SPECIAL_CHARS_CONVERSION[] = {0, 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01, 0, 0};

        uint8_t numSpecialChars = CountSpecialCharacters(chars, length);
        if (numSpecialChars)
        {
            uint32_t v6 = 0;
            for (uint32_t i = 0; i < numSpecialChars; i++)
            {
                uint32_t v8 = 0x3F;
                if (!i)
                    v8 = SPECIAL_CHARS_CONVERSION[numSpecialChars];

                uint8_t v10 = chars[i];
                v6 = (v6 << 6) | (v10 & v8);
            }
            *outVal = v6;
        }
    }

    void Parser::Destroy(HeapId heapId)
    {
        if (this)
        {
            if (this->rootNodes)
            {
                Internal_Free(this->rootNodes, HEAP_XML);
                this->rootNodes = nullptr;
            }
            if (this->nodeStack)
                this->ResetTree(this->nodeStack);

            if ((heapId & 1) != 0)
                Internal_ForceFree(this);
        }
    }

    void Internal_DestroyParserInstance()
    {
        if (GetParserInstance())
        {
            GetParserInstance()->Destroy(HEAP_XML);
            if (GetParserInstance())
            {
                Internal_Free(GetParserInstance(), HEAP_MAIN);
                sParsingStruct.instance = nullptr;
            }
        }
    }

    int Parser::NodeCheck(char *rawData, uint32_t rawDataSize, uint32_t *outDepth)
    {

        int numOpeningTags = 0;
        int tagDepth = 0;
        int numClosingTags = 0;
        int maxTagDepth = 0;
        int isInsideTagFlag = 0;

        ParsingObject v11(rawData, rawDataSize);
        while (!v11.ReadLengthOverflow() && !v11.HasInvalidCharacters())
        {
            if (isInsideTagFlag)
            {
                if (v11.ReadCharacter() == '/')
                {
                    --tagDepth;
                    ++numClosingTags;
                    if (tagDepth < 0)
                        return 0;
                }
                else
                {
                    ++tagDepth;
                    ++numOpeningTags;
                    if (tagDepth > maxTagDepth)
                        maxTagDepth = tagDepth;
                }
                isInsideTagFlag = 0;
                v11.SkipSpecialCharacters();
            }
            else
            {
                isInsideTagFlag = v11.ReadCharacter() == '<';
                v11.SkipSpecialCharacters();
            }
        }

        if (numOpeningTags != numClosingTags)
            return 0;

        *outDepth = maxTagDepth + 1;
        return numOpeningTags;
    }

    bool Parser::EnsureUTF8(char *rawData, uint32_t rawDataSize)
    {
        ParsingObject v6(rawData, rawDataSize);
        while (!v6.ReadLengthOverflow() && !v6.HasInvalidCharacters())
            v6.SkipSpecialCharacters();

        if (!v6.HasInvalidCharacters())
            return true;

        this->SetError(XMLError::Error_CharEncoding, v6.currentReadLength);
        return false;
    }

    void Parser::Parse(char *rawData, uint32_t rawDataSize)
    {
        this->Reset();
        if (!this->EnsureUTF8(rawData, rawDataSize))
        {
            nn::olv::Report::Print(REPORT_TYPE_512, "XMLParser ERROR char set is not UTF8 !!!\n");
            return;
        }

        uint32_t nodeDepth = 0;
        int numTags = this->NodeCheck(rawData, rawDataSize, &nodeDepth);
        if (!numTags)
        {
            this->SetError(XMLError::Error_CalcElementNum, 0);
            nn::olv::Report::Print(REPORT_TYPE_512, "XMLParser ERROR node check failed !!!\n");
            return;
        }

        this->numTags = numTags;
        this->rootNodes = (XMLNode *)Internal_Alloc(sizeof(XMLNode) * numTags, HEAP_XML);
        if (!this->rootNodes)
        {
            nn::olv::Report::Print(REPORT_TYPE_512, "XMLParser ERROR memory allocation failed !!! -rootNode\n");
            return;
        }

        memset(this->rootNodes, 0, sizeof(XMLNode) * numTags);
        this->nodeStack = this->InitNodeStack(nodeDepth);
        if (!this->nodeStack)
        {
            nn::olv::Report::Print(REPORT_TYPE_512, "XMLParser ERROR memory allocation failed !!! -nodeStack\n");
            if (this->rootNodes)
            {
                Internal_Free(this->rootNodes, HEAP_XML);
                this->rootNodes = nullptr;
            }
            return;
        }

        this->nodeDepth = nodeDepth;
        if (this->ParseImpl(rawData, rawDataSize))
            this->SetError(Error_None, 0);
        else
            nn::olv::Report::Print(REPORT_TYPE_512, "XMLParser ERROR parse failed !!!\n");
    }

    bool Parser::ParseImpl(char *rawData, uint32_t rawDataSize)
    {
        ParsingObject v9(rawData, rawDataSize);
        v9.SkipBlankCharacters();

        if (v9.ReadLengthOverflow())
        {
            this->SetError(Error_NotExistRootElement, v9.currentReadLength);
            return false;
        }
        else if (v9.ReadCharacter() == '<')
        {
            bool result = this->ParseNodesRecursive(v9);
            if (result)
            {
                v9.SkipBlankCharacters();
                if (v9.ReadLengthOverflow())
                {
                    return true;
                }
                else
                {
                    this->SetError(Error_CharAfterRootElement, v9.currentReadLength);
                    return false;
                }
            }
            return result;
        }
        else
        {
            this->SetError(Error_CharBeforeRootElement, v9.currentReadLength);
        }
        return false;
    }

    bool Parser::ParseNodesRecursive(ParsingObject &object)
    {
        if (this->EnsureRecursiveCoherency())
        {
            bool res = true;
            if (!this->ReadOpeningTag(object) || !this->ReadTagData(object) || !this->ReadClosingTag(object))
            {
                res = false;
            }
            this->currentDepth--;
            return res;
        }
        else
        {
            this->SetError(Error_OverMaxRecursive, object.currentReadLength);
            return false;
        }
    }

    bool Parser::ReadOpeningTag_FindOpening(ParsingObject &object)
    {
        if (object.ReadLengthOverflow())
        {
            this->SetError(Error_IllegalEndText, object.currentReadLength);
            return false;
        }
        else
        {
            uint32_t character = object.ReadCharacter();
            if (character != '<')
                this->SetError(Error_StartTag, object.currentReadLength);

            object.SkipSpecialCharacters();
            return character == '<';
        }
    }

    bool Parser::WriteCurrentTagName(char *name, uint32_t length)
    {
        bool result;             // r3
        XMLNodeStack *nodeStack; // r8

        if (this->currentNumTag < this->numTags)
        {
            nodeStack = nodeStack;
            if (this->nodeStack->currentDepth == this->nodeStack->numDepth)
            {
                this->SetError(Error_OverMaxRecursive, 0);
                result = 0;
            }
            else
            {
                this->nodeStack->entries[this->nodeStack->currentDepth] = &this->rootNodes[this->currentNumTag];
                this->nodeStack->currentDepth++;

                XMLNode *node = this->GetLatestNodeByDepth();
                node->tagName = name;
                node->tagNameLen = length;

                this->currentNumTag++;
                result = true;
            }
        }
        else
        {
            this->SetError(Error_NodeFull, 0);
            result = false;
        }
        return result;
    }

    bool Parser::ReadOpeningTag_ReadNameImpl(ParsingObject &object)
    {
        uint32_t v4;
        uint32_t v7;

        if (!object.ReadLengthOverflow() && ((v4 = object.ReadCharacter(), Internal_ValidateCharacter_List1_2(v4)) || object.ReadCharacter() == ':' || object.ReadCharacter() == '_'))
        {
            while (!object.ReadLengthOverflow())
            {
                v7 = object.ReadCharacter();
                if (!Internal_ValidateCharacterForValidName(v7))
                    break;
                object.SkipSpecialCharacters();
            }
            return true;
        }
        else
        {
            this->SetError(Error_NotName, object.currentReadLength);
        }
        return false;
    }

    bool Parser::ReadOpeningTag_ReadName(ParsingObject &object)
    {
        ParsingObject v10(object);
        if (this->ReadOpeningTag_ReadNameImpl(object))
        {
            char *curNamePtr = v10.GetCurrentReadPointer();
            uint32_t endNameOffset = object.currentReadLength;
            uint32_t curNameOffset = v10.currentReadLength;
            if (this->WriteCurrentTagName(curNamePtr, endNameOffset - curNameOffset))
                return true;
        }
        return false;
    }

    bool Parser::FindClosing(ParsingObject &object, XMLError errorToSet)
    {
        if (object.ReadLengthOverflow())
        {
            this->SetError(Error_IllegalEndText, object.currentReadLength);
            return false;
        }

        uint32_t character = object.ReadCharacter();
        if (character != '>')
            this->SetError(errorToSet, object.currentReadLength);

        object.SkipSpecialCharacters();
        return character == '>';
    }

    bool Parser::ReadOpeningTag(ParsingObject &object)
    {
        if (this->ReadOpeningTag_FindOpening(object) && this->ReadOpeningTag_ReadName(object))
        {
            object.SkipBlankCharacters();
            if (this->FindClosing(object, Error_StartTag))
                return true;
        }

        return false;
    }

    bool Parser::CheckIsCharData(ParsingObject &object, uint32_t length)
    {
        int v6;          // r31
        unsigned int v7; // r29

        v6 = 0;
        v7 = 0;
        if (length)
        {
            while (v6)
            {
                if (v6 != 1)
                {
                    if (object.ReadCharacter() != ']')
                    {
                        if (object.ReadCharacter() == '>')
                        {
                            this->SetError(Error_NotCharData, object.currentReadLength);
                            return 0;
                        }
                    LABEL_13:
                        v6 = 0;
                    LABEL_14:
                        object.SkipSpecialCharacters();
                        ++v7;
                        goto LABEL_15;
                    }
                    goto LABEL_10;
                }
                if (object.ReadCharacter() != ']')
                    goto LABEL_13;
                v6 = 2;
                object.SkipSpecialCharacters();
                ++v7;
            LABEL_15:
                if (v7 >= length)
                    return 1;
            }
            if (object.ReadCharacter() != ']')
                goto LABEL_14;
        LABEL_10:
            v6 = 1;
            object.SkipSpecialCharacters();
            ++v7;
            goto LABEL_15;
        }
        return 1;
    }

    bool Parser::FindEndTagData(ParsingObject &object)
    {
        uint32_t length = 0;
        ParsingObject v7(object);
        while (!v7.ReadLengthOverflow())
        {
            if (v7.ReadCharacter() == '<')
                break;

            if (v7.ReadCharacter() == '&')
                break;

            v7.SkipSpecialCharacters();

            length++;
        }

        return this->CheckIsCharData(object, length);
    }

    bool Parser::IsXMLEntity(ParsingObject &object)
    {
        if (object.ReadLengthOverflow())
        {
            this->SetError(Error_IllegalEndText, object.currentReadLength);
            return false;
        }
        else if (object.ReadCharacter() == '&')
        {
            object.SkipSpecialCharacters();
            return true;
        }
        else
        {
            this->SetError(Error_EntityRef, object.currentReadLength);
            return false;
        }
    }

    bool Parser::IsXMLEntityClosure(ParsingObject &object, XMLError errorToThrow)
    {
        if (object.ReadLengthOverflow())
        {
            this->SetError(Error_IllegalEndText, object.currentReadLength);
            return false;
        }
        else if (object.ReadCharacter() == ';')
        {
            object.SkipSpecialCharacters();
            return true;
        }
        else
        {
            this->SetError(errorToThrow, object.currentReadLength);
            return false;
        }
    }

    bool Parser::VerifyXMLEntities(ParsingObject &object)
    {
        return this->IsXMLEntity(object) && this->ReadOpeningTag_ReadNameImpl(object) && this->IsXMLEntityClosure(object, Error_EntityRef);
    }

    bool Parser::VerifyXMLReferencesImpl(ParsingObject &object)
    {
        ParsingObject v12(object);
        if (this->VerifyXMLReferencesImplDec(v12))
            goto LABEL_4;

        if (this->CheckCantContinueError())
            return false;

        v12 = ParsingObject(object);
        if (this->VerifyXMLReferencesImplHex(v12))
        {
        LABEL_4:
            object = v12;
            return true;
        }

        if (this->CheckCantContinueError())
            return false;

        this->SetError(Error_CharRef, object.currentReadLength);
        return false;
    }

    bool Parser::IsXMLDecimalReference(ParsingObject &object)
    {
        const char *VALID_DEC_REF = "&#";
        int v4 = 0;
        do
        {
            if (object.ReadLengthOverflow())
            {
                this->SetError(Error_IllegalEndText, object.currentReadLength);
                return false;
            }
            if (object.ReadCharacter() != VALID_DEC_REF[v4])
            {
                this->SetError(Error_CharRefDec, object.currentReadLength);
                return false;
            }
            object.SkipSpecialCharacters();
            v4++;
        } while (v4 < 2);

        return true;
    }

    bool Parser::ValidateXMLDecimalReference(ParsingObject &object)
    {
        int i;
        for (i = 0; i < !object.ReadLengthOverflow(); ++i)
        {
            uint32_t c = object.ReadCharacter();
            if (!Internal_InRange('0', '9', c))
                break;
            object.SkipSpecialCharacters();
        }

        if (i)
            return true;

        this->SetError(Error_CharRefDec, object.currentReadLength);
        return false;
    }

    bool Parser::VerifyXMLReferencesImplDec(ParsingObject &object)
    {
        return this->IsXMLDecimalReference(object) && this->ValidateXMLDecimalReference(object) && this->IsXMLEntityClosure(object, Error_CharRefDec);
    }

    bool Parser::IsXMLHexadecimalReference(ParsingObject &object)
    {
        const char *VALID_HEX_REF = "&#x";
        int v4 = 0;
        do
        {
            if (object.ReadLengthOverflow())
            {
                this->SetError(Error_IllegalEndText, object.currentReadLength);
                return false;
            }
            if (object.ReadCharacter() != VALID_HEX_REF[v4])
            {
                this->SetError(Error_CharRefHex, object.currentReadLength);
                return false;
            }
            object.SkipSpecialCharacters();
            v4++;
        } while (v4 < 3);
        return true;
    }

    bool Parser::ValidateXMLHexadecimalReference(ParsingObject &object)
    {
        int i;
        for (i = 0; i < !object.ReadLengthOverflow(); ++i)
        {
            uint32_t c = object.ReadCharacter();
            if ((!Internal_InRange('0', '9', c) && !Internal_InRange('A', 'Z', c) && !Internal_InRange('a', 'z', c)))
                break;
            object.SkipSpecialCharacters();
        }

        if (i)
            return true;

        this->SetError(Error_CharRefHex, object.currentReadLength);

        return false;
    }

    bool Parser::VerifyXMLReferencesImplHex(ParsingObject &object)
    {
        return this->IsXMLHexadecimalReference(object) && this->ValidateXMLHexadecimalReference(object) && this->IsXMLEntityClosure(object, Error_CharRefHex);
    }

    bool Parser::VerifyXMLReferences(ParsingObject &object)
    {
        ParsingObject v12(object);
        if (this->VerifyXMLEntities(v12) || (v12 = ParsingObject(object), this->VerifyXMLReferencesImpl(v12)))
        {
            object = v12;
            return true;
        }
        else
        {
            this->SetError(Error_Reference, object.currentReadLength);
            return false;
        }
    }

    bool Parser::ReadTagDataImpl(ParsingObject &object)
    {
        ParsingObject v12(object);
        if (this->ParseNodesRecursive(v12))
            goto LABEL_4;

        if (this->CheckCantContinueError())
            return false;

        v12 = ParsingObject(object);
        if (this->VerifyXMLReferences(v12))
        {
        LABEL_4:
            object = v12;
            return true;
        }

        if (this->CheckCantContinueError())
            return false;

        this->SetError(Error_Content, object.currentReadLength);
        return false;
    }

    bool Parser::ReadTagData(ParsingObject &object)
    {
        ParsingObject newParser1(object);
        if (!this->FindEndTagData(newParser1))
        {
            newParser1 = ParsingObject(object);
        }

    LABEL_3:
        ParsingObject newParser2(newParser1);
        while (1)
        {
            if (!this->ReadTagDataImpl(newParser1))
                break;

            newParser2 = newParser1;
            if (this->FindEndTagData(newParser1))
                goto LABEL_3;

            newParser1 = newParser2;
        }

        if (this->CheckCantContinueError())
            return false;

        if (!this->GetLatestNodeByDepth()->unk_14)
        {
            XMLNode *node = this->GetLatestNodeByDepth();
            node->tagContent = object.GetCurrentReadPointer();
            node->tagContentLen = newParser2.currentReadLength - object.currentReadLength;
        }

        object = newParser2;
        return true;
    }

    bool Parser::IsClosingTag(ParsingObject &object)
    {
        const char *VALID_CLOSING_TAG = "</";
        int v4 = 0;
        do
        {
            if (object.ReadLengthOverflow())
            {
                this->SetError(Error_IllegalEndText, object.currentReadLength);
                return false;
            }
            if (object.ReadCharacter() != VALID_CLOSING_TAG[v4])
            {
                this->SetError(Error_EndTag, object.currentReadLength);
                return false;
            }
            object.SkipSpecialCharacters();
            v4++;
        } while (v4 < 2);

        return true;
    }

    bool XMLNode::CompareNodeTagName(const char *name, uint32_t length)
    {
        if (this->tagNameLen == length)
            return strncmp(this->tagName, name, length) == 0;

        return false;
    }

    void Parser::WriteDepthToTree()
    {
        XMLNode *node = this->GetLatestNodeByDepth();
        --this->nodeStack->currentDepth;
        if (!this->IsNodeStackRootDepth())
        {
            XMLNode *parentNode = this->GetLatestNodeByDepth();
            XMLNode *v5 = parentNode->unk_14;
            if (!v5)
            {
                v5 = node;
                parentNode->unk_14 = node;
            }
            if (v5 != node)
            {
                for (XMLNode *ent = v5->unk_10; ent; ent = ent->unk_10)
                    v5 = ent;
                v5->unk_10 = node;
            }
        }
    }

    bool Parser::ReadClosingTagName(ParsingObject &object)
    {
        ParsingObject v10(object);
        bool result = this->ReadOpeningTag_ReadNameImpl(object);
        if (!result)
        {
            return false;
        }

        char *tagName = v10.GetCurrentReadPointer();
        uint32_t tagNameLen = object.currentReadLength - v10.currentReadLength;

        XMLNode *node = this->GetLatestNodeByDepth();
        if (node->CompareNodeTagName(tagName, tagNameLen))
        {
            this->WriteDepthToTree();
            return true;
        }

        this->SetError(Error_EndTag, v10.currentReadLength);
        return false;
    }

    bool Parser::ReadClosingTag(ParsingObject &object)
    {
        bool result = false;
        if (this->IsClosingTag(object))
        {
            if (this->ReadClosingTagName(object))
            {
                object.SkipBlankCharacters();
                if (this->FindClosing(object, Error_EndTag))
                {
                    result = true;
                }
            }
        }
        return result;
    }

    int Internal_UnknownCallback(int unk, XMLNode *node, void *userptr)
    {
        return 0;
    }

    bool Internal_CreateParseObject()
    {
        Parser *parser = (Parser *)Internal_Alloc(sizeof(Parser), HEAP_MAIN);
        if (parser)
        {
            sParsingStruct.instance = new (parser) Parser();
            return true;
        }
        nn::olv::Report::Print(REPORT_TYPE_2048, "Create Parser Object FAILED.\n");
        return false;
    }

    char *Internal_ReadRawData(char *buffer, uint32_t bufferSize, uint32_t *outSize)
    {
        if (!buffer || !bufferSize)
        {
            nn::olv::Report::Print(REPORT_TYPE_2048, "rawdata read error! invalid parameter.\n", outSize);
            return nullptr;
        }

        uint32_t i;
        for (i = 0;; ++i)
        {
            if (i == bufferSize)
            {
                nn::olv::Report::Print(REPORT_TYPE_2048, "rawdata read error!\n");
                return nullptr;
            }

            if (buffer[i] == '>')
                break;
        }

        uint32_t offset = i + 1;
        if (outSize)
            *outSize = offset;

        return &buffer[offset];
    }

    bool Internal_CreateRawDataImpl(char *buffer, int bufferSize)
    {
        if (!buffer) // xd
            return false;

        uint32_t rawDataSize;
        char *rawData = Internal_ReadRawData(buffer, bufferSize, &rawDataSize);
        if (!rawData)
            return false;

        GetParserInstance()->Parse(rawData, bufferSize - rawDataSize);
        if (!GetParserInstance()->HasError())
            return true;

        uint32_t pos = GetParserInstance()->GetReadLength();
        XMLError err = GetParserInstance()->GetError();
        const char *reason = XMLError_ToString(err);
        nn::olv::Report::Print(REPORT_TYPE_512, "rawData Create Failed. pos:%d reason:%s.\n", pos, reason);
        return false;
    }

    bool Internal_CreateRawData(char *buffer, uint32_t bufferSize)
    {
        return buffer && Internal_CreateRawDataImpl(buffer, bufferSize);
    }

    int Internal_AssignCurrentNode(XMLNode *node)
    {
        sParsingStruct.tagName = node->tagName;
        sParsingStruct.tagNameLen = node->tagNameLen;
        sParsingStruct.tagContent = node->tagContent;
        sParsingStruct.tagContentLen = node->tagContentLen;
        return 0;
    }

    int Internal_OperateCallbacks(XMLNode *node, XMLParseCallback parseCallback, XMLParseCallback unknownCallback, void *userptr, int *outRes)
    {
        if (!node)
            return (int)node;

        do
        {
            sParsingStruct.currentNode = node;
            int res = Internal_AssignCurrentNode(node);
            if (res < 0)
            {
                nn::olv::Report::Print(REPORT_TYPE_512, "Memory Allocation FAILED from rawData update.\n");
                return 2;
            }

            int tmp = *outRes;
            res = parseCallback(tmp, node, userptr);
            *outRes = tmp + 1;
            if (res == 1)
            {
                nn::olv::Report::Print(REPORT_TYPE_512, "Operation call back return ERROR.\n");
                return 1;
            }

            if (res)
                return res;

            if (node->unk_14)
            {
                sParsingStruct.PushNextNode(node);
                res = Internal_OperateCallbacks(node->unk_14, parseCallback, unknownCallback, userptr, outRes);
                sParsingStruct.PopNextNode();
                if (res != 0)
                {
                    return res;
                }
            }

            unknownCallback(*outRes, node, userptr);
            node = node->unk_10;
        } while (node != nullptr);

        return (int)node;
    }

    int Internal_SendXMLToCallbacks(XMLParseCallback parseCallback, XMLParseCallback unknownCallback, void *userptr)
    {
        int tmp = 0;
        sParsingStruct.Reset();
        sParsingStruct.PushNextNode(GetParserInstance()->rootNodes);
        return Internal_OperateCallbacks(GetParserInstance()->rootNodes, parseCallback, unknownCallback, userptr, &tmp);
    }

    int Internal_ParseXML(const char *buffer, uint32_t bufferSize, XMLParseCallback parseCallback, XMLParseCallback unknownCallback, void *userptr)
    {
        if (!Internal_CreateParseObject())
            return 1;

        if (Internal_CreateRawData((char *)buffer, bufferSize))
        {
            int res = Internal_SendXMLToCallbacks(parseCallback, unknownCallback, userptr);
            Internal_DestroyParserInstance();
            return res;
        }
        else
        {
            Internal_DestroyParserInstance();
            return 1;
        }

        return 0;
    }

    int ParseXML(const char *buffer, uint32_t bufferSize, XMLParseCallback callback, void *userptr)
    {
        return Internal_ParseXML(buffer, bufferSize, callback, Internal_UnknownCallback, userptr);
    }

    char *GetTagName()
    {
        return sParsingStruct.tagName;
    }

    uint32_t GetTagNameLength()
    {
        return sParsingStruct.tagNameLen;
    }

    char *GetTagContent()
    {
        return sParsingStruct.tagContent;
    }

    uint32_t GetTagContentLength()
    {
        return sParsingStruct.tagContentLen;
    }

    bool CompareAgainstTagName(char *tagName1, const char *tagName2)
    {
        return strncmp(tagName1, tagName2, xml::GetTagNameLength()) == 0;
    }

    XMLNode *FindChildByName(XMLNode *node, const char *name, uint32_t length)
    {
        if (node != nullptr)
        {
            do
            {
                if (node->CompareNodeTagName(name, length) != 0)
                    return node;

                node = node->unk_10;
            } while (node != nullptr);
        }
        return node;
    }

    int Internal_AssignCurrentNode(XMLNode *node);

    bool SelectNodeByName(const char *nodeName)
    {
        const char *nodeNameEnd = nodeName - 1;
        do
        {
            nodeNameEnd++;
        } while (*nodeNameEnd);

        int res;
        XMLNode *node = FindChildByName(sParsingStruct.currentNode, nodeName, (uint32_t)(nodeNameEnd - nodeName));
        if ((node != nullptr) && (res = Internal_AssignCurrentNode(node), res < 0))
            return false;

        return node != nullptr;
    }

    CharacterRange VALIDATE_LIST_RANGE1[] = {
        CharacterRange(0x41, 0x5A),
        CharacterRange(0x61, 0x7A),
        CharacterRange(0xC0, 0xD6),
        CharacterRange(0xD8, 0xF6),
        CharacterRange(0xF8, 0xFF),
        CharacterRange(0x100, 0x131),
        CharacterRange(0x134, 0x13E),
        CharacterRange(0x141, 0x148),
        CharacterRange(0x14A, 0x17E),
        CharacterRange(0x180, 0x1C3),
        CharacterRange(0x1CD, 0x1F0),
        CharacterRange(0x1F4, 0x1F5),
        CharacterRange(0x1FA, 0x217),
        CharacterRange(0x250, 0x2A8),
        CharacterRange(0x2BB, 0x2C1),
        CharacterRange(0x388, 0x38A),
        CharacterRange(0x38E, 0x3A1),
        CharacterRange(0x3A3, 0x3CE),
        CharacterRange(0x3D0, 0x3D6),
        CharacterRange(0x3E2, 0x3F3),
        CharacterRange(0x401, 0x40C),
        CharacterRange(0x40E, 0x44F),
        CharacterRange(0x451, 0x45C),
        CharacterRange(0x45E, 0x481),
        CharacterRange(0x490, 0x4C4),
        CharacterRange(0x4C7, 0x4C8),
        CharacterRange(0x4CB, 0x4CC),
        CharacterRange(0x4D0, 0x4EB),
        CharacterRange(0x4EE, 0x4F5),
        CharacterRange(0x4F8, 0x4F9),
        CharacterRange(0x531, 0x556),
        CharacterRange(0x561, 0x586),
        CharacterRange(0x5D0, 0x5EA),
        CharacterRange(0x5F0, 0x5F2),
        CharacterRange(0x621, 0x63A),
        CharacterRange(0x641, 0x64A),
        CharacterRange(0x671, 0x6B7),
        CharacterRange(0x6BA, 0x6BE),
        CharacterRange(0x6C0, 0x6CE),
        CharacterRange(0x6D0, 0x6D3),
        CharacterRange(0x6E5, 0x6E6),
        CharacterRange(0x905, 0x939),
        CharacterRange(0x958, 0x961),
        CharacterRange(0x985, 0x98C),
        CharacterRange(0x98F, 0x990),
        CharacterRange(0x993, 0x9A8),
        CharacterRange(0x9AA, 0x9B0),
        CharacterRange(0x9B6, 0x9B9),
        CharacterRange(0x9DC, 0x9DD),
        CharacterRange(0x9DF, 0x9E1),
        CharacterRange(0x9F0, 0x9F1),
        CharacterRange(0xA05, 0xA0A),
        CharacterRange(0xA0F, 0xA10),
        CharacterRange(0xA13, 0xA28),
        CharacterRange(0xA2A, 0xA30),
        CharacterRange(0xA32, 0xA33),
        CharacterRange(0xA35, 0xA36),
        CharacterRange(0xA38, 0xA39),
        CharacterRange(0xA59, 0xA5C),
        CharacterRange(0xA72, 0xA74),
        CharacterRange(0xA85, 0xA8B),
        CharacterRange(0xA8F, 0xA91),
        CharacterRange(0xA93, 0xAA8),
        CharacterRange(0xAAA, 0xAB0),
        CharacterRange(0xAB2, 0xAB3),
        CharacterRange(0xAB5, 0xAB9),
        CharacterRange(0xB05, 0xB0C),
        CharacterRange(0xB0F, 0xB10),
        CharacterRange(0xB13, 0xB28),
        CharacterRange(0xB2A, 0xB30),
        CharacterRange(0xB32, 0xB33),
        CharacterRange(0xB36, 0xB39),
        CharacterRange(0xB5C, 0xB5D),
        CharacterRange(0xB5F, 0xB61),
        CharacterRange(0xB85, 0xB8A),
        CharacterRange(0xB8E, 0xB90),
        CharacterRange(0xB92, 0xB95),
        CharacterRange(0xB99, 0xB9A),
        CharacterRange(0xB9E, 0xB9F),
        CharacterRange(0xBA3, 0xBA4),
        CharacterRange(0xBA8, 0xBAA),
        CharacterRange(0xBAE, 0xBB5),
        CharacterRange(0xBB7, 0xBB9),
        CharacterRange(0xC05, 0xC0C),
        CharacterRange(0xC0E, 0xC10),
        CharacterRange(0xC12, 0xC28),
        CharacterRange(0xC2A, 0xC33),
        CharacterRange(0xC35, 0xC39),
        CharacterRange(0xC60, 0xC61),
        CharacterRange(0xC85, 0xC8C),
        CharacterRange(0xC8E, 0xC90),
        CharacterRange(0xC92, 0xCA8),
        CharacterRange(0xCAA, 0xCB3),
        CharacterRange(0xCB5, 0xCB9),
        CharacterRange(0xCE0, 0xCE1),
        CharacterRange(0xD05, 0xD0C),
        CharacterRange(0xD0E, 0xD10),
        CharacterRange(0xD12, 0xD28),
        CharacterRange(0xD2A, 0xD39),
        CharacterRange(0xD60, 0xD61),
        CharacterRange(0xE01, 0xE2E),
        CharacterRange(0xE32, 0xE33),
        CharacterRange(0xE40, 0xE45),
        CharacterRange(0xE81, 0xE82),
        CharacterRange(0xE87, 0xE88),
        CharacterRange(0xE94, 0xE97),
        CharacterRange(0xE99, 0xE9F),
        CharacterRange(0xEA1, 0xEA3),
        CharacterRange(0xEAA, 0xEAB),
        CharacterRange(0xEAD, 0xEAE),
        CharacterRange(0xEB2, 0xEB3),
        CharacterRange(0xEC0, 0xEC4),
        CharacterRange(0xF40, 0xF47),
        CharacterRange(0xF49, 0xF69),
        CharacterRange(0x10A0, 0x10C5),
        CharacterRange(0x10D0, 0x10F6),
        CharacterRange(0x1102, 0x1103),
        CharacterRange(0x1105, 0x1107),
        CharacterRange(0x110B, 0x110C),
        CharacterRange(0x110E, 0x1112),
        CharacterRange(0x1154, 0x1155),
        CharacterRange(0x115F, 0x1161),
        CharacterRange(0x116D, 0x116E),
        CharacterRange(0x1172, 0x1173),
        CharacterRange(0x11AE, 0x11AF),
        CharacterRange(0x11B7, 0x11B8),
        CharacterRange(0x11BC, 0x11C2),
        CharacterRange(0x1E00, 0x1E9B),
        CharacterRange(0x1EA0, 0x1EF9),
        CharacterRange(0x1F00, 0x1F15),
        CharacterRange(0x1F18, 0x1F1D),
        CharacterRange(0x1F20, 0x1F45),
        CharacterRange(0x1F48, 0x1F4D),
        CharacterRange(0x1F50, 0x1F57),
        CharacterRange(0x1F5F, 0x1F7D),
        CharacterRange(0x1F80, 0x1FB4),
        CharacterRange(0x1FB6, 0x1FBC),
        CharacterRange(0x1FC2, 0x1FC4),
        CharacterRange(0x1FC6, 0x1FCC),
        CharacterRange(0x1FD0, 0x1FD3),
        CharacterRange(0x1FD6, 0x1FDB),
        CharacterRange(0x1FE0, 0x1FEC),
        CharacterRange(0x1FF2, 0x1FF4),
        CharacterRange(0x1FF6, 0x1FFC),
        CharacterRange(0x212A, 0x212B),
        CharacterRange(0x2180, 0x2182),
        CharacterRange(0x3041, 0x3094),
        CharacterRange(0x30A1, 0x30FA),
        CharacterRange(0x3105, 0x312C),
        CharacterRange(0xAC00, 0xD7A3),
    };

    CharacterRange VALIDATE_LIST_RANGE2[] = {
        CharacterRange(0x4E00, 0x9FA5),
        CharacterRange(0x3021, 0x3029),
    };

    CharacterRange VALIDATE_LIST_RANGE3[] = {
        CharacterRange(0x30, 0x39),
        CharacterRange(0x660, 0x669),
        CharacterRange(0x6F0, 0x6F9),
        CharacterRange(0x966, 0x96F),
        CharacterRange(0x9E6, 0x9EF),
        CharacterRange(0xA66, 0xA6F),
        CharacterRange(0xAE6, 0xAEF),
        CharacterRange(0xB66, 0xB6F),
        CharacterRange(0xBE7, 0xBEF),
        CharacterRange(0xC66, 0xC6F),
        CharacterRange(0xCE6, 0xCEF),
        CharacterRange(0xD66, 0xD6F),
        CharacterRange(0xE50, 0xE59),
        CharacterRange(0xED0, 0xED9),
        CharacterRange(0xF20, 0xF29),
    };

    CharacterRange VALIDATE_LIST_RANGE4[] = {
        CharacterRange(0x300, 0x345),
        CharacterRange(0x360, 0x361),
        CharacterRange(0x483, 0x486),
        CharacterRange(0x591, 0x5A1),
        CharacterRange(0x5A3, 0x5B9),
        CharacterRange(0x5BB, 0x5BD),
        CharacterRange(0x5C1, 0x5C2),
        CharacterRange(0x64B, 0x652),
        CharacterRange(0x6D6, 0x6DC),
        CharacterRange(0x6DD, 0x6DF),
        CharacterRange(0x6E0, 0x6E4),
        CharacterRange(0x6E7, 0x6E8),
        CharacterRange(0x6EA, 0x6ED),
        CharacterRange(0x901, 0x903),
        CharacterRange(0x93E, 0x94C),
        CharacterRange(0x951, 0x954),
        CharacterRange(0x962, 0x963),
        CharacterRange(0x981, 0x983),
        CharacterRange(0x9C0, 0x9C4),
        CharacterRange(0x9C7, 0x9C8),
        CharacterRange(0x9CB, 0x9CD),
        CharacterRange(0x9E2, 0x9E3),
        CharacterRange(0xA40, 0xA42),
        CharacterRange(0xA47, 0xA48),
        CharacterRange(0xA4B, 0xA4D),
        CharacterRange(0xA70, 0xA71),
        CharacterRange(0xA81, 0xA83),
        CharacterRange(0xABE, 0xAC5),
        CharacterRange(0xAC7, 0xAC9),
        CharacterRange(0xACB, 0xACD),
        CharacterRange(0xB01, 0xB03),
        CharacterRange(0xB3E, 0xB43),
        CharacterRange(0xB47, 0xB48),
        CharacterRange(0xB4B, 0xB4D),
        CharacterRange(0xB56, 0xB57),
        CharacterRange(0xB82, 0xB83),
        CharacterRange(0xBBE, 0xBC2),
        CharacterRange(0xBC6, 0xBC8),
        CharacterRange(0xBCA, 0xBCD),
        CharacterRange(0xC01, 0xC03),
        CharacterRange(0xC3E, 0xC44),
        CharacterRange(0xC46, 0xC48),
        CharacterRange(0xC4A, 0xC4D),
        CharacterRange(0xC55, 0xC56),
        CharacterRange(0xC82, 0xC83),
        CharacterRange(0xCBE, 0xCC4),
        CharacterRange(0xCC6, 0xCC8),
        CharacterRange(0xCCA, 0xCCD),
        CharacterRange(0xCD5, 0xCD6),
        CharacterRange(0xD02, 0xD03),
        CharacterRange(0xD3E, 0xD43),
        CharacterRange(0xD46, 0xD48),
        CharacterRange(0xD4A, 0xD4D),
        CharacterRange(0xE34, 0xE3A),
        CharacterRange(0xE47, 0xE4E),
        CharacterRange(0xEB4, 0xEB9),
        CharacterRange(0xEBB, 0xEBC),
        CharacterRange(0xEC8, 0xECD),
        CharacterRange(0xF18, 0xF19),
        CharacterRange(0xF71, 0xF84),
        CharacterRange(0xF86, 0xF8B),
        CharacterRange(0xF90, 0xF95),
        CharacterRange(0xF99, 0xFAD),
        CharacterRange(0xFB1, 0xFB7),
        CharacterRange(0x20D0, 0x20DC),
        CharacterRange(0x302A, 0x302F),
    };

    CharacterRange VALIDATE_LIST_RANGE5[]{
        CharacterRange(0x3031, 0x3035),
        CharacterRange(0x309D, 0x309E),
        CharacterRange(0x30FC, 0x30FE),
    };

    uint32_t SPECIAL_LIST1[] = {0x386, 0x38C, 0x3DA, 0x3DC, 0x3DE, 0x3E0, 0x559, 0x6D5, 0x93D, 0x9B2, 0xA5E, 0xA8D, 0xABD, 0xAE0, 0xB3D, 0xB9C, 0xCDE, 0xE30, 0xE84, 0xE8A, 0xE8D, 0xEA5, 0xEA7, 0xEB0, 0xEBD, 0x1100, 0x1109, 0x113C, 0x113E, 0x1140, 0x114C, 0x114E, 0x1150, 0x1159, 0x1163, 0x1165, 0x1167, 0x1169, 0x1175, 0x119E, 0x11A8, 0x11AB, 0x11BA, 0x11EB, 0x11F0, 0x11F9, 0x1F59, 0x1F5B, 0x1F5D, 0x1FBE, 0x2126, 0x212E};
    uint32_t SPECIAL_LIST2[] = {0x3007};
    uint32_t SPECIAL_LIST3[] = {0};
    uint32_t SPECIAL_LIST4[] = {0x5BF, 0x5C4, 0x670, 0x93C, 0x94D, 0x9BC, 0x9BE, 0x9BF, 0x9D7, 0xA02, 0xA3C, 0xA3E, 0xA3F, 0xABC, 0xB3C, 0xBD7, 0xD57, 0xE31, 0xEB1, 0xF35, 0xF37, 0xF39, 0xF3E, 0xF3F, 0xF97, 0xFB9, 0x20E1, 0x3099, 0x309A};
    uint32_t SPECIAL_LIST5[] = {0xB7, 0x2D0, 0x2D1, 0x387, 0x640, 0xE46, 0xEC6, 0x3005};
}