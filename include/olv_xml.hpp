#pragma once

#include "olv_functions.hpp"
#include <new>

namespace nn::olv::xml
{

    uint8_t CountSpecialCharacters(uint8_t *chars, uint32_t length);
    bool ValidateCharacters(uint8_t *chars, uint32_t length);
    void ReadUTFCharacter(uint32_t *outVal, uint8_t *chars, uint32_t length);
    bool IsBlankCharacter(uint32_t val);

    struct CharacterRange
    {
        uint32_t minVal;
        uint32_t maxVal;

        CharacterRange(uint32_t a, uint32_t b) : minVal(a), maxVal(b) {}
    };

    extern CharacterRange VALIDATE_LIST_RANGE1[];
    extern CharacterRange VALIDATE_LIST_RANGE2[];
    extern CharacterRange VALIDATE_LIST_RANGE3[];
    extern CharacterRange VALIDATE_LIST_RANGE4[];
    extern CharacterRange VALIDATE_LIST_RANGE5[];

    extern uint32_t SPECIAL_LIST1[];
    extern uint32_t SPECIAL_LIST2[];
    extern uint32_t SPECIAL_LIST3[];
    extern uint32_t SPECIAL_LIST4[];
    extern uint32_t SPECIAL_LIST5[];

    enum XMLError : uint32_t
    {
        Error_None,
        Error_NodeFull,
        Error_OverMaxRecursive,
        Error_CharEncoding,
        Error_CalcElementNum,
        Error_NotExistRootElement,
        Error_CharBeforeRootElement,
        Error_CharAfterRootElement,
        Error_IllegalEndText,
        Error_StartTag,
        Error_EndTag,
        Error_MismatchStartEndTag,
        Error_NotName,
        Error_NotCharData,
        Error_Reference,
        Error_Content,
        Error_UnsupportContentFormat,
        Error_EntityRef,
        Error_CharRef,
        Error_CharRefDec,
        Error_CharRefHex,
        Error_MAX,
    };

    struct XMLNode
    {
        char *tagName;
        char *tagContent;
        uint32_t tagNameLen;
        uint32_t tagContentLen;
        XMLNode *unk_10;
        XMLNode *unk_14;

        bool CompareNodeTagName(const char *name, uint32_t length);
    };

    struct XMLNodeStack
    {
        struct Entry
        {
            uint32_t unk_00;
        };

        uint32_t numDepth;
        uint32_t currentDepth;
        XMLNode **entries;
    };

    struct ParsingObject
    {
        char *rawData;
        uint32_t rawDataSize;
        uint32_t currentReadLength;

        ParsingObject(char *rawData, uint32_t rawDataSize)
        {
            this->rawData = rawData;
            this->rawDataSize = rawDataSize;
            this->currentReadLength = 0;
        }

        bool ReadLengthOverflow()
        {
            return this->currentReadLength >= this->rawDataSize;
        }

        uint32_t GetRemainingLength()
        {
            return this->rawDataSize - this->currentReadLength;
        }

        char *GetCurrentReadPointer()
        {
            return &this->rawData[this->currentReadLength];
        }

        bool HasInvalidCharacters()
        {
            if (!this->ReadLengthOverflow())
            {
                uint32_t remainingLength = this->GetRemainingLength();
                char *currentPtr = this->GetCurrentReadPointer();
                return !ValidateCharacters((uint8_t *)currentPtr, remainingLength);
            }
            else
            {
                return false;
            }
        }

        uint8_t CountSpecialCharacters()
        {
            uint32_t remainingLength = this->GetRemainingLength();
            char *currentPtr = this->GetCurrentReadPointer();
            return nn::olv::xml::CountSpecialCharacters((uint8_t *)currentPtr, remainingLength);
        }

        void SkipSpecialCharacters()
        {
            this->currentReadLength += this->CountSpecialCharacters();
        }

        uint32_t ReadCharacter()
        {
            uint32_t remainingLength = this->GetRemainingLength();
            char *currentPtr = this->GetCurrentReadPointer();
            uint32_t val;
            nn::olv::xml::ReadUTFCharacter(&val, (uint8_t *)currentPtr, remainingLength);
            return val;
        }

        void SkipBlankCharacters()
        {
            while (true)
            {
                if (this->ReadLengthOverflow())
                    break;

                uint32_t val = this->ReadCharacter();
                if (!IsBlankCharacter(val))
                    break;

                this->SkipSpecialCharacters();
            }
        }
    };

    struct UnkClass
    {
        uint32_t unk_00 = 0;
        uint32_t unk_04 = 0;
        uint32_t currentReadLength = 0;

        UnkClass() = default;

        void Reset()
        {
            this->unk_00 = 0;
            this->currentReadLength = 0;
        }
    };

    class Parser
    {
    public:
        uint32_t nodeDepth = 0;
        uint32_t currentDepth = 0;
        uint32_t maxDepthParsed = 0;
        XMLError error = XMLError::Error_None;
        uint32_t unk_10 = 0;
        uint32_t readLength = 0;
        XMLNode *rootNodes = nullptr;
        uint32_t numTags = 0;
        uint32_t currentNumTag = 0;
        XMLNodeStack *nodeStack = nullptr;
        UnkClass unkClass;

        Parser()
        {
            this->Reset();
        }

        void Reset()
        {
            this->currentDepth = 0;
            this->maxDepthParsed = 0;
            this->error = XMLError::Error_None;
            this->readLength = 0;
            this->currentNumTag = 0;
            this->unkClass.Reset();

            if (this->rootNodes)
            {
                Internal_Free(this->rootNodes, HEAP_XML);
                this->rootNodes = nullptr;
            }

            this->numTags = 0;
            if (this->nodeStack)
            {
                this->ResetTree(this->nodeStack);
                this->nodeStack = nullptr;
            }
            this->nodeDepth = 0;
        }

        void ResetTree(XMLNodeStack *ptr)
        {
            if (ptr)
            {
                XMLNode **v3 = ptr->entries;
                if (v3)
                {
                    Internal_Free(v3, HEAP_XML);
                    ptr->entries = nullptr;
                }
                Internal_Free(ptr, HEAP_XML);
            }
        }

        XMLNode *GetLatestNodeByDepth()
        {
            return this->nodeStack->entries[this->nodeStack->currentDepth - 1];
        }

        void SetError(XMLError error, uint32_t readLength)
        {
            this->error = error;
            this->readLength = readLength;
            this->unkClass.unk_00 = error;
            this->unkClass.currentReadLength = readLength;
        }

        XMLNodeStack *InitNodeStack(int numDepth)
        {
            XMLNodeStack *nodeStack = (XMLNodeStack *)Internal_Alloc(sizeof(XMLNodeStack), HEAP_XML);
            if (!nodeStack)
                return 0;
            nodeStack->numDepth = numDepth;
            nodeStack->currentDepth = 0;
            nodeStack->entries = (XMLNode **)Internal_Alloc(sizeof(XMLNodeStack::Entry) * numDepth, HEAP_XML);
            return nodeStack;
        }

        bool EnsureRecursiveCoherency()
        {
            if (this->currentDepth >= this->nodeDepth)
                return false;

            this->currentDepth++;
            if (this->currentDepth > this->maxDepthParsed)
                this->maxDepthParsed = this->currentDepth;

            return 1;
        }

        bool CheckCantContinueError()
        {
            if (this->error != Error_OverMaxRecursive && this->error != Error_NodeFull)
                return false;

            return true;
        }

        bool IsNodeStackRootDepth()
        {
            return this->nodeStack->currentDepth == 0;
        }

        bool HasError()
        {
            return this->error != Error_None;
        }

        uint32_t GetReadLength()
        {
            return this->readLength;
        }

        XMLError GetError()
        {
            return this->error;
        }

        void Destroy(HeapId heapId);

        void Parse(char *rawData, uint32_t rawDataSize);
        bool ParseImpl(char *rawData, uint32_t rawDataSize);

        bool ParseNodesRecursive(ParsingObject &object);

        /* =========================================================== */

        bool WriteCurrentTagName(char *name, uint32_t length);

    public:
        bool ReadOpeningTag(ParsingObject &object);

    private:
        bool ReadOpeningTag_FindOpening(ParsingObject &object);
        bool ReadOpeningTag_ReadName(ParsingObject &object);
        bool ReadOpeningTag_ReadNameImpl(ParsingObject &object);

        /* =========================================================== */

    public:
        bool ReadTagData(ParsingObject &object);

    private:
        bool ReadTagDataImpl(ParsingObject &object);
        bool FindEndTagData(ParsingObject &object);
        bool CheckIsCharData(ParsingObject &object, uint32_t length);

        /* =========================================================== */

        bool VerifyXMLReferences(ParsingObject &object);

        bool VerifyXMLEntities(ParsingObject &object);
        bool IsXMLEntity(ParsingObject &object);
        bool IsXMLEntityClosure(ParsingObject &object, XMLError errorToThrow);

        bool VerifyXMLReferencesImpl(ParsingObject &object);

        bool VerifyXMLReferencesImplDec(ParsingObject &object);
        bool IsXMLDecimalReference(ParsingObject &object);
        bool ValidateXMLDecimalReference(ParsingObject &object);

        bool VerifyXMLReferencesImplHex(ParsingObject &object);
        bool IsXMLHexadecimalReference(ParsingObject &object);
        bool ValidateXMLHexadecimalReference(ParsingObject &object);

        /* =========================================================== */

    public:
        bool ReadClosingTag(ParsingObject &object);

    private:
        bool IsClosingTag(ParsingObject &object);
        bool ReadClosingTagName(ParsingObject &object);
        void WriteDepthToTree();
        /* =========================================================== */

    private:
        bool FindClosing(ParsingObject &object, XMLError error);

        /* =========================================================== */

    public:
        bool EnsureUTF8(char *rawData, uint32_t rawDataSize);
        int NodeCheck(char *rawData, uint32_t rawDataSize, uint32_t *outUnk);
    };

    typedef int (*XMLParseCallback)(int unk, XMLNode *node, void *userptr);

    int ParseXML(const char *buffer, uint32_t bufferSize, XMLParseCallback callback, void *userptr);

    struct XMLParsingStruct
    {
        Parser *instance = nullptr;
        XMLNode *currentNode = 0;
        char *tagName = 0;
        char *tagContent = 0;
        uint32_t tagNameLen = 0;
        uint32_t tagContentLen = 0;
        XMLNode *nodeList[32] = {0};
        uint32_t unk_final = 0;

        void Reset()
        {
            for (int i = 0; i < 32; ++i)
                this->nodeList[i] = nullptr;

            this->unk_final = 0;
        }

        void PushNextNode(XMLNode *node)
        {
            if (this->unk_final < 0x20)
            {
                this->nodeList[this->unk_final] = node;
                this->unk_final++;
            }
        }

        void PopNextNode()
        {

            if (this->unk_final == 0)
                return;

            this->nodeList[this->unk_final] = nullptr;
            this->unk_final--;
        }
    };

    extern XMLParsingStruct sParsingStruct;

    char *GetTagName();
    uint32_t GetTagNameLength();

    char *GetTagContent();
    uint32_t GetTagContentLength();

    bool CompareAgainstTagName(char *tagName1, const char *tagName2);

    XMLNode *FindChildByName(XMLNode *node, const char *name, uint32_t length);

    int Internal_AssignCurrentNode(XMLNode *node);

    bool SelectNodeByName(const char *nodeName);

}